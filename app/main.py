import asyncio
from contextlib import asynccontextmanager
import structlog
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from prometheus_fastapi_instrumentator import Instrumentator
from prometheus_client import Gauge

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router
from app.services.whisper_service import WhisperTranscriber

logger = structlog.get_logger(__name__)

# GRPC server'ı opsiyonel yap - hata durumunda devre dışı bırak
async def setup_grpc_server(transcriber):
    """GRPC server'ı güvenli şekilde başlat"""
    try:
        from app.services.grpc_server import serve as serve_grpc
        return await serve_grpc(transcriber)
    except Exception as e:
        logger.warning("GRPC server başlatılamadı, devre dışı bırakılıyor", error=str(e))
        return None

def add_custom_instrumentator(app: FastAPI):
    instrumentator = Instrumentator()
    app_info_gauge = Gauge(
        "app_info",
        "Application information",
        ["service_version", "model_size", "device"]
    )
    
    app_info_gauge.labels(
        service_version=settings.SERVICE_VERSION,
        model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
        device=settings.STT_WHISPER_SERVICE_DEVICE
    ).set(1)
    
    instrumentator.instrument(app)
    
    @app.get("/metrics", include_in_schema=False)
    async def metrics():
        from prometheus_client import generate_latest, CONTENT_TYPE_LATEST
        from fastapi.responses import Response
        return Response(
            content=generate_latest(),
            media_type=CONTENT_TYPE_LATEST
        )
    
    return instrumentator

@asynccontextmanager
async def lifespan(app: FastAPI):
    setup_logging()
    logger.info("🚀 STT Whisper Service başlatılıyor...")
    
    # Model yükleme
    transcriber_instance = WhisperTranscriber()
    
    try:
        await asyncio.wait_for(
            asyncio.get_event_loop().run_in_executor(
                None, transcriber_instance.load_model
            ), 
            timeout=300.0
        )
    except asyncio.TimeoutError:
        logger.error("Model yükleme zaman aşımına uğradı")
        transcriber_instance.model_loaded = False
    except Exception as e:
        logger.error("Model yükleme hatası", error=str(e))
        transcriber_instance.model_loaded = False
    
    app.state.transcriber = transcriber_instance
    app.state.model_ready = transcriber_instance.model_loaded

    # GRPC server'ı güvenli şekilde başlat
    grpc_server = None
    if app.state.model_ready:
        grpc_server = await setup_grpc_server(transcriber_instance)
    else:
        logger.error("❌ Model yüklenemediği için GRPC sunucusu başlatılmıyor")
    
    yield
    
    if grpc_server:
        logger.info("GRPC sunucusu durduruluyor...")
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