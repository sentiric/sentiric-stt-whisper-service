### 📄 File: stt-whisper-service/Dockerfile (DÜZELTİLMİŞ)

ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE_TAG=${PYTHON_VERSION}-slim-bullseye

# ==================================
#      Aşama 1: Builder
# ==================================
FROM python:${BASE_IMAGE_TAG} AS builder

WORKDIR /app

# --- YENİ EKLENEN SİSTEM BAĞIMLILIKLARI ---
# Derleme için gerekli tüm sistem bağımlılıklarını tek seferde yükle.
# 'av' kütüphanesi için ffmpeg ve pkg-config ZORUNLUDUR.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ffmpeg \
    pkg-config \
    libavcodec-dev \
    libavdevice-dev \
    libavfilter-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libswscale-dev \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
# --- DEĞİŞİKLİK SONU ---

RUN pip install --no-cache-dir --upgrade pip poetry
ENV POETRY_VIRTUALENVS_IN_PROJECT=true
COPY poetry.lock pyproject.toml ./
RUN poetry install --without dev --no-root


# ==================================
#      Aşama 2: Final Image
# ==================================
FROM python:${BASE_IMAGE_TAG}

WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 PATH="/app/.venv/bin:$PATH"

# Yalnızca çalışma zamanı için gereken 'ffmpeg' paketini yükle
RUN apt-get update && apt-get install -y --no-install-recommends ffmpeg netcat-openbsd curl ca-certificates && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Builder aşamasından yüklenmiş Python paketlerini kopyala
COPY --from=builder --chown=appuser:appgroup /app/.venv ./.venv

# Uygulama kodunu kopyala
COPY --chown=appuser:appgroup ./app ./app

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030"]