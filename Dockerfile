# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - CPU & SIZE-OPTIMIZED (STABLE v12)
# =================================================================

# --- STAGE 1: Builder ---
FROM python:3.11-slim-bookworm AS builder
WORKDIR /wheelhouse
# ÖNEMLİ: wheel paketini kuruyoruz!
RUN apt-get update && apt-get install -y --no-install-recommends git && \
    pip install --no-cache-dir wheel && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
COPY requirements.txt .
RUN pip wheel --no-cache-dir -r requirements.txt

# --- STAGE 2: Final Production Image ---
FROM python:3.11-slim-bookworm
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    HF_HOME=/app/model-cache
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    curl \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"
COPY --from=builder /wheelhouse /wheelhouse
RUN pip install --no-cache-dir /wheelhouse/*.whl \
    && rm -rf /wheelhouse /root/.cache/pip
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser
COPY --chown=appuser:appgroup ./app ./app
RUN mkdir -p /app/model-cache && chown -R appuser:appgroup /app/model-cache
USER appuser
HEALTHCHECK --interval=30s --timeout=10s --start-period=15s --retries=3 \
    CMD curl -f http://localhost:15030/health || exit 1
EXPOSE 15030 15031
CMD ["python", "-m", "app.runner"]