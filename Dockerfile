# --- STAGE 1: Builder ---
# Whisper ve ctranslate2 genellikle bazı sistem kütüphanelerine ihtiyaç duyar
FROM python:3.11-slim-bullseye AS builder

RUN apt-get update && \
    # Build araçları ve gerekli kütüphaneler (ctranslate2 için)
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libopenblas-dev \
    libjemalloc-dev \
    git \
    # --- ÇÖZÜM: PyAV (av paketi) için gerekli FFmpeg kütüphaneleri EKLENDİ ---
    ffmpeg \
    libavformat-dev \
    libavcodec-dev \
    libavdevice-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev && \
    rm -rf /var/lib/apt/lists/*

# Poetry kurulumu
RUN pip install poetry

# Build argümanlarını tanımla
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"

WORKDIR /app

# Proje dosyalarını kopyala
COPY pyproject.toml ./
COPY app ./app
COPY README.md .

# Bağımlılıkları kur
RUN poetry install --no-root --only main

# --- STAGE 2: Production ---
# --- STAGE 2: Production ---
FROM python:3.11-slim-bullseye

WORKDIR /app

# Gerekli sistem bağımlılıkları (jemalloc ve temel araçlar)
RUN apt-get update && apt-get install -y --no-install-recommends \
    netcat-openbsd \
    curl \
    ca-certificates \
    libjemalloc2 \
    ffmpeg && \
    rm -rf /var/lib/apt/lists/*

# Root olmayan kullanıcı oluştur
RUN useradd -m -u 1001 appuser

# Builder'dan sanal ortam bağımlılıklarını kopyala
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
COPY --from=builder /app/app ./app

# Dosya sahipliğini yeni kullanıcıya ver
RUN chown -R appuser:appuser /app

# YENİ: Build argümanlarını environment değişkenlerine ata
ARG GIT_COMMIT
ARG BUILD_DATE
ARG SERVICE_VERSION
ENV GIT_COMMIT=${GIT_COMMIT}
ENV BUILD_DATE=${BUILD_DATE}
ENV SERVICE_VERSION=${SERVICE_VERSION}
# Whisper model konfigürasyonu
ENV WHISPER_MODEL_SIZE=large-v3
ENV WHISPER_DEVICE=cpu

USER appuser

# Başlangıç komutu: STT Motoru HTTP'de 15011'de dinleyecektir (Gateway'in iç portu)
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011"]