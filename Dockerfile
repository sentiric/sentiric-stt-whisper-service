### 📄 File: [ilgili-servis]/Dockerfile (STANDART ŞABLON)

ARG PYTHON_VERSION=3.11
ARG BASE_IMAGE_TAG=${PYTHON_VERSION}-slim-bullseye

# STAGE 1: BUILDER
FROM python:${BASE_IMAGE_TAG} AS builder
WORKDIR /app
RUN apt-get update && apt-get install -y --no-install-recommends curl build-essential && \
    pip install --no-cache-dir --upgrade pip poetry && \
    rm -rf /var/lib/apt/lists/*
ENV POETRY_VIRTUALENVS_IN_PROJECT=true
COPY poetry.lock pyproject.toml ./
RUN poetry install --without dev --no-root

# STAGE 2: PRODUCTION
FROM python:${BASE_IMAGE_TAG}
WORKDIR /app
ARG GIT_COMMIT="unknown"
ARG BUILD_DATE="unknown"
ARG SERVICE_VERSION="0.0.0"
ENV GIT_COMMIT=${GIT_COMMIT} BUILD_DATE=${BUILD_DATE} SERVICE_VERSION=${SERVICE_VERSION} PYTHONUNBUFFERED=1 PATH="/app/.venv/bin:$PATH"
RUN apt-get update && apt-get install -y --no-install-recommends netcat-openbsd curl ca-certificates && rm -rf /var/lib/apt/lists/*
RUN addgroup --system --gid 1001 appgroup && adduser --system --no-create-home --uid 1001 --ingroup appgroup appuser
COPY --from=builder --chown=appuser:appgroup /app/.venv ./.venv
COPY --chown=appuser:appgroup ./app ./app
USER appuser

EXPOSE 15030 15031 15032
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "15030"]