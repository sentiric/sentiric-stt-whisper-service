# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v6.1 (CPU - FINAL FIX)
# =================================================================
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE_TAG=${PYTHON_VERSION}-slim-bookworm

# --- Aşama 1: Builder ---
FROM python:${BASE_IMAGE_TAG} AS builder
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Debian için doğru venv paket adını kullanıyoruz: python3-venv
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config git curl python3-venv \
    ffmpeg libavformat-dev libavcodec-dev libavdevice-dev \
    libavutil-dev libavfilter-dev libswscale-dev libswresample-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir --upgrade pip poetry
ENV POETRY_VIRTUALENVS_IN_PROJECT=true

COPY poetry.lock pyproject.toml ./
RUN poetry install --without dev --no-root

# --- Aşama 2: Final Image ---
FROM python:${BASE_IMAGE_TAG} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH" \
    HF_HOME="/app/model-cache"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg netcat-openbsd curl ca-certificates libgomp1 \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

COPY --from=builder --chown=appuser:appgroup /app/.venv ./.venv
COPY --from=builder --chown=appuser:appgroup /app/app ./app

RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]