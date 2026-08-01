import { useCallback, useEffect, useState } from "react";
import { ref as dbRef, onValue } from "firebase/database";
import { ref as stRef, listAll, getDownloadURL, getMetadata } from "firebase/storage";
import { db, storage } from "../../api.js";

export function usePetPhotos(id) {
  const [photos, setPhotos] = useState(null);

  const refresh = useCallback(async () => {
    const res = await listAll(stRef(storage, `enroll/${id}`));
    const items = await Promise.all(res.items.map(async (it) => {
      const md = await getMetadata(it).catch(() => null);
      const raw = md?.customMetadata?.pets_detected;
      return {
        path: it.fullPath, name: it.name,
        url: await getDownloadURL(it).catch(() => null),
        petsDetected: raw === undefined || raw === null ? null : Number(raw),
        timeCreated: md?.timeCreated ? Date.parse(md.timeCreated) : 0,
      };
    }));
    items.sort((a, b) => b.timeCreated - a.timeCreated || b.name.localeCompare(a.name));
    setPhotos(items);
  }, [id]);

  useEffect(() => {
    setPhotos(null);
    refresh().catch(() => setPhotos([]));
  }, [refresh]);

  return { photos, refresh };
}

export function coverOf(photos, thumbnailPhoto = null) {
  if (!photos || photos.length === 0) return null;
  if (thumbnailPhoto) {
    const selected = photos.find((p) => p.path === thumbnailPhoto && p.url);
    if (selected) return selected;
  }
  return photos.find((p) => p.petsDetected === 1 && p.url) || photos.find((p) => p.url) || null;
}

export async function toUprightJpeg(file) {
  const bmp = await createImageBitmap(file, { imageOrientation: "from-image" });
  const canvas = document.createElement("canvas");
  canvas.width = bmp.width; canvas.height = bmp.height;
  canvas.getContext("2d").drawImage(bmp, 0, 0);
  bmp.close();
  return new Promise((res) => canvas.toBlob((b) => res(b), "image/jpeg", 0.92));
}

export function waitForCommand(key, timeoutMs = 120_000) {
  return new Promise((resolve) => {
    let settled = false;
    const finish = (result) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      unsub();
      resolve(result);
    };
    const timer = setTimeout(() => finish(false), timeoutMs);
    const unsub = onValue(dbRef(db, `commands/${key}`), (snap) => {
      const cmd = snap.val();
      if (cmd && cmd.status && cmd.status !== "pending") finish(true);
    });
  });
}
