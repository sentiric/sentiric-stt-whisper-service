import io
import librosa
import numpy as np
import structlog
from fastapi import APIRouter, File, UploadFile, HTTPException, Form
from typing import Optional

from app.services.whisper_service import WhisperTranscriber
from app.core.config import settings

router = APIRouter()
logger = structlog.get_logger(__name__)

# Modelin başlangıçta bir kez yüklenmesi için tek bir nesne oluşturulur.
# main.py bu 'transcriber' nesnesini import edecek.
transcriber = WhisperTranscriber()

@router.post("/transcribe", summary="Transcribe audio to text")
async def create_transcription(
    file: UploadFile = File(..., description="Audio file to transcribe."),
    language: Optional[str] = Form(None, description="Language of the audio (e.g., 'tr', 'en'). Auto-detects if not provided.")
):
    """
    Ses dosyasını alır ve metin transkripsiyonunu döndürür.
    """
    if not transcriber.model_loaded:
        raise HTTPException(status_code=503, detail="Model is still loading or failed to load. Please try again in a moment.")

    try:
        # Ses dosyasının içeriğini byte olarak oku
        audio_bytes = await file.read()

        # Byte verisini librosa ile numpy dizisine çevir.
        # Bu, farklı formatlardaki (mp3, wav, flac vb.) ses dosyalarını destekler.
        audio_array, sampling_rate = librosa.load(
            io.BytesIO(audio_bytes),
            sr=settings.TARGET_SAMPLE_RATE,
            mono=True
        )
        
        # Whisper modelinin beklediği float32 formatına getir
        audio_array = audio_array.astype(np.float32)

        logger.info(
            "Received audio for transcription",
            filename=file.filename,
            content_type=file.content_type,
            language_hint=language
        )

        # Transkripsiyon işlemini gerçekleştir
        result = transcriber.transcribe(
            audio_data=audio_array,
            language=language
        )
        
        return result

    except Exception as e:
        logger.error("Error during transcription process", error=str(e), exc_info=True)
        raise HTTPException(status_code=500, detail=f"An internal error occurred during transcription: {str(e)}")