import io
import librosa
import numpy as np
import structlog
from fastapi import APIRouter, File, UploadFile, HTTPException, Form, Request, status
from typing import Optional

from app.core.config import settings

router = APIRouter()
logger = structlog.get_logger(__name__)

@router.post("/transcribe", summary="Bir ses dosyasını metne çevirir")
async def create_transcription(
    request: Request,
    file: UploadFile = File(..., description="Metne çevrilecek ses dosyası."),
    language: Optional[str] = Form(None, description="Sesin dili (örn: 'tr', 'en'). Otomatik algılar.")
):
    transcriber = request.app.state.transcriber
    if not transcriber or not transcriber.model_loaded:
        raise HTTPException(
            status_code=status.HTTP_503_SERVICE_UNAVAILABLE, 
            detail="Model henüz hazır değil. Lütfen bir süre sonra tekrar deneyin."
        )

    try:
        audio_bytes = await file.read()
        audio_array, _ = librosa.load(
            io.BytesIO(audio_bytes),
            sr=settings.TARGET_SAMPLE_RATE,
            mono=True
        )
        audio_array = audio_array.astype(np.float32)

        logger.info(
            "Transkripsiyon isteği alındı",
            filename=file.filename,
            content_type=file.content_type,
            language_hint=language
        )

        result = transcriber.transcribe(audio_data=audio_array, language=language)
        return result

    except Exception as e:
        logger.error("Transkripsiyon işlemi sırasında hata", error=str(e), exc_info=True)
        raise HTTPException(status_code=status.HTTP_500_INTERNAL_SERVER_ERROR, detail=str(e))