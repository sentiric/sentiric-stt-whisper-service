from contextlib import asynccontextmanager
import structlog
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router
from app.services.whisper_service import WhisperTranscriber

logger = structlog.get_logger(__name__)

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Başlangıç
    setup_logging()
    logger.info("🚀 STT Whisper Service başlatılıyor...")
    
    transcriber_instance = WhisperTranscriber()
    transcriber_instance.load_model()
    
    app.state.transcriber = transcriber_instance
    app.state.model_ready = transcriber_instance.model_loaded
    
    yield
    
    # Kapanış
    logger.info("🛑 STT Whisper Service kapatılıyor.")
    app.state.transcriber = None

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    lifespan=lifespan
)

app.include_router(api_router, prefix="/api/v1")

@app.get("/health", tags=["Health"])
async def health_check(request: Request):
    is_ready = getattr(request.app.state, 'model_ready', False)
    status_code = 200 if is_ready else 503
    
    return JSONResponse(
        status_code=status_code,
        content={
            "status": "healthy" if is_ready else "model_loading_failed",
            "service": settings.PROJECT_NAME,
            "model_ready": is_ready,
            "model_size": settings.WHISPER_MODEL_SIZE,
            "version": settings.SERVICE_VERSION
        }
    )

@app.get("/")
async def root():
    return {"message": f"Welcome to {settings.PROJECT_NAME} v{settings.SERVICE_VERSION}"}