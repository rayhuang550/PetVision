from __future__ import annotations

import copy
import os
import threading
import uuid
from collections import OrderedDict
from datetime import datetime, timezone


DEFAULT_MAX_RECORDS = 1_000
DEFAULT_MAX_BYTES = 256 * 1024 * 1024


def _positive_env(name: str, default: int) -> int:
    try:
        value = int(os.environ.get(name, str(default)))
    except ValueError:
        return default
    return value if value > 0 else default


class RequestHistory:

    def __init__(self, max_records: int | None = None, max_bytes: int | None = None):
        self.max_records = max_records or _positive_env(
            "PETVISION_REQUEST_HISTORY_MAX", DEFAULT_MAX_RECORDS)
        self.max_bytes = max_bytes or _positive_env(
            "PETVISION_REQUEST_HISTORY_BYTES", DEFAULT_MAX_BYTES)
        self._records: OrderedDict[str, dict] = OrderedDict()
        self._bytes = 0
        self._lock = threading.RLock()

    def begin(self, *, endpoint: str, query: dict[str, str], client: str | None,
              filename: str | None, content_type: str | None,
              request_format: str, enrollment_name: str | None = None) -> str:
        request_id = uuid.uuid4().hex
        started_at = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
            "+00:00", "Z")
        record = {
            "id": request_id,
            "started_at": started_at,
            "endpoint": endpoint,
            "method": "POST",
            "query": dict(query),
            "client": client,
            "filename": filename,
            "content_type": content_type,
            "request_format": request_format,
            "enrollment_name": enrollment_name,
            "status": "processing",
            "http_status": None,
            "ms": None,
            "input_bytes": None,
            "original_size": None,
            "upright_size": None,
            "exif_orientation": None,
            "pet_count": None,
            "detections": [],
            "whole_frame": None,
            "error": None,
            "_image": None,
            "_crops": [],
            "_blob_bytes": 0,
        }
        with self._lock:
            self._records[request_id] = record
            self._evict()
        return request_id

    def attach_source(self, request_id: str, image: bytes, media_type: str,
                      *, original_size: list[int] | None, upright_size: list[int] | None,
                      exif_orientation: int | None) -> None:
        with self._lock:
            record = self._records.get(request_id)
            if record is None:
                return
            old_size = record["_blob_bytes"]
            record["_image"] = (image, media_type)
            record["input_bytes"] = len(image)
            record["original_size"] = original_size
            record["upright_size"] = upright_size
            record["exif_orientation"] = exif_orientation
            record["_blob_bytes"] = len(image) + sum(len(blob) for blob, _ in record["_crops"])
            self._bytes += record["_blob_bytes"] - old_size
            self._evict()

    def complete(self, request_id: str, *, http_status: int, ms: float,
                 pet_count: int | None, detections: list[dict],
                 crops: list[tuple[bytes, str]], whole_frame: dict | None = None,
                 error: str | None = None) -> None:
        with self._lock:
            record = self._records.get(request_id)
            if record is None:
                return
            old_size = record["_blob_bytes"]
            record["status"] = "completed" if http_status < 400 else "failed"
            record["http_status"] = http_status
            record["ms"] = ms
            record["pet_count"] = pet_count
            record["detections"] = copy.deepcopy(detections)
            record["whole_frame"] = copy.deepcopy(whole_frame)
            record["error"] = error
            record["_crops"] = list(crops)
            image_size = len(record["_image"][0]) if record["_image"] else 0
            record["_blob_bytes"] = image_size + sum(len(blob) for blob, _ in crops)
            self._bytes += record["_blob_bytes"] - old_size
            self._evict()

    def fail(self, request_id: str, *, http_status: int, ms: float, error: str) -> None:
        self.complete(request_id, http_status=http_status, ms=ms, pet_count=None,
                      detections=[], crops=[], error=error)

    def list(self, *, limit: int = 200, offset: int = 0,
             endpoint: str | None = None, status: str | None = None) -> dict:
        with self._lock:
            records = list(reversed(self._records.values()))
            if endpoint:
                records = [r for r in records if r["endpoint"] == endpoint]
            if status:
                records = [r for r in records if r["status"] == status]
            total = len(records)
            page = records[offset:offset + limit]
            return {
                "requests": [self._summary(r) for r in page],
                "total": total,
                "retained": len(self._records),
                "retained_bytes": self._bytes,
                "max_records": self.max_records,
                "max_bytes": self.max_bytes,
            }

    def detail(self, request_id: str) -> dict | None:
        with self._lock:
            record = self._records.get(request_id)
            if record is None:
                return None
            detail = self._public(record)
            detail["image_url"] = (
                f"/diagnostics/requests/{request_id}/image" if record["_image"] else None)
            for index, detection in enumerate(detail["detections"]):
                detection["crop_url"] = (
                    f"/diagnostics/requests/{request_id}/detections/{index}/crop"
                    if index < len(record["_crops"]) else None)
            return detail

    def image(self, request_id: str) -> tuple[bytes, str] | None:
        with self._lock:
            record = self._records.get(request_id)
            return record["_image"] if record else None

    def crop(self, request_id: str, index: int) -> tuple[bytes, str] | None:
        with self._lock:
            record = self._records.get(request_id)
            if record is None or index < 0 or index >= len(record["_crops"]):
                return None
            return record["_crops"][index]

    def clear(self) -> int:
        with self._lock:
            count = len(self._records)
            self._records.clear()
            self._bytes = 0
            return count

    def _evict(self) -> None:
        while len(self._records) > self.max_records or self._bytes > self.max_bytes:
            candidates = [
                request_id for request_id, record in self._records.items()
                if record["status"] != "processing"
            ]
            newest = next(reversed(self._records), None)
            candidates = [request_id for request_id in candidates if request_id != newest]
            if not candidates:
                break
            evicted = self._records.pop(candidates[0])
            self._bytes -= evicted["_blob_bytes"]

    @staticmethod
    def _summary(record: dict) -> dict:
        top = None
        if record["detections"]:
            match = record["detections"][0].get("match") or {}
            if match.get("available"):
                top = match.get("name") or "Unknown"
        return {
            "id": record["id"],
            "started_at": record["started_at"],
            "endpoint": record["endpoint"],
            "status": record["status"],
            "http_status": record["http_status"],
            "ms": record["ms"],
            "pet_count": record["pet_count"],
            "top_result": top,
            "error": record["error"],
        }

    @staticmethod
    def _public(record: dict) -> dict:
        return copy.deepcopy({key: value for key, value in record.items() if not key.startswith("_")})


HISTORY = RequestHistory()
