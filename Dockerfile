# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - OPTIMIZED DOCKERFILE v11.0
# =================================================================
ARG BASE_IMAGE=python:3.11-slim-bookworm

# --- Stage 1: Dependencies Builder ---
FROM python:3.11-slim-bookworm AS builder

WORKDIR /app

# Sistem bağımlılıkları - MİNİMAL
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ffmpeg \
    libsndfile1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Poetry kurulumu
RUN pip install --no-cache-dir poetry

# Bağımlılıkları kopyala ve kur
COPY pyproject.toml poetry.lock ./
RUN poetry install --only main --no-root --no-directory

# --- Stage 2: Production Image ---
FROM ${BASE_IMAGE} AS production

WORKDIR /app

# Build arg'lar
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown" 
ARG SERVICE_VERSION="0.0.0"

# Environment variables - OPTIMIZED
ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    # Performance optimizations
    NUMBA_DEBUG=0 \
    NUMBA_DISABLE_JIT=0 \
    NUMBA_CACHE_DIR=/tmp \
    LIBROSA_CACHE_LEVEL=0 \
    PYTHONWARNINGS=ignore \
    # Model cache
    HF_HOME=/app/model-cache \
    HF_HUB_DISABLE_SYMLINKS_WARNING=1

# Runtime dependencies - MİNİMAL
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# App user
RUN addgroup --system --gid 1001 appgroup \
    && adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Python packages from builder
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin

# Application code
COPY --chown=appuser:appgroup ./app ./app

# Create cache directories
RUN mkdir -p /app/model-cache /tmp/numba_cache \
    && chown -R appuser:appgroup /app /tmp/numba_cache

USER appuser

# Healthcheck
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:${STT_WHISPER_SERVICE_HTTP_PORT:-15030}/health || exit 1

EXPOSE 15030 15031 15032

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]