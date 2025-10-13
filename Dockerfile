# ==================================
#      Aşama 1: Builder
# ==================================
FROM python:3.11-slim-bullseye AS builder

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ffmpeg libavcodec-dev libavdevice-dev \
    libavfilter-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip install poetry==1.8.2
RUN poetry config virtualenvs.create false
COPY pyproject.toml ./
RUN poetry install --no-interaction --no-ansi --no-root --only main


# ==================================
#      Aşama 2: Final Image
# ==================================
FROM python:3.11-slim-bullseye AS final

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends ffmpeg && rm -rf /var/lib/apt/lists/*

# Builder aşamasından yüklenmiş Python paketlerini ve komutlarını kopyala
COPY --from=builder /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages
# !!! === ÇÖZÜM BU SATIR === !!!
# uvicorn gibi komutların bulunduğu bin dizinini de kopyala
COPY --from=builder /usr/local/bin /usr/local/bin

# Uygulama kodunu kopyala
COPY ./app ./app

HEALTHCHECK --interval=30s --timeout=30s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15031/health || exit 1

EXPOSE 15031

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15031", "--no-access-log"]