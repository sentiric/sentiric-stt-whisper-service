# sentiric-stt-whisper-service/app/main.py
from fastapi import FastAPI, Depends, HTTPException, status
from contextlib import asynccontextmanager
from app.core.logging import setup_logging
from app.core.config import settings
import structlog

logger = structlog.get_logger(__name__)

# NOT: Model yükleme işlemi yavaş olacağı için lifespan içinde yapılmalıdır.
@asynccontextmanager
async def lifespan(app: FastAPI):
    setup_logging()
    logger.info("STT Whisper Service başlatılıyor", 
                version=settings.SERVICE_VERSION, 
                env=settings.ENV,
                model=settings.WHISPER_MODEL_SIZE)
    
    # TODO: Model yükleme (Load model into memory)
    # try:
    #     global WHISPER_MODEL
    #     WHISPER_MODEL = load_whisper_model(...)
    #     logger.info("Whisper modeli başarıyla yüklendi.")
    # except Exception as e:
    #     logger.critical("Model yüklenemedi, servis başlatılamıyor.", error=str(e))
    #     raise e
    
    yield
    
    logger.info("STT Whisper Service kapatılıyor")

app = FastAPI(
    title="Sentiric STT Whisper Service",
    description="Faster-Whisper tabanlı uzman STT motoru",
    version=settings.SERVICE_VERSION,
    lifespan=lifespan
)

# RPC'ler burada tanımlanacak: WhisperTranscribe

@app.get("/health", status_code=status.HTTP_200_OK)
async def health_check():
    # Modelin yüklendiğinden emin olmak için daha sonra model kontrolü eklenecek
    # if WHISPER_MODEL is None:
    #    return {"status": "degraded", "detail": "Whisper model not loaded"}, status.HTTP_503_SERVICE_UNAVAILABLE
    
    return {"status": "ok", "service": "stt-whisper"}