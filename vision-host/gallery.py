import glob
import os
from dataclasses import dataclass

import numpy as np
from PIL import Image, ImageOps

import config

_IMG_EXTS = ("*.jpg", "*.jpeg", "*.png", "*.webp", "*.bmp")


def valid_pet_name(name: str) -> bool:
    if not isinstance(name, str) or not name or name != name.strip() or name.startswith("."):
        return False
    return os.path.basename(name) == name and "/" not in name and "\\" not in name


def _load_upright(path: str) -> Image.Image:
    with Image.open(path) as im:
        return ImageOps.exif_transpose(im)


@dataclass
class Match:
    name: str | None
    known: bool
    confidence: float
    similarity: float
    scores: dict[str, float]


def build(data_dir: str = config.DATA_DIR, gallery_path: str = config.GALLERY_PATH) -> dict[str, int]:
    from embedder import get_embedder
    embedder = get_embedder()
    embeddings: list[np.ndarray] = []
    labels: list[str] = []
    counts: dict[str, int] = {}

    for pet in sorted(os.listdir(data_dir)):
        pet_dir = os.path.join(data_dir, pet)
        if not os.path.isdir(pet_dir):
            continue
        paths = sorted(p for ext in _IMG_EXTS for p in glob.glob(os.path.join(pet_dir, ext)))
        if not paths:
            continue
        imgs = [_load_upright(p) for p in paths]
        embs = embedder.embed_batch(imgs)
        embeddings.extend(embs)
        labels.extend([pet] * len(embs))
        counts[pet] = len(embs)

    if not embeddings:
        raise RuntimeError(f"No enrollment images found under {data_dir}/<pet>/*")

    np.savez(gallery_path, embeddings=np.asarray(embeddings, dtype=np.float32),
             labels=np.asarray(labels))
    return counts


def load(gallery_path: str = config.GALLERY_PATH) -> tuple[np.ndarray, np.ndarray]:
    data = np.load(gallery_path, allow_pickle=True)
    return data["embeddings"], data["labels"]


def match(emb: np.ndarray, embeddings: np.ndarray, labels: np.ndarray,
          threshold: float = config.THRESHOLD) -> Match:
    sims = embeddings @ emb
    scores: dict[str, float] = {}
    for label, sim in zip(labels, sims):
        label = str(label)
        if sim > scores.get(label, -1.0):
            scores[label] = float(sim)

    ranked = sorted(scores.items(), key=lambda kv: kv[1], reverse=True)
    top_name, top_sim = ranked[0]
    runner_up = ranked[1][1] if len(ranked) > 1 else 0.0
    confidence = float(np.clip((top_sim - runner_up) / config.CONF_FULL_MARGIN, 0.0, 1.0))
    known = top_sim >= threshold

    return Match(
        name=top_name if known else None,
        known=known,
        confidence=confidence,
        similarity=top_sim,
        scores=scores,
    )
