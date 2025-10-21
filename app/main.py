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

def add_custom_instrumentator(app: FastAPI):
    """Prometheus metriklerini ekle"""
    instrumentator = Instrumentator()
    
    # Custom metric'leri ekle
    app_info_gauge = Gauge(
        "app_info",
        "Application information",
        ["service_version", "model_size", "device"]
    )
    
    # Metric değerlerini set et
    app_info_gauge.labels(
        service_version=settings.SERVICE_VERSION,
        model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
        device=settings.STT_WHISPER_SERVICE_DEVICE
    ).set(1)
    
    # Instrumentator'u uygula
    instrumentator.instrument(app)
    
    # Metrics endpoint'ini ayrıca ekle
    @app.get("/metrics", include_in_schema=False)
    async def metrics():
        from prometheus_client import generate_latest, CONTENT_TYPE_LATEST
        from fastapi.responses import Response
        return Response(
            content=generate_latest(),
            media_type=CONTENT_TYPE_LATEST
        )
    
    return instrumentator

async def setup_grpc_server(transcriber):
    """GRPC server'ı güvenli şekilde başlat - PROTOBUF SORUNU ÇÖZÜLDÜ"""
    try:
        # Protobuf versiyon kontrolü
        import google.protobuf
        logger.info(f"Protobuf version: {google.protobuf.__version__}")
        
        from app.services.grpc_server import serve as serve_grpc
        grpc_server = await serve_grpc(transcriber)
        
        if grpc_server:
            logger.info("✅ GRPC server başarıyla başlatıldı")
            return grpc_server
        else:
            logger.warning("⚠️ GRPC server None döndü")
            return None
            
    except Exception as e:
        logger.error("❌ GRPC server başlatılamadı", error=str(e), exc_info=True)
        return None

@asynccontextmanager
async def lifespan(app: FastAPI):
    """Uygulama ömrü yönetimi - GRPC ile birlikte"""
    setup_logging()
    logger.info(
        "🚀 STT Whisper Service başlatılıyor...",
        service_version=settings.SERVICE_VERSION,
        model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE
    )
    
    # Model yükleme işlemi
    transcriber_instance = WhisperTranscriber()
    app.state.model_ready = False
    app.state.grpc_server = None
    
    try:
        # Model yükleme için timeout ile async çalıştır
        logger.info("Whisper modeli yükleniyor...")
        await asyncio.wait_for(
            asyncio.get_event_loop().run_in_executor(
                None, transcriber_instance.load_model
            ), 
            timeout=600.0  # 10 dakika timeout
        )
        
        # Model başarıyla yüklendi
        app.state.transcriber = transcriber_instance
        app.state.model_ready = transcriber_instance.model_loaded
        
        if app.state.model_ready:
            logger.info(
                "✅ STT Whisper Service hazır",
                model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
                device=transcriber_instance.device
            )
            
            # GRPC server'ı başlat - MODEL HAZIRSA
            app.state.grpc_server = await setup_grpc_server(transcriber_instance)
            
        else:
            logger.error("❌ Model yüklenemedi")
            
    except asyncio.TimeoutError:
        logger.error("⏰ Model yükleme zaman aşımına uğradı")
        app.state.transcriber = transcriber_instance
        app.state.model_ready = False
    except Exception as e:
        logger.error("❌ Model yükleme hatası", error=str(e))
        app.state.transcriber = transcriber_instance
        app.state.model_ready = False
    
    yield  # Uygulama çalışıyor
    
    # Shutdown işlemleri - GRPC server'ı durdur
    logger.info("🛑 STT Whisper Service durduruluyor...")
    if app.state.grpc_server:
        logger.info("GRPC sunucusu durduruluyor...")
        await app.state.grpc_server.stop(grace=1)

# FastAPI uygulaması oluştur
app = FastAPI(
    title=settings.PROJECT_NAME,
    version=settings.SERVICE_VERSION,
    description="Yüksek performanslı Whisper tabanlı Konuşma Tanıma servisi",
    lifespan=lifespan
)

# Instrumentator'u ekle
instrumentator = add_custom_instrumentator(app)

# API router'larını ekle
app.include_router(api_router, prefix="/api/v1")

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
    """Kök endpoint"""
    return {
        "message": f"Welcome to {settings.PROJECT_NAME}",
        "version": settings.SERVICE_VERSION,
        "docs_url": "/docs",
        "health_url": "/health"
    }

@app.get("/info", tags=["Info"])
async def service_info():
    """Servis bilgileri endpoint'i"""
    return {
        "service": settings.PROJECT_NAME,
        "version": settings.SERVICE_VERSION,
        "model_size": settings.STT_WHISPER_SERVICE_MODEL_SIZE,
        "device": settings.STT_WHISPER_SERVICE_DEVICE,
        "environment": settings.ENV,
        "endpoints": {
            "transcribe": "/api/v1/transcribe",
            "health": "/health",
            "metrics": "/metrics",
            "docs": "/docs"
        },
        "features": {
            "rest_api": True,
            "grpc_api": True,
            "health_check": True,
            "metrics": True
        }
    }