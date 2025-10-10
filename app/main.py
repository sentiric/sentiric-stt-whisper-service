from contextlib import asynccontextmanager
import structlog
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router

logger = structlog.get_logger(__name__)

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    setup_logging()
    logger.info(
        "🚀 STT Whisper Service starting...",
        version=settings.SERVICE_VERSION,
        model=settings.WHISPER_MODEL_SIZE
    )
    
    # Model otomatik olarak yüklenecek (WhisperTranscriber.__init__)
    
    yield
    
    # Shutdown
    logger.info("🛑 STT Whisper Service shutting down")

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    lifespan=lifespan,
    docs_url="/docs",
    redoc_url="/redoc"
)

# Routes
app.include_router(api_router, prefix="/api/v1")

@app.get("/health")
async def health_check():
    """Basit health check - model hazır mı?"""
    from app.api.v1.endpoints import transcriber
    return {
        "status": "healthy" if transcriber.model_loaded else "loading",
        "service": "stt-whisper",
        "model_loaded": transcriber.model_loaded,
        "model_size": settings.WHISPER_MODEL_SIZE
    }

@app.get("/")
async def root():
    return {"message": "Sentiric STT Whisper Service - Saf Transkripsiyon Motoru"}

# Global exception handler
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    logger.error("Global exception handler", error=str(exc), path=request.url.path)
    return JSONResponse(
        status_code=500,
        content={"detail": "Internal server error"}
    )