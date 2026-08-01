# Vision Host

A local FastAPI service that uses YOLO to detect pets and DINOv2 to create embeddings for the
feeder.

## Setup and run

```bash
conda create -n petvision python=3.11 -y
conda activate petvision
pip install -r requirements.txt
uvicorn server:app --host 0.0.0.0 --port 8000
```

The first run downloads the model weights. Open http://localhost:8000 to view recent requests,
including images, detections, crops, scores, response times, and errors. The firmware uses
`POST /embed`.

Make sure the feeder can connect to the Vision Host on TCP port 8000.
