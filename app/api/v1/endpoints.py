import io
import time
import librosa
import numpy as np
import structlog
from fastapi import APIRouter, File, UploadFile, HTTPException, Form, Request, status
from typing import Optional
from prometheus_client import Histogram

from app.core.config import settings

router = APIRouter()
logger = structlog.get_logger(__name__)

# Prometheus metrikleri için Histogram nesneleri
TRANSCRIPTION_DURATION = Histogram('transcription_duration_seconds', 'Transcription processing time in seconds.')
AUDIO_DURATION = Histogram('audio_duration_seconds', 'Duration of the audio file being transcribed in seconds.')

# app/api/v1/endpoints.py - Geliştirilmiş güvenlik kontrolleri
@router.post("/transcribe")
async def create_transcription(
    request: Request,
    file: UploadFile = File(..., description="Metne çevrilecek ses dosyası."),
    language: Optional[str] = Form(None, description="Sesin dili (örn: 'tr', 'en'). Otomatik algılar.")
):
    # Dosya boyutu kontrolü (max 50MB)
    MAX_FILE_SIZE = 50 * 1024 * 1024
    file.file.seek(0, 2)  # End of file
    file_size = file.file.tell()
    file.file.seek(0)  # Reset file pointer
    
    if file_size > MAX_FILE_SIZE:
        raise HTTPException(
            status_code=status.HTTP_413_REQUEST_ENTITY_TOO_LARGE,
            detail=f"Dosya boyutu 50MB'tan büyük olamaz. Mevcut boyut: {file_size} bytes"
        )
    
    # Dosya tipi kontrolü
    allowed_content_types = ['audio/', 'video/', 'application/octet-stream']
    if not any(file.content_type.startswith(prefix) for prefix in allowed_content_types):
        raise HTTPException(
            status_code=status.HTTP_415_UNSUPPORTED_MEDIA_TYPE,
            detail="Desteklenmeyen dosya formatı"
        )
        
    transcriber = request.app.state.transcriber
    if not transcriber or not transcriber.model_loaded:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE, 
            detail="Model henüz hazır değil. Lütfen bir süre sonra tekrar deneyin."
        )

    start_time = time.time()
    try:
        audio_bytes = await file.read()
        
        # librosa.get_duration sesin orijinal süresini byte'lardan hesaplar
        original_duration = librosa.get_duration(y=librosa.load(io.BytesIO(audio_bytes), sr=None)[0])
        AUDIO_DURATION.observe(original_duration)

        audio_array, _ = librosa.load(
            io.BytesIO(audio_bytes),
            sr=settings.STT_WHISPER_SERVICE_TARGET_SAMPLE_RATE,
            mono=True
        )
        audio_array = audio_array.astype(np.float32)

        logger.info("Transkripsiyon isteği alındı", filename=file.filename, language_hint=language)

        result = transcriber.transcribe(audio_data=audio_array, language=language)
        return result

    except Exception as e:
        logger.error("Transkripsiyon işlemi sırasında hata", error=str(e), exc_info=True)
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=str(e))
    finally:
        process_time = time.time() - start_time
        TRANSCRIPTION_DURATION.observe(process_time)
        logger.info("Transkripsiyon işlemi tamamlandı", duration=f"{process_time:.4f}s")