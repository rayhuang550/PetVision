import os

MODEL = os.environ.get("PETVISION_MODEL", "facebook/dinov2-base")

IMAGE_SIZE = int(os.environ.get("PETVISION_IMAGE_SIZE", "518"))

MIN_SIDE = int(os.environ.get("PETVISION_MIN_SIDE", "224"))

PATCH = 14

DET_MODEL = os.environ.get("PETVISION_DET_MODEL", "yolo26m.pt")
DEFAULT_DET_CLASSES = "bird,cat,dog,horse"
DET_CLASSES = set(
    pet_class.strip()
    for pet_class in os.environ.get("PETVISION_DET_CLASSES", DEFAULT_DET_CLASSES).split(",")
    if pet_class.strip()
)
DET_CONF = float(os.environ.get("PETVISION_DET_CONF", "0.25"))
DET_NMS_IOU = float(os.environ.get("PETVISION_DET_NMS_IOU", "0.55"))
CROP_MARGIN = float(os.environ.get("PETVISION_CROP_MARGIN", "0.0"))

THRESHOLD = float(os.environ.get("PETVISION_THRESHOLD", "0.20"))

CONF_FULL_MARGIN = float(os.environ.get("PETVISION_CONF_FULL_MARGIN", "0.15"))

ROOT = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.environ.get("PETVISION_DATA_DIR", os.path.join(ROOT, "data"))
GALLERY_PATH = os.environ.get("PETVISION_GALLERY", os.path.join(ROOT, "gallery.npz"))
