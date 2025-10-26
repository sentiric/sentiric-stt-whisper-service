import asyncio
from contextlib import asynccontextmanager
import structlog
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse
from prometheus_fastapi_instrumentator import Instrumentator
from prometheus_client import Gauge, generate_latest, CONTENT_TYPE_LATEST
from fastapi.responses import Response

from app.core.config import settings
from app.core.logging import setup_logging
from app.api.v1.endpoints import router as api_router
from app.services.whisper_service import WhisperTranscriber
from app.services.grpc_server import serve as serve_grpc

logger = structlog.get_logger(__name__)

def add_custom_instrumentator(app: FastAPI):
    """Prometheus metriklerini ekle"""
    instrumentator = Instrumentator()
    
    app_info_gauge = Gauge(
        "app_info", "Application information",
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
        return Response(content=generate_latest(), media_type=CONTENT_TYPE_LATEST)
    
    return instrumentator

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Uygulama ömrü yönetimi - gRPC sunucusu ile birlikte"""
    setup_logging()
    logger.info(
        "🚀 STT Whisper Service başlatılıyor...",
        service_version=settings.SERVICE_VERSION,
        model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE
    )
    
    transcriber_instance = WhisperTranscriber()
    app.state.model_ready = False
    app.state.grpc_server = None
    
    model_load_task = asyncio.get_event_loop().run_in_executor(
        None, transcriber_instance.load_model
    )
    
    try:
        await asyncio.wait_for(model_load_task, timeout=settings.STT_WHISPER_SERVICE_MODEL_LOAD_TIMEOUT)
        
        app.state.transcriber = transcriber_instance
        app.state.model_ready = transcriber_instance.model_loaded
        
        if app.state.model_ready:
            logger.info("✅ Whisper modeli başarıyla yüklendi, gRPC sunucusu başlatılıyor...")
            app.state.grpc_server = await serve_grpc(transcriber_instance)
        else:
            logger.error("❌ Model yüklenemedi, servis hazır değil.")
            
    except asyncio.TimeoutError:
        logger.error("⏰ Model yükleme zaman aşımına uğradı.")
    except Exception as e:
        logger.error("❌ Başlatma sırasında kritik hata", error=str(e), exc_info=True)

    yield  # Uygulama çalışıyor
    
    logger.info("🛑 STT Whisper Service durduruluyor...")
    if app.state.grpc_server:
        logger.info("gRPC sunucusu durduruluyor...")
        await app.state.grpc_server.stop(grace=1)

app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    description="Yüksek performanslı Whisper tabanlı Konuşma Tanıma servisi",
    lifespan=lifespan
)

instrumentator = add_custom_instrumentator(app)
app.include_router(api_router, prefix="/api/v1")

# Diğer endpoint'ler (/health, /info, /) aynı kalır...
@app.get("/health", tags=["Health"])
async def health_check(request: Request):
    """Sağlık kontrol endpoint'i"""
    is_ready = getattr(request.app.state, 'model_ready', False)
    grpc_ready = getattr(request.app.state, 'grpc_server', None) is not None
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
    
    # Model bilgileri
    transcriber = getattr(request.app.state, 'transcriber', None)
    model_info = {}
    if transcriber:
        model_info = {
            "model_loaded": transcriber.model_loaded,
            "device": getattr(transcriber, 'device', 'unknown')
        }
    
    return JSONResponse(
        status_code=status_code,
        content={
            "status": "healthy" if is_ready else "unhealthy",
            "model_ready": is_ready,
            "grpc_ready": grpc_ready,
            "gpu_available": gpu_available,
            "gpu_info": gpu_info,
            "service_version": settings.SERVICE_VERSION,
            "model_size": settings.STT_WHISPER_SERVICE_MODEL_SIZE,
            "model_info": model_info
        }
    )

@app.get("/", tags=["Root"])
async def root():
    return {"message": f"Welcome to {settings.PROJECT_NAME}", "version": settings.SERVICE_VERSION}