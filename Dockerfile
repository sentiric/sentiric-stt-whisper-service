# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - OPTIMIZED PRODUCTION v18.0
#    MULTI-STAGE BUILD + SIZE OPTIMIZATION
# =================================================================
ARG PYTHON_VERSION=3.11-slim-bookworm

# Build stage for dependencies
FROM python:${PYTHON_VERSION} AS builder

WORKDIR /app

# Build arguments
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown" 
ARG SERVICE_VERSION="0.0.0"

# Install build dependencies including git
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements first for better caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir --user -r requirements.txt

# Runtime stage
FROM python:${PYTHON_VERSION} AS production

WORKDIR /app

# Build arguments
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown" 
ARG SERVICE_VERSION="0.0.0"

# Environment variables
ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    NUMBA_DEBUG=0 \
    NUMBA_DISABLE_JIT=0 \
    NUMBA_CACHE_DIR=/tmp \
    LIBROSA_CACHE_LEVEL=0 \
    PYTHONWARNINGS=ignore \
    HF_HOME=/app/model-cache \
    HF_HUB_DISABLE_SYMLINKS_WARNING=1 \
    STT_WHISPER_SERVICE_DEVICE=auto \
    STT_WHISPER_SERVICE_MODEL_LOAD_TIMEOUT=600 \
    PATH=/app/.local/bin:$PATH

# Runtime dependencies (minimal)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Copy installed packages from builder
COPY --from=builder /root/.local /app/.local

# App user
RUN addgroup --system --gid 1001 appgroup \
    && adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Application code
COPY --chown=appuser:appgroup ./app ./app

# Create cache directories
RUN mkdir -p /app/model-cache /tmp/numba_cache \
    && chown -R appuser:appgroup /app /tmp/numba_cache

USER appuser

# Healthcheck with retry logic
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:${STT_WHISPER_SERVICE_HTTP_PORT:-15030}/health || exit 1

EXPOSE 15030 15031 15032

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]