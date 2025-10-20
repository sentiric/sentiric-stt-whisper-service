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

# Numba cache sorununu önlemek için librosa importundan önce environment ayarı
import os
os.environ['NUMBA_CACHE_DIR'] = '/tmp/numba_cache'

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
        
        # Librosa get_duration yerine alternatif süre hesaplama
        try:
            # Wave dosyası ise frame sayısından süre hesapla
            import wave
            with wave.open(io.BytesIO(audio_bytes)) as wav_file:
                frames = wav_file.getnframes()
                rate = wav_file.getframerate()
                original_duration = frames / float(rate)
        except:
            # Diğer formatlar için librosa kullan (cache sorunu olmaz)
            audio_for_duration, _ = librosa.load(
                io.BytesIO(audio_bytes),
                sr=None,
                mono=True
            )
            original_duration = len(audio_for_duration) / librosa.get_samplerate(io.BytesIO(audio_bytes))
        
        AUDIO_DURATION.observe(original_duration)

        # Asıl transkripsiyon için audio processing
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