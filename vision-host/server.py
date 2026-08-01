import base64
import io
import os
import time

import numpy as np
from fastapi import FastAPI, File, Form, HTTPException, Query, Request, UploadFile
from fastapi.responses import FileResponse, JSONResponse, Response
from PIL import Image, ImageOps

import config
import gallery
from detector import get_detector, crop_with_margin, margin_box, norm_box
from embedder import get_embedder
from request_history import HISTORY

app = FastAPI(title="PetVision Vision Host", docs_url=None, redoc_url=None, openapi_url=None)

_STATE: dict = {"embeddings": None, "labels": None}


def _reload_gallery() -> None:
    if os.path.exists(config.GALLERY_PATH):
        _STATE["embeddings"], _STATE["labels"] = gallery.load()
    else:
        _STATE["embeddings"], _STATE["labels"] = None, None


@app.on_event("startup")
def _startup() -> None:
    get_embedder()
    get_detector()
    _reload_gallery()


def _begin_request(endpoint: str, request: Request, file: UploadFile | None,
                   *, enrollment_name: str | None = None) -> str:
    return HISTORY.begin(
        endpoint=endpoint,
        query=dict(request.query_params),
        client=request.client.host if request.client else None,
        filename=file.filename if file is not None else None,
        content_type=(file.content_type if file is not None else request.headers.get("content-type")),
        request_format="multipart" if file is not None else "raw",
        enrollment_name=enrollment_name,
    )


async def _read_image_upright(request: Request, file: UploadFile | None,
                              request_id: str) -> Image.Image:
    raw = await file.read() if file is not None else await request.body()
    supplied_type = file.content_type if file is not None else request.headers.get("content-type")
    HISTORY.attach_source(
        request_id, raw, supplied_type or "application/octet-stream",
        original_size=None, upright_size=None, exif_orientation=None)
    with Image.open(io.BytesIO(raw)) as opened:
        original_size = list(opened.size)
        orientation = opened.getexif().get(274)
        media_type = Image.MIME.get(opened.format or "") or supplied_type
        img = ImageOps.exif_transpose(opened).convert("RGB")
    HISTORY.attach_source(
        request_id, raw, media_type or "application/octet-stream",
        original_size=original_size, upright_size=list(img.size), exif_orientation=orientation)
    return img


def _jpeg_bytes(img: Image.Image) -> bytes:
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=92)
    return buf.getvalue()


def _b64_jpeg(img: Image.Image) -> str:
    return base64.b64encode(_jpeg_bytes(img)).decode("ascii")


def _detect_and_crop(img: Image.Image):
    dets = get_detector().detect(img)
    return [(d, crop_with_margin(img, d.box)) for d in dets]


def _gallery_snapshot():
    if _STATE["embeddings"] is None:
        return None
    return _STATE["embeddings"], _STATE["labels"]


def _match_detail(vector: np.ndarray, snapshot, *, role: str | None = None) -> dict:
    if snapshot is None:
        detail = {"available": False, "reason": "no pets enrolled"}
        if role is not None:
            detail["role"] = role
        return detail
    match = gallery.match(vector, snapshot[0], snapshot[1])
    ranked = sorted(match.scores.items(), key=lambda item: item[1], reverse=True)
    return {
        "available": True,
        **({"role": role} if role is not None else {}),
        "name": match.name,
        "best_pet": ranked[0][0] if ranked else None,
        "known": match.known,
        "confidence": round(match.confidence, 3),
        "similarity": round(match.similarity, 3),
        "threshold": config.THRESHOLD,
        "scores": {name: round(score, 3) for name, score in ranked},
    }


def _detection_detail(detection, crop: Image.Image, image: Image.Image,
                      vector: np.ndarray, snapshot, *, role: str | None = None) -> dict:
    raw_box = norm_box(detection.box, image.size)
    crop_box = norm_box(margin_box(detection.box, image.size), image.size)
    return {
        "class": detection.cls,
        "yolo_confidence": round(detection.confidence, 3),
        "raw_box": raw_box,
        "crop_box": crop_box,
        "raw_box_pixels": list(detection.box),
        "crop_box_pixels": list(margin_box(detection.box, image.size)),
        "crop_size": list(crop.size),
        "embedding_dim": int(vector.shape[0]),
        "match": _match_detail(vector, snapshot, role=role),
    }


