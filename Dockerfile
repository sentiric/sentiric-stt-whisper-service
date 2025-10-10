# --- STAGE 1: Builder ---
# Whisper ve ctranslate2 genellikle bazı sistem kütüphanelerine ihtiyaç duyar
FROM python:3.11-slim-bullseye AS builder

# Gerekli sistem bağımlılıkları
# C-eklentilerini (PyAV, CTranslate2) kaynak koddan derlemek için gereken her şey
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    python3-dev \
    libopenblas-dev \
    libjemalloc-dev \
    git \
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

WORKDIR /app

# Sadece pyproject.toml kopyalanıyor, lock dosyası build ortamında çözülecek
COPY pyproject.toml ./
COPY app ./app
COPY README.md .

# --- NİHAİ ÇÖZÜM ---
# 1. Poetry'nin çözemediği 'av' paketini 'pip' ile zorla kur.
# 2. Ardından, 'av' zaten kurulu olduğu için sorunsuz çalışacak olan 'poetry install' komutunu çalıştır.
RUN poetry config virtualenvs.create false && \
    pip install av && \
    poetry install --no-root --only main --no-interaction --no-ansi

# --- STAGE 2: Production ---
FROM python:3.11-slim-bullseye

WORKDIR /app

# Gerekli sistem bağımlılıkları (PyAV için ffmpeg ve ctranslate2 için jemalloc)
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

# Build argümanlarını environment değişkenlerine ata
ARG GIT_COMMIT
ARG BUILD_DATE
ARG SERVICE_VERSION
ENV GIT_COMMIT=${GIT_COMMIT}
ENV BUILD_DATE=${BUILD_DATE}
ENV SERVICE_VERSION=${SERVICE_VERSION}
ENV WHISPER_MODEL_SIZE=large-v3
ENV WHISPER_DEVICE=cpu

USER appuser

# Başlangıç komutu
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011"]