import { useEffect, useState } from "react";
import { initializeApp } from "firebase/app";
import { getAuth } from "firebase/auth";
import {
  getDatabase,
  get,
  onValue,
  push,
  ref,
  remove,
  serverTimestamp,
  set,
  update,
} from "firebase/database";
import { getStorage } from "firebase/storage";

const missing = [
  "VITE_PROJECT_ID",
  "VITE_APP_ID",
  "VITE_API_KEY",
  "VITE_AUTH_DOMAIN",
  "VITE_DATABASE_URL",
  "VITE_STORAGE_BUCKET",
]
  .filter((key) => !import.meta.env[key]);
if (missing.length) throw new Error(`Missing Firebase configuration: ${missing.join(", ")}`);

const firebase = initializeApp({
  projectId: import.meta.env.VITE_PROJECT_ID,
  appId: import.meta.env.VITE_APP_ID,
  apiKey: import.meta.env.VITE_API_KEY,
  authDomain: import.meta.env.VITE_AUTH_DOMAIN,
  databaseURL: import.meta.env.VITE_DATABASE_URL,
  storageBucket: import.meta.env.VITE_STORAGE_BUCKET,
});

export const db = getDatabase(firebase);
export const auth = getAuth(firebase);
export const storage = getStorage(firebase);

export function useValue(path) {
  const [value, setValue] = useState(undefined);
  useEffect(() => {
    if (!path) return undefined;
    setValue(undefined);
    return onValue(ref(db, path), (snapshot) => setValue(snapshot.val()));
  }, [path]);
  return value;
}

export const api = {
  async createPet(config) {
    const pet = push(ref(db, "pets"));
    await set(pet, config);
    return pet.key;
  },
  updatePet: (id, config) => update(ref(db, `pets/${id}`), config),
  setPetThumbnail: (id, path) => set(ref(db, `pets/${id}/thumbnail_photo`), path || null),
  removePet: (id) => remove(ref(db, `pets/${id}`)),
  setTimezone: (timezone) => set(ref(db, "config/timezone"), timezone),
  setHopperFood: (id, food) => set(ref(db, `hoppers/${id}/food`), food),
  async createFeed(pet) {
    const command = push(ref(db, "commands"));
    await set(command, { type: "feed", pet, ts: serverTimestamp(), status: "pending" });
    return command.key;
  },
  async createEnroll(pet, photos) {
    const command = push(ref(db, "commands"));
    await set(command, { type: "enroll", pet, photos, ts: serverTimestamp(), status: "pending" });
    return command.key;
  },
  async createUnenroll(pet, photos) {
    const command = push(ref(db, "commands"));
    await set(command, { type: "unenroll", pet, photos, ts: serverTimestamp(), status: "pending" });
    return command.key;
  },
  clearCommand: (key) => remove(ref(db, `commands/${key}`)),
  async clearTerminalCommands() {
    const commands = (await get(ref(db, "commands"))).val() || {};
    const removals = {};
    for (const [key, command] of Object.entries(commands)) {
      if (command.status !== "pending") removals[key] = null;
    }
    const keys = Object.keys(removals);
    if (keys.length) await update(ref(db, "commands"), removals);
    return keys.length;
  },
  clearFeederHold: () => remove(ref(db, "status/feeder/hold")),
  removeLogEntry: (pet, key) => remove(ref(db, `log/${pet}/${key}`)),
};