def _elapsed_ms(started: float) -> float:
    return round((time.perf_counter() - started) * 1000, 1)


def _error_text(exc: Exception) -> str:
    message = str(exc).strip()
    return f"{type(exc).__name__}: {message}" if message else type(exc).__name__


@app.post("/embed")
async def embed(request: Request, image: UploadFile | None = File(default=None),
                crops: int = 0):
    t0 = time.perf_counter()
    request_id = _begin_request("/embed", request, image)
    try:
        img = await _read_image_upright(request, image, request_id)
        emb = get_embedder()
        snapshot = _gallery_snapshot()
        detections = []
        diagnostic_detections = []
        diagnostic_crops = []
        for d, crop in _detect_and_crop(img):
            v = emb.embed(crop)
            crop_bytes = _jpeg_bytes(crop)
            entry = {"class": d.cls, "confidence": round(d.confidence, 3),
                     "box": norm_box(margin_box(d.box, img.size), img.size),
                     "embedding": v.tolist(), "dim": int(v.shape[0])}
            if crops:
                entry["crop_image"] = base64.b64encode(crop_bytes).decode("ascii")
            detections.append(entry)
            diagnostic_detections.append(_detection_detail(d, crop, img, v, snapshot))
            diagnostic_crops.append((crop_bytes, "image/jpeg"))

        response_elapsed = _elapsed_ms(t0)
        resp = {"request_id": request_id, "pet_count": len(detections),
                "detections": detections, "ms": response_elapsed}
        whole_frame_diagnostic = None
        if crops and len(detections) != 1:
            wv = emb.embed(img)
            resp["whole_frame"] = {"embedding": wv.tolist(), "dim": int(wv.shape[0]),
                                   "crop_image": _b64_jpeg(img)}
            whole_frame_diagnostic = {
                "embedding_dim": int(wv.shape[0]),
                "match": _match_detail(wv, snapshot),
            }
        HISTORY.complete(
            request_id, http_status=200, ms=_elapsed_ms(t0), pet_count=len(detections),
            detections=diagnostic_detections, crops=diagnostic_crops,
            whole_frame=whole_frame_diagnostic)
        return resp
    except Exception as exc:
        HISTORY.fail(request_id, http_status=500, ms=_elapsed_ms(t0), error=_error_text(exc))
        raise


@app.post("/identify")
async def identify(request: Request, image: UploadFile | None = File(default=None)):
    t0 = time.perf_counter()
    request_id = _begin_request("/identify", request, image)
    try:
        img = await _read_image_upright(request, image, request_id)
        snapshot = _gallery_snapshot()
        if snapshot is None:
            elapsed = _elapsed_ms(t0)
            HISTORY.complete(
                request_id, http_status=409, ms=elapsed, pet_count=None,
                detections=[], crops=[], error="no pets enrolled")
            return JSONResponse({"error": "no pets enrolled"}, status_code=409)
        emb = get_embedder()
        results = []
        diagnostic_detections = []
        diagnostic_crops = []
        for d, crop in _detect_and_crop(img):
            vector = emb.embed(crop)
            m = gallery.match(vector, snapshot[0], snapshot[1])
            results.append({
                "class": d.cls, "box": norm_box(margin_box(d.box, img.size), img.size),
                "name": m.name, "known": m.known,
                "confidence": round(m.confidence, 3), "similarity": round(m.similarity, 3),
                "scores": {k: round(v, 3) for k, v in m.scores.items()},
            })
            diagnostic_detections.append(_detection_detail(
                d, crop, img, vector, snapshot, role="/identify result"))
            diagnostic_crops.append((_jpeg_bytes(crop), "image/jpeg"))
        elapsed = _elapsed_ms(t0)
        HISTORY.complete(
            request_id, http_status=200, ms=elapsed, pet_count=len(results),
            detections=diagnostic_detections, crops=diagnostic_crops)
        return {"pet_count": len(results), "results": results, "ms": elapsed}
    except Exception as exc:
        HISTORY.fail(request_id, http_status=500, ms=_elapsed_ms(t0), error=_error_text(exc))
        raise


