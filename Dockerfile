# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v10.0 (PRODUCTION)
# =================================================================
ARG BASE_IMAGE=python:3.11-slim-bookworm

# --- Aşama 1: Builder (Poetry ile Bağımlılıkları Kurma) ---
FROM python:3.11-slim-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    POETRY_NO_INTERACTION=1 \
    POETRY_VIRTUALENVS_CREATE=false \
    POETRY_CACHE_DIR='/var/cache/pypoetry' \
    PIP_NO_CACHE_DIR=off \
    PIP_DISABLE_PIP_VERSION_CHECK=on \
    # Debug çıktılarını kapat
    NUMBA_DEBUG=0 \
    NUMBA_DISABLE_JIT=0 \
    PYTHONWARNINGS=ignore

WORKDIR /app

# Sistem bağımlılıkları (optimize edilmiş)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl pkg-config \
    ffmpeg libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev \
    libavfilter-dev libavutil-dev libswresample-dev libsndfile1 \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Poetry ve bağımlılıkları kur
RUN pip install --upgrade pip && pip install poetry

# Bağımlılık tanımlarını kopyala ve kur
COPY poetry.lock pyproject.toml ./
RUN poetry install --only main --no-root --no-directory --no-ansi

# --- Aşama 2: Final Image (Production) ---
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"

# Environment değişkenleri
ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTHONWARNINGS=ignore \
    # Numba optimizasyonları
    NUMBA_DEBUG=0 \
    NUMBA_WARNINGS=0 \
    NUMBA_DISABLE_JIT=0 \
    NUMBA_CACHE_DIR=/tmp/numba_cache \
    # Librosa optimizasyonları
    LIBROSA_CACHE_LEVEL=0 \
    # HuggingFace optimizasyonları
    HF_HOME=/app/model-cache \
    HF_HUB_DISABLE_SYMLINKS_WARNING=1 \
    HF_HUB_VERBOSITY=error \
    # Diğer optimizasyonlar
    MPLCONFIGDIR=/tmp/matplotlib_cache \
    TF_CPP_MIN_LOG_LEVEL=2 \
    CUDA_VISIBLE_DEVICES=0

# Runtime bağımlılıkları (minimal)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg libsndfile1 curl ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Kullanıcı oluştur (security)
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Cache dizinlerini oluştur ve yetkilendir
RUN mkdir -p /tmp/numba_cache /tmp/matplotlib_cache && \
    chown -R appuser:appgroup /tmp/numba_cache /tmp/matplotlib_cache

# Builder aşamasından paketleri kopyala
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin

# Uygulama kodunu kopyala
COPY --chown=appuser:appgroup ./app ./app

# Model cache dizini oluştur
RUN mkdir -p /app/model-cache && chown -R appuser:appgroup /app

USER appuser

# Healthcheck
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:${STT_WHISPER_SERVICE_HTTP_PORT:-15030}/health || exit 1

EXPOSE 15030 15031 15032

CMD ["sh", "-c", "uvicorn app.main:app --host 0.0.0.0 --port ${STT_WHISPER_SERVICE_HTTP_PORT:-15030} --no-access-log"]