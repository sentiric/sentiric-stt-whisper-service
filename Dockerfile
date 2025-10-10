# ======================================================================================
#    SENTIRIC PYTHON SERVICE - NIHAI DOCKERFILE v3.0 (BRUTE FORCE BUILD)
# ======================================================================================
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE_TAG=${PYTHON_VERSION}-slim-bullseye

# STAGE 1: BUILDER
FROM python:${BASE_IMAGE_TAG} AS builder
WORKDIR /app
ENV PIP_BREAK_SYSTEM_PACKAGES=1 PIP_NO_CACHE_DIR=1 POETRY_NO_INTERACTION=1 POETRY_VIRTUALENVS_IN_PROJECT=true

# ADIM 1: 'av' paketini kaynak koddan derlemek için GEREKEN HER ŞEYİ kur.
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    python3-dev \
    ffmpeg \
    libavformat-dev \
    libavcodec-dev \
    libavdevice-dev \
    libavutil-dev \
    libswscale-dev \
    libswresample-dev && \
    pip install --no-cache-dir --upgrade pip poetry && \
    rm -rf /var/lib/apt/lists/*

COPY pyproject.toml ./

# ADIM 2: NIHAI ÇÖZÜM
# Önce SADECE problemli 'av' paketini 'pip' ile kur. Ortam hazır olduğu için bu çalışacak.
# Sonra 'poetry install' çalıştır. Poetry, 'av'yi atlayıp geri kalanları kuracak.
# NOT: Artık --sync yok, çünkü lock dosyamız yok.
RUN pip install av && \
    poetry install --without dev --no-root

# STAGE 2: PRODUCTION
FROM python:${BASE_IMAGE_TAG}
WORKDIR /app
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 PATH="/app/.venv/bin:$PATH" HF_HUB_DISABLE_SYMLINKS_WARNING=1 \
    HF_HOME="/tmp/huggingface_cache"

RUN apt-get update && apt-get install -y --no-install-recommends \
    netcat-openbsd curl ca-certificates ffmpeg \
    && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup user

COPY --from=builder --chown=user:appgroup /app/.venv ./.venv
COPY --chown=user:appgroup ./app ./app

USER user

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15011", "--no-access-log"]