import { usePetPhotos, coverOf } from "./usePetPhotos.js";

export default function PetPhoto({ id, name, color, thumbnailPhoto }) {
  const { photos } = usePetPhotos(id);
  const cover = coverOf(photos, thumbnailPhoto);

  if (cover) return <img src={cover.url} alt={name} loading="lazy" />;
  return (
    <div className="disc-wrap">
      <span className="disc" style={{ background: color }}>{(name || "?").slice(0, 1).toUpperCase()}</span>
    </div>
  );
}