@app.post("/enroll")
async def enroll(request: Request, name: str = Form(...), image: UploadFile | None = File(default=None)):
    t0 = time.perf_counter()
    request_id = _begin_request("/enroll", request, image, enrollment_name=name)
    try:
        img = await _read_image_upright(request, image, request_id)
        if not gallery.valid_pet_name(name):
            elapsed = _elapsed_ms(t0)
            HISTORY.complete(
                request_id, http_status=400, ms=elapsed, pet_count=None,
                detections=[], crops=[], error="invalid pet name")
            return JSONResponse({"error": "invalid pet name"}, status_code=400)

        snapshot = _gallery_snapshot()
        crops = _detect_and_crop(img)
        diagnostic_detections = []
        diagnostic_crops = []
        emb = get_embedder()
        for detection, crop in crops:
            vector = emb.embed(crop)
            diagnostic_detections.append(_detection_detail(
                detection, crop, img, vector, snapshot,
                role="diagnostic comparison against the pre-enroll Gallery"))
            diagnostic_crops.append((_jpeg_bytes(crop), "image/jpeg"))

        to_store = crops[0][1] if len(crops) == 1 else img
        pet_dir = os.path.join(config.DATA_DIR, name)
        os.makedirs(pet_dir, exist_ok=True)
        to_store.save(os.path.join(pet_dir, f"{int(time.time() * 1000)}.jpg"), quality=92)
        counts = gallery.build()
        _reload_gallery()
        elapsed = _elapsed_ms(t0)
        HISTORY.complete(
            request_id, http_status=200, ms=elapsed, pet_count=len(crops),
            detections=diagnostic_detections, crops=diagnostic_crops)
        return {"enrolled": name, "pets_detected": len(crops), "counts": counts,
                "boxes": [norm_box(margin_box(d.box, img.size), img.size) for d, _ in crops]}
    except Exception as exc:
        HISTORY.fail(request_id, http_status=500, ms=_elapsed_ms(t0), error=_error_text(exc))
        raise


@app.get("/gallery")
def gallery_summary():
    if _STATE["labels"] is None:
        return {"pets": {}}
    labels, counts = np.unique(_STATE["labels"], return_counts=True)
    return {"pets": {str(l): int(c) for l, c in zip(labels, counts)}}


@app.get("/diagnostics/requests")
def diagnostic_requests(limit: int = Query(default=200, ge=1, le=1_000),
                        offset: int = Query(default=0, ge=0),
                        endpoint: str | None = None, status: str | None = None):
    return HISTORY.list(limit=limit, offset=offset, endpoint=endpoint, status=status)


@app.get("/diagnostics/requests/{request_id}")
def diagnostic_request(request_id: str):
    detail = HISTORY.detail(request_id)
    if detail is None:
        raise HTTPException(status_code=404, detail="request not found")
    return detail


@app.get("/diagnostics/requests/{request_id}/image")
def diagnostic_request_image(request_id: str):
    blob = HISTORY.image(request_id)
    if blob is None:
        raise HTTPException(status_code=404, detail="request image not found")
    return Response(content=blob[0], media_type=blob[1], headers={"Cache-Control": "no-store"})


@app.get("/diagnostics/requests/{request_id}/detections/{index}/crop")
def diagnostic_detection_crop(request_id: str, index: int):
    blob = HISTORY.crop(request_id, index)
    if blob is None:
        raise HTTPException(status_code=404, detail="Detection crop not found")
    return Response(content=blob[0], media_type=blob[1], headers={"Cache-Control": "no-store"})


@app.delete("/diagnostics/requests")
def clear_diagnostic_requests():
    return {"cleared": HISTORY.clear()}


@app.get("/assets/hanken-grotesk.woff2", include_in_schema=False)
def ui_font():
    return FileResponse(
        os.path.join(config.ROOT, "web", "assets", "hanken-grotesk.woff2"),
        media_type="font/woff2",
        headers={"Cache-Control": "public, max-age=86400"},
    )


@app.get("/")
def index():
    return FileResponse(os.path.join(config.ROOT, "web", "index.html"))
