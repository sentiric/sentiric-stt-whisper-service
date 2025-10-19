# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v8.4 (SIMPLIFIED)
# =================================================================
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE=python:${PYTHON_VERSION}-slim-bookworm

FROM ${BASE_IMAGE} AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Temel bağımlılıklar
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl \
    ffmpeg libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev \
    libsndfile1 portaudio19-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# PyTorch'u öncelikli kur
RUN pip install --no-cache-dir --upgrade pip
RUN pip install --no-cache-dir torch==2.3.0 --index-url https://download.pytorch.org/whl/cpu

# Diğer bağımlılıklar
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH" \
    HF_HOME="/app/model-cache"

# Runtime bağımlılıklar
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg libsndfile1 curl ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# CUDA için (GPU image)
RUN if command -v nvidia-smi > /dev/null 2>&1; then \
        apt-get update && apt-get install -y --no-install-recommends \
        cuda-cudart-12-1 cuda-nvtx-12-1 \
        && apt-get clean && rm -rf /var/lib/apt/lists/*; \
    fi

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin
COPY --chown=appuser:appgroup ./app ./app

RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]