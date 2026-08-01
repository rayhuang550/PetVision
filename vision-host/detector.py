from dataclasses import dataclass
from functools import lru_cache

from PIL import Image

import config


@dataclass
class Detection:
    cls: str
    confidence: float
    box: tuple[int, int, int, int]


class Detector:
    def __init__(self, model_name: str = config.DET_MODEL):
        from ultralytics import YOLO
        self.model = YOLO(model_name)

    def detect(self, image: Image.Image) -> list[Detection]:
        res = self.model.predict(image, conf=config.DET_CONF, verbose=False)[0]
        names = res.names
        out: list[Detection] = []
        for b in res.boxes:
            name = names[int(b.cls[0])]
            if name not in config.DET_CLASSES:
                continue
            x1, y1, x2, y2 = (int(v) for v in b.xyxy[0].tolist())
            out.append(Detection(cls=name, confidence=float(b.conf[0]), box=(x1, y1, x2, y2)))
        return class_agnostic_nms(out)


def iou(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> float:
    ax1, ay1, ax2, ay2 = a
    bx1, by1, bx2, by2 = b
    ix1, iy1 = max(ax1, bx1), max(ay1, by1)
    ix2, iy2 = min(ax2, bx2), min(ay2, by2)
    iw, ih = max(0, ix2 - ix1), max(0, iy2 - iy1)
    inter = iw * ih
    if inter == 0:
        return 0.0
    area_a = max(0, ax2 - ax1) * max(0, ay2 - ay1)
    area_b = max(0, bx2 - bx1) * max(0, by2 - by1)
    union = area_a + area_b - inter
    return inter / union if union > 0 else 0.0


def class_agnostic_nms(detections: list[Detection],
                       iou_threshold: float = config.DET_NMS_IOU) -> list[Detection]:
    kept: list[Detection] = []
    for det in sorted(detections, key=lambda d: d.confidence, reverse=True):
        if all(iou(det.box, k.box) <= iou_threshold for k in kept):
            kept.append(det)
    return kept


def margin_box(box: tuple[int, int, int, int], size: tuple[int, int],
               margin: float = config.CROP_MARGIN) -> tuple[int, int, int, int]:
    x1, y1, x2, y2 = box
    w, h = x2 - x1, y2 - y1
    dx, dy = int(w * margin), int(h * margin)
    iw, ih = size
    return (max(0, x1 - dx), max(0, y1 - dy), min(iw, x2 + dx), min(ih, y2 + dy))


def crop_with_margin(image: Image.Image, box: tuple[int, int, int, int],
                     margin: float = config.CROP_MARGIN) -> Image.Image:
    return image.crop(margin_box(box, image.size, margin))


def norm_box(box: tuple[int, int, int, int], size: tuple[int, int]) -> list[float]:
    x1, y1, x2, y2 = box
    iw, ih = size
    return [x1 / iw, y1 / ih, (x2 - x1) / iw, (y2 - y1) / ih]


@lru_cache(maxsize=1)
def get_detector() -> Detector:
    return Detector()
