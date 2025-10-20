# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v9.0 (POETRY NATIVE)
# =================================================================
ARG BASE_IMAGE=python:3.11-slim-bookworm

# --- Aşama 1: Builder (Poetry ile Bağımlılıkları Kurma) ---
FROM python:3.11-slim-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    POETRY_NO_INTERACTION=1 \
    POETRY_VIRTUALENVS_CREATE=false \
    POETRY_CACHE_DIR='/var/cache/pypoetry' \
    PIP_NO_CACHE_DIR=off \
    PIP_DISABLE_PIP_VERSION_CHECK=on

WORKDIR /app

# Sistem bağımlılıkları (TÜM FFMPEG DEV PAKETLERİ EKLENDİ)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl pkg-config \
    ffmpeg libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev libavfilter-dev libavutil-dev libswresample-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Builder aşamasında protobuf'u önce kurun
RUN pip install --upgrade pip && pip install poetry

# Sadece bağımlılık tanımlarını kopyala ve kur
COPY poetry.lock pyproject.toml ./
RUN poetry install --only main --no-root --no-directory

# --- Aşama 2: Final Image (Çalışma Ortamı) ---
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    HF_HOME="/app/model-cache" \
    PYTHONPATH="/app"

# Runtime bağımlılıkları
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg libsndfile1 curl ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Kullanıcı oluştur
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Builder aşamasından kurulan Python paketlerini kopyala
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin

# Uygulama kodunu kopyala
COPY --chown=appuser:appgroup ./app ./app

# Model cache dizini oluştur ve yetkilendir
RUN mkdir -p /app/model-cache && chown -R appuser:appgroup /app

USER appuser

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15030/health || exit 1

EXPOSE 15030 15031 15032

CMD ["sh", "-c", "uvicorn app.main:app --host 0.0.0.0 --port ${STT_WHISPER_SERVICE_HTTP_PORT:-15030} --no-access-log"]