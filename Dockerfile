# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v5.0 (FINAL & ROBUST)
# =================================================================
# Build argümanı ile temel imajı dinamik olarak seç
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE=python:${PYTHON_VERSION}-slim-bookworm

# --- Aşama 1: Builder ---
# Temel olarak her zaman resmi Python imajını kullanıyoruz.
FROM python:${PYTHON_VERSION}-slim-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Gerekli sistem bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl \
    ffmpeg libavformat-dev libavcodec-dev libavdevice-dev \
    libavutil-dev libavfilter-dev libswscale-dev libswresample-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Poetry'yi kur
RUN pip install --no-cache-dir --upgrade pip poetry
ENV POETRY_VIRTUALENVS_IN_PROJECT=true

# Bağımlılıkları kur
COPY poetry.lock pyproject.toml ./
# POETRY_EXTRAS argümanı CI tarafından sağlanacak (--extras cuda veya boş)
ARG POETRY_EXTRAS=""
RUN poetry install --without dev --no-root ${POETRY_EXTRAS}

# --- Aşama 2: Final Image ---
# Temel imajı yine build argümanından alıyoruz.
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH" \
    HF_HOME="/app/model-cache" \
    LD_LIBRARY_PATH="/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}"

# Sadece runtime bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg netcat-openbsd curl ca-certificates \
    # GPU imajı için CUDA runtime kütüphanelerini ekliyoruz (CPU imajında bu komut zararsızdır)
    && if [ -d /usr/local/cuda ]; then apt-get install -y --no-install-recommends cuda-compat-12-1; fi \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

COPY --from=builder --chown=appuser:appgroup /app/.venv ./.venv
COPY --chown=appuser:appgroup ./app ./app

RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]