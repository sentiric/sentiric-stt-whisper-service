import structlog
import numpy as np
from typing import Optional
from faster_whisper import WhisperModel
from app.core.config import settings

logger = structlog.get_logger(__name__)

class WhisperTranscriber:
    """
    Saf Whisper transkripsiyon motoru. Tek sorumluluğu: Ses -> Metin.
    Modeli başlangıçta yükler ve gelen ses verilerini metne çevirir.
    """
    
    def __init__(self):
        self.model: Optional[WhisperModel] = None
        self.model_loaded = False
        self._load_model()
    
    def _load_model(self):
        """
        Whisper modelini `HF_HOME` ortam değişkenini kullanarak yükler.
        Bu, modelin kalıcı bir volume'de saklanmasını sağlar.
        """
        if self.model_loaded:
            return
        try:
            logger.info(
                "Loading Whisper model...",
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
            logger.info("✅ Whisper model successfully loaded.")
            
        except Exception as e:
            logger.error("❌ Failed to load Whisper model", error=str(e), exc_info=True)
            self.model_loaded = False
            raise
    
    def transcribe(self, audio_data: np.ndarray, language: Optional[str] = None) -> dict:
        """
        Saf transkripsiyon işlemi.
        
        Args:
            audio_data: 16kHz, mono, float32 PCM ses verisi (numpy array).
            language: "tr", "en" gibi dil kodu. None ise otomatik algılar.
        """
        if not self.model:
            raise RuntimeError("Model is not loaded or initialization failed.")
        
        try:
            segments, info = self.model.transcribe(
                audio_data,
                language=language,
                beam_size=5,
            )
            
            full_text = " ".join(segment.text for segment in segments)
            
            logger.info(
                "Transcription completed",
                detected_language=info.language,
                language_probability=round(info.language_probability, 2),
                duration_seconds=round(info.duration, 2)
            )
            
            return {
                "text": full_text.strip(),
                "language": info.language,
                "language_probability": info.language_probability,
                "duration": info.duration
            }
            
        except Exception as e:
            logger.error("Transcription failed during model execution", error=str(e), exc_info=True)
            raise