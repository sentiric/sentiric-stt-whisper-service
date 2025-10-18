import structlog
import numpy as np
from typing import Optional
from faster_whisper import WhisperModel
from app.core.config import settings

logger = structlog.get_logger(__name__)

class WhisperTranscriber:
    def __init__(self):
        self.model: Optional[WhisperModel] = None
        self.model_loaded = False

    def load_model(self):
        if self.model_loaded:
            return
        try:
            logger.info(
                "Whisper modeli yükleniyor...",
                model_size=settings.WHISPER_MODEL_SIZE,
                device=settings.WHISPER_DEVICE,
                compute_type=settings.WHISPER_COMPUTE_TYPE
            )
            self.model = WhisperModel(
                settings.WHISPER_MODEL_SIZE,
                device=settings.WHISPER_DEVICE,
                compute_type=settings.WHISPER_COMPUTE_TYPE,
            )
            self.model_loaded = True
            logger.info("✅ Whisper modeli başarıyla yüklendi.")
        except Exception as e:
            self.model_loaded = False
            logger.error("❌ Whisper modeli yüklenemedi", error=str(e), exc_info=True)
            raise

    def transcribe(self, audio_data: np.ndarray, language: Optional[str] = None) -> dict:
        if not self.model:
            raise RuntimeError("Model yüklenmemiş veya başlatma başarısız olmuş.")
        
        try:
            segments, info = self.model.transcribe(
                audio_data,
                language=language,
                beam_size=5,
                log_prob_threshold=settings.WHISPER_LOGPROB_THRESHOLD,
                no_speech_threshold=settings.WHISPER_NO_SPEECH_THRESHOLD
            )
            
            full_text = " ".join(segment.text for segment in segments)
            
            logger.info(
                "Transkripsiyon tamamlandı",
                detected_language=info.language,
                lang_probability=round(info.language_probability, 2),
                duration_seconds=round(info.duration, 2)
            )
            
            return {
                "text": full_text.strip(),
                "language": info.language,
                "language_probability": info.language_probability,
                "duration": info.duration
            }
        except Exception as e:
            logger.error("Transkripsiyon sırasında model hatası", error=str(e), exc_info=True)
            raise