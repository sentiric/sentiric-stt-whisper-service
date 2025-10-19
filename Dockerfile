# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v8.6 (STABLE)
# =================================================================
# Build argümanları ile temel imajı dinamik olarak seç (sadece final aşaması için)
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE=python:${PYTHON_VERSION}-slim-bookworm

# --- Aşama 1: Builder (Her zaman Python temelli) ---
# HATA DÜZELTME: Builder aşamasının temel imajı, build araçlarını ve Python'u içeren
# sabit bir imaj olmalıdır. Dinamik BASE_IMAGE kullanımı burada hataya neden oluyordu.
FROM python:3.11-slim-bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Temel bağımlılıklar (optimize edilmiş)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl pkg-config \
    ffmpeg libavcodec-dev libavformat-dev libswscale-dev libavdevice-dev \
    libsndfile1 portaudio19-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /var/cache/apt/*

# Pip'i güvenli şekilde yükle
RUN python3 -m ensurepip --upgrade
RUN python3 -m pip install --no-cache-dir --upgrade pip

# Önce temel bağımlılıkları kur
COPY requirements.txt .
RUN pip install --no-cache-dir --timeout 300 \
    numpy==1.26.4 \
    soundfile==0.12.1 \
    librosa==0.10.1

# PyTorch'u doğru kaynaktan kur (Builder aşaması için CPU yeterlidir)
RUN pip install --no-cache-dir --timeout 600 \
    torch==2.3.0 torchvision==0.18.0 torchaudio==2.3.0 \
    --index-url https://download.pytorch.org/whl/cpu

# Kalan bağımlılıkları kur
RUN pip install --no-cache-dir --timeout 300 -r requirements.txt

# --- Aşama 2: Final Image (Dinamik Temel) ---
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/usr/local/bin:$PATH" \
    HF_HOME="/app/model-cache" \
    PYTHONPATH="/app"

# Runtime bağımlılıklar
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg libsndfile1 curl ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /var/cache/apt/*

# CUDA için (GPU image) - sadece gerekli kütüphaneler
RUN if [ -x "$(command -v nvidia-smi)" ]; then \
        echo "CUDA detected, installing runtime libraries..." && \
        apt-get update && apt-get install -y --no-install-recommends \
        cuda-cudart-12-1 cuda-nvtx-12-1 && \
        apt-get clean && rm -rf /var/lib/apt/lists/* /var/cache/apt/*; \
    else \
        echo "No CUDA detected, skipping CUDA libraries"; \
    fi

# Kullanıcı oluştur
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

# Python paketlerini kopyala
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /usr/local/bin /usr/local/bin

# Uygulama kodunu kopyala
COPY --chown=appuser:appgroup ./app ./app

# Model cache dizini oluştur
RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

# Sağlık kontrolü için healtcheck ekle
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15030/health || exit 1

EXPOSE 15030 15031 15032

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]