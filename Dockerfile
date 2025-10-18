# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - DOCKERFILE v4.0 (UNIVERSAL)
# =================================================================
# Build argümanları ile temel imajı dinamik olarak seç
ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE=python:${PYTHON_VERSION}-slim-bookworm

# --- Aşama 1: Builder ---
FROM ${BASE_IMAGE} AS builder

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Temel imaja göre doğru paketleri kurmak için koşullu mantık
RUN \
    apt-get update && \
    if [ -f /etc/debian_version ]; then \
        # Debian (CPU) için kurulum
        apt-get install -y --no-install-recommends \
            build-essential cmake pkg-config git curl \
            ffmpeg libavformat-dev libavcodec-dev libavdevice-dev \
            libavutil-dev libavfilter-dev libswscale-dev libswresample-dev; \
    else \
        # Ubuntu (GPU) için kurulum
        apt-get install -y --no-install-recommends \
            build-essential cmake pkg-config git curl \
            python${PYTHON_VERSION} python${PYTHON_VERSION}-venv python3-pip \
            ffmpeg libavformat-dev libavcodec-dev libavdevice-dev \
            libavutil-dev libavfilter-dev libswscale-dev libswresample-dev; \
        update-alternatives --install /usr/bin/python python /usr/bin/python${PYTHON_VERSION} 1; \
        update-alternatives --install /usr/bin/pip pip /usr/bin/pip3 1; \
    fi && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir --upgrade pip poetry
ENV POETRY_VIRTUALENVS_IN_PROJECT=true

COPY poetry.lock pyproject.toml ./
# GPU imajı build edilirken --extras cuda kullanılır, diğerinde kullanılmaz
ARG POETRY_EXTRAS=""
RUN poetry install --without dev --no-root ${POETRY_EXTRAS}

# --- Aşama 2: Final Image ---
FROM ${BASE_IMAGE} AS final
WORKDIR /app

ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 \
    PATH="/app/.venv/bin:$PATH" \
    HF_HOME="/app/model-cache" \
    LD_LIBRARY_PATH="/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}"

# Sadece runtime bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg netcat-openbsd curl ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser

COPY --from=builder --chown=appuser:appgroup /app/.venv ./.venv
COPY --chown=appuser:appgroup ./app ./app

RUN mkdir -p /app/model-cache && \
    chown -R appuser:appgroup /app/model-cache

USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]