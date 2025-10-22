# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - CPU OPTIMIZED v21.0
# =================================================================
FROM python:3.11-slim-bookworm AS production

WORKDIR /app

# Build argümanları
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"

# Environment değişkenleri
ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    NUMBA_CACHE_DIR=/tmp \
    LIBROSA_CACHE_LEVEL=0 \
    PYTHONWARNINGS=ignore \
    HF_HOME=/app/model-cache \
    STT_WHISPER_SERVICE_DEVICE=cpu \
    STT_WHISPER_SERVICE_MODEL_LOAD_TIMEOUT=300

# YENİ EKLENEN SATIRLAR: İnteraktif prompt'ları engelle
ENV TZ=Etc/UTC
ENV DEBIAN_FRONTEND=noninteractive

# Sistem bağımlılıklarını tek katmanda kur ve temizle
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    curl \
    git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# ... dosyanın geri kalanı aynı ...
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip && \
    pip install --no-cache-dir -r requirements.txt
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser
COPY --chown=appuser:appgroup ./app ./app
RUN mkdir -p /app/model-cache /tmp/numba_cache \
    && chown -R appuser:appgroup /app /tmp/numba_cache
USER appuser
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15030/health || exit 1
EXPOSE 15030 15031
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]