# --- STAGE 1: Builder ---
# Whisper ve ctranslate2 genellikle bazı sistem kütüphanelerine ihtiyaç duyar
FROM python:3.11-slim-bullseye AS builder

# Gerekli sistem bağımlılıkları
# build-essential & cmake: CTranslate2 için derleme araçları
# ffmpeg ve dev kütüphaneleri: faster-whisper'ın bağımlılığı olan PyAV (av) için
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
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

# Build argümanlarını tanımla
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"

WORKDIR /app

# Sadece pyproject.toml kopyalanıyor, lock dosyası build ortamında çözülecek
COPY pyproject.toml ./
COPY app ./app
COPY README.md .

# Bağımlılıkları kur. Sanal ortam oluşturmadan doğrudan sisteme kur.
# Bu, ikinci aşamaya kopyalamayı kolaylaştırır.
RUN poetry config virtualenvs.create false && \
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

# Whisper model konfigürasyonu
ENV WHISPER_MODEL_SIZE=large-v3
ENV WHISPER_DEVICE=cpu

USER appuser

# Başlangıç komutu: STT Motoru HTTP'de 15011'de dinleyecektir (Gateway'in iç portu)
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011"]