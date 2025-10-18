from contextlib import asynccontextmanager
import structlog
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router, transcriber

logger = structlog.get_logger(__name__)

@asynccontextmanager
async def lifespan(app: FastAPI):
    setup_logging()
    logger.info(
        "🚀 STT Whisper Service başlatılıyor...",
        version=settings.SERVICE_VERSION,
        model=settings.WHISPER_MODEL_SIZE
    )
    # Model yüklemesi `endpoints` modülü import edilirken tetiklendi.
    yield
    logger.info("🛑 STT Whisper Service kapatılıyor.")

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    lifespan=lifespan
)

app.include_router(api_router, prefix="/api/v1")

@app.get("/health", tags=["Health"])
async def health_check():
    """Servisin ve AI modelinin durumunu kontrol eder."""
    is_loaded = transcriber.model_loaded
    status_code = 200 if is_loaded else 503
    
    response_data = {
        "status": "healthy" if is_loaded else "model_loading",
        "service": "stt-whisper-service",
        "model_loaded": is_loaded,
        "model_size": settings.WHISPER_MODEL_SIZE
    }
    
    return JSONResponse(status_code=status_code, content=response_data)

@app.get("/")
async def root():
    return {"message": "Sentiric STT Whisper Service - Uzman Transkripsiyon Motoru"}