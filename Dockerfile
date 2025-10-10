FROM python:3.11-slim-bullseye

WORKDIR /app

# Sistem bağımlılıkları
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Poetry'yi yükle
RUN pip install poetry

# Poetry'nin sanal ortam oluşturmasını engelle
RUN poetry config virtualenvs.create false

# Sadece bağımlılık dosyalarını kopyala (Docker katman önbelleği için optimizasyon)
COPY pyproject.toml ./

# Bağımlılıkları yükle (sadece production, geliştirme bağımlılıkları hariç)
RUN poetry install --no-interaction --no-ansi --no-root --only main

# Uygulama kodunu kopyala
COPY ./app ./app

# Health check
HEALTHCHECK --interval=30s --timeout=30s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15011/health || exit 1

EXPOSE 15011

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011", "--no-access-log"]