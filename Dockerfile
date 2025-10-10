FROM python:3.11-slim-bullseye

WORKDIR /app

# Sistem bağımlılıkları - SADECE GEREKLİ OLANLAR
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Python bağımlılıkları - WHISPER ÖZEL
RUN pip install --no-cache-dir \
    fastapi==0.111.0 \
    uvicorn[standard]==0.29.0 \
    pydantic==2.7.3 \
    python-multipart==0.0.9 \
    structlog==24.1.0 \
    faster-whisper==1.0.3 \
    ctranslate2==4.0.0 \
    soundfile==0.12.1 \
    librosa==0.10.1 \
    numpy==1.26.4

# Uygulama kodunu kopyala
COPY ./app ./app
COPY pyproject.toml .

# Health check
HEALTHCHECK --interval=30s --timeout=30s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15011/health || exit 1

EXPOSE 15011

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011", "--no-access-log"]