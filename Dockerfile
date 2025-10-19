# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v8.1 (FINAL & UNIFIED)
# =================================================================
# Build argümanları ile temel imajı dinamik olarak seç
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE=python:${PYTHON_VERSION}-slim-bookworm

# --- Aşama 1: Builder (Her zaman stabil Debian tabanlı) ---
FROM python:${PYTHON_VERSION}-slim-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Gerekli sistem bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl python3-venv \
    ffmpeg libavformat-dev libavcodec-dev libavdevice-dev \
    libavutil-dev libavfilter-dev libswscale-dev libswresample-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Poetry'yi kur
RUN pip install --no-cache-dir --upgrade pip poetry
ENV POETRY_VIRTUALENVS_IN_PROJECT=true

# Bağımlılıkları kur
COPY poetry.lock pyproject.toml ./
RUN poetry export -f requirements.txt --output requirements.txt --without-hashes
RUN pip install --no-cache-dir -r requirements.txt

# --- Aşama 2: Final Image (Dinamik Temel) ---
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH" \
    HF_HOME="/app/model-cache" \
    LD_LIBRARY_PATH="/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}"

# CUDA runtime kütüphanelerini ekle (GPU imajı için)
RUN if [ "$(uname -m)" = "x86_64" ] && [ -d "/usr/local/cuda" ]; then \
        apt-get update && apt-get install -y --no-install-recommends \
        cuda-cudart-12-1 \
        cuda-nvtx-12-1 \
        cuda-nvml-dev-12-1 \
        && apt-get clean && rm -rf /var/lib/apt/lists/*; \
    fi
    
# Sadece runtime bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg netcat-openbsd curl ca-certificates libsndfile1 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Builder'dan gelen Python bağımlılıklarını ve uygulama kodunu kopyala
COPY --from=builder /app/.venv ./.venv
COPY --chown=appuser:appgroup ./app ./app

RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]