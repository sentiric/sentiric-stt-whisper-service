import asyncio
from contextlib import asynccontextmanager
import structlog
import time
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from prometheus_fastapi_instrumentator import Instrumentator, Info

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router
from app.services.whisper_service import WhisperTranscriber
from app.services.grpc_server import serve as serve_grpc

logger = structlog.get_logger(__name__)

def add_custom_instrumentator(app: FastAPI):
    instrumentator = Instrumentator(
        should_instrument_requests=True,
        should_instrument_responses=True,
        excluded_handlers=["/metrics", "/health", "/docs", "/openapi.json"],
    )
    # Servis bilgilerini metrik etiketlerine ekle
    instrumentator.add(Info(
        {
            "service_version": settings.SERVICE_VERSION,
            "model_size": settings.STT_WHISPER_SERVICE_MODEL_SIZE,
            "device": settings.STT_WHISPER_SERVICE_DEVICE
        },
        "app_info"
    ))
    instrumentator.add(
        "transcription_duration_seconds",
        "Histogram of transcription processing time.",
    )
    instrumentator.add(
        "audio_duration_seconds",
        "Histogram of transcribed audio duration.",
    )
    instrumentator.instrument(app).expose(app, endpoint="/metrics", port=settings.STT_WHISPER_SERVICE_METRICS_PORT, include_in_schema=False)
    return instrumentator

@asynccontextmanager
async def lifespan(app: FastAPI):
    setup_logging()
    logger.info("🚀 STT Whisper Service başlatılıyor...")
    
    # Model yükleme işlemini async olarak başlat
    transcriber_instance = WhisperTranscriber()
    
    try:
        # Model yükleme için timeout ekle
        await asyncio.wait_for(
            asyncio.get_event_loop().run_in_executor(
                None, transcriber_instance.load_model
            ), 
            timeout=300.0  # 5 dakika timeout
        )
    except asyncio.TimeoutError:
        logger.error("Model yükleme zaman aşımına uğradı")
        transcriber_instance.model_loaded = False
    except Exception as e:
        logger.error("Model yükleme hatası", error=str(e))
        transcriber_instance.model_loaded = False
    
    app.state.transcriber = transcriber_instance
    app.state.model_ready = transcriber_instance.model_loaded

    # Model hazır değilse gRPC sunucusunu başlatma
    grpc_server = None
    if app.state.model_ready:
        grpc_server = await serve_grpc(transcriber_instance)
    else:
        logger.error("❌ Model yüklenemediği için gRPC sunucusu başlatılmıyor")
    
    yield
    
    if grpc_server:
        logger.info("gRPC sunucusu durduruluyor...")
        await grpc_server.stop(grace=1)

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    lifespan=lifespan
)

instrumentator = add_custom_instrumentator(app)
app.include_router(api_router, prefix="/api/v1")

@app.get("/health", tags=["Health"])
async def health_check(request: Request):
    is_ready = getattr(request.app.state, 'model_ready', False)
    status_code = 200 if is_ready else 503
    
    # GPU durumu kontrolü
    gpu_available = False
    gpu_info = {}
    try:
        import torch
        gpu_available = torch.cuda.is_available()
        if gpu_available:
            gpu_info = {
                "gpu_count": torch.cuda.device_count(),
                "current_device": torch.cuda.current_device(),
                "device_name": torch.cuda.get_device_name()
            }
    except ImportError:
        pass
    
    return JSONResponse(
        status_code=status_code,
        content={
            "status": "healthy" if is_ready else "model_loading_or_failed",
            "model_ready": is_ready,
            "gpu_available": gpu_available,
            "gpu_info": gpu_info,
            "service_version": settings.SERVICE_VERSION,
            "model_size": settings.STT_WHISPER_SERVICE_MODEL_SIZE
        }
    )

@app.get("/")
async def root():
    return {"message": f"Welcome to {settings.PROJECT_NAME}"}