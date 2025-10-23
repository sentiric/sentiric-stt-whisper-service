# =================================================================
#    SENTIRIC STT-WHISPER-SERVICE - CPU & SIZE-OPTIMIZED (MULTI-STAGE) v9
# =================================================================

# --- STAGE 1: Builder ---
# Bu aşama, Python paketlerini indirip bir "wheelhouse" (paket arşivi) oluşturur.
FROM python:3.11-slim-bookworm AS builder

WORKDIR /wheelhouse

# Builder aşamasında sadece `git` gerekli (sentiric-contracts için).
RUN apt-get update && apt-get install -y --no-install-recommends git \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

COPY requirements.txt .

# Bağımlılıkları kurmak yerine, onları tekerlek (wheel) formatında indiriyoruz.
RUN pip wheel --no-cache-dir -r requirements.txt

# --- STAGE 2: Final Production Image ---
FROM python:3.11-slim-bookworm

# ENV'ler ve ARG'lar
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"

ENV GIT_COMMIT=${GIT_COMMIT} \
    BUILD_DATE=${BUILD_DATE} \
    SERVICE_VERSION=${SERVICE_VERSION} \
    PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    NUMBA_CACHE_DIR=/tmp \
    HF_HOME=/app/model-cache \
    TZ=Etc/UTC \
    DEBIAN_FRONTEND=noninteractive

WORKDIR /app

# Sadece RUNTIME için gerekli sistem bağımlılıklarını kur
RUN apt-get update && apt-get install -y --no-install-recommends \
    ffmpeg \
    libsndfile1 \
    curl \
    # OPTIMIZATION 1: APT cache'ini ve gereksiz dosyaları temizle
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Sanal ortamı oluştur
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Builder aşamasında indirilen wheel dosyalarını kopyala
COPY --from=builder /wheelhouse /wheelhouse

# Bağımlılıkları önbellekten kur VE HEMEN ARDINDAN GEREKSIZ DOSYALARI TEMIZLE
RUN pip install --no-cache-dir /wheelhouse/*.whl \
    # OPTIMIZATION 2: Wheelhouse'ı (indirilen paketleri) kurulumdan sonra sil
    && rm -rf /wheelhouse \
    # OPTIMIZATION 3: pip'in kendi cache'ini temizle
    && rm -rf /root/.cache/pip \
    # OPTIMIZATION 4: Derlenmiş Python dosyalarını ve testleri bulup sil
    && find /opt/venv -type f -name '*.pyc' -delete \
    && find /opt/venv -type d -name '__pycache__' -delete \
    && find /opt/venv -type d -name 'tests' -delete

# Uygulama kullanıcısı ve kod
RUN addgroup --system --gid 1001 appgroup && \
    adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser
COPY --chown=appuser:appgroup ./app ./app
RUN mkdir -p /app/model-cache /tmp/numba_cache \
    && chown -R appuser:appgroup /app /tmp/numba_cache

USER appuser

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:15030/health || exit 1

EXPOSE 15030 15031 15032

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030", "--no-access-log"]