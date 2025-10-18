from contextlib import asynccontextmanager
import structlog
import asyncio
import grpc
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router
from app.services.whisper_service import WhisperTranscriber
from app.services.grpc_server import serve as serve_grpc

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

    grpc_server = None
    if app.state.model_ready:
        # Model hazırsa gRPC sunucusunu başlat
        grpc_server = await serve_grpc(transcriber_instance)
    
    yield
    
    # Kapanış
    if grpc_server:
        logger.info("gRPC sunucusu durduruluyor...")
        await grpc_server.stop(grace=1)
    
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
            "status": "healthy" if is_ready else "model_loading_or_failed",
            "service": settings.PROJECT_NAME,
            "model_ready": is_ready,
        }
    )

@app.get("/")
async def root():
    return {"message": f"Welcome to {settings.PROJECT_NAME} v{settings.SERVICE_VERSION}"}