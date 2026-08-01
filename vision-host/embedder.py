from functools import lru_cache

import numpy as np
import torch
from PIL import Image
from torchvision import transforms
from transformers import AutoModel

import config

_IMAGENET_MEAN = [0.485, 0.456, 0.406]
_IMAGENET_STD = [0.229, 0.224, 0.225]


def _native_size(w: int, h: int, cap: int = config.IMAGE_SIZE,
                 min_side: int = config.MIN_SIDE, patch: int = config.PATCH) -> tuple[int, int]:
    longer = max(w, h)
    scale = cap / longer if longer > cap else (min_side / longer if longer < min_side else 1.0)
    rw, rh = round(w * scale), round(h * scale)
    snap = lambda v: max(patch, int(round(v / patch)) * patch)
    return snap(rw), snap(rh)


class Embedder:
    def __init__(self, model_name: str = config.MODEL):
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.model = AutoModel.from_pretrained(model_name).to(self.device).eval()
        self.normalize = transforms.Compose([
            transforms.ToTensor(),
            transforms.Normalize(_IMAGENET_MEAN, _IMAGENET_STD),
        ])

    def _tensor(self, image: Image.Image) -> torch.Tensor:
        img = image.convert("RGB")
        w, h = img.size
        img = img.resize(_native_size(w, h), Image.BICUBIC)
        return self.normalize(img)

    @torch.inference_mode()
    def embed(self, image: Image.Image) -> np.ndarray:
        x = self._tensor(image).unsqueeze(0).to(self.device)
        out = self.model(x, interpolate_pos_encoding=True).last_hidden_state
        emb = torch.cat([out[:, 0], out[:, 1:].mean(dim=1)], dim=-1)
        emb = torch.nn.functional.normalize(emb, dim=-1)
        return emb.float().cpu().numpy()[0]

    def embed_batch(self, images: list[Image.Image]) -> np.ndarray:
        return np.stack([self.embed(im) for im in images])


@lru_cache(maxsize=1)
def get_embedder() -> Embedder:
    return Embedder()
