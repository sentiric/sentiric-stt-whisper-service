# app/services/whisper_service.py
import structlog
import numpy as np
from typing import Optional
from faster_whisper import WhisperModel
from app.core.config import settings

logger = structlog.get_logger(__name__)

class WhisperTranscriber:
    """
    Saf Whisper transkripsiyon motoru - Tek sorumluluk: Ses → Metin
    """
    
    def __init__(self):
        self.model = None
        self.model_loaded = False
        self._load_model()
    
    def _load_model(self):
        """Whisper modelini yükler - Sadece bir kere çalışır"""
        try:
            logger.info(
                "Loading Whisper model...",
                model_size=settings.WHISPER_MODEL_SIZE,
                device=settings.WHISPER_DEVICE
            )
            
            # --- DEĞİŞİKLİK BURADA ---
            # HF_HOME ortam değişkeninin kullanılabilmesi için bu satırı kaldırıyoruz.
            self.model = WhisperModel(
                settings.WHISPER_MODEL_SIZE,
                device=settings.WHISPER_DEVICE,
                compute_type=settings.WHISPER_COMPUTE_TYPE,
                # download_root="/tmp/whisper_models"  # <-- BU SATIR KALDIRILDI
            )
            # --- DEĞİŞİKLİK SONU ---
            
            self.model_loaded = True
            logger.info("✅ Whisper model successfully loaded")
            
        except Exception as e:
            logger.error("❌ Failed to load Whisper model", error=str(e))
            raise
    
    def transcribe(
        self,
        audio_data: np.ndarray,
        language: Optional[str] = None,
        task: str = "transcribe"
    ) -> dict:
        """
        Saf transkripsiyon işlemi - Girdi: ses array, Çıktı: metin
        
        Args:
            audio_data: 16kHz, mono PCM audio as numpy array
            language: "tr", "en", etc. None for auto-detect
            task: "transcribe" or "translate"
        """
        if not self.model_loaded:
            raise RuntimeError("Model not loaded")
        
        try:
            segments, info = self.model.transcribe(
                audio_data,
                language=language,
                task=task,
                beam_size=5,
                best_of=5,
                patience=1.0,
                length_penalty=1.0,
                repetition_penalty=1.0,
                no_speech_threshold=0.6,
                log_prob_threshold=-1.0,
                compression_ratio_threshold=2.4
            )
            
            full_text = " ".join(segment.text for segment in segments)
            
            logger.info(
                "Transcription completed",
                detected_language=getattr(info, 'language', 'unknown'),
                language_probability=getattr(info, 'language_probability', 0),
                text_length=len(full_text)
            )
            
            return {
                "text": full_text.strip(),
                "language": getattr(info, 'language', None),
                "language_probability": getattr(info, 'language_probability', 0),
                "duration": getattr(info, 'duration', 0)
            }
            
        except Exception as e:
            logger.error("Transcription failed", error=str(e))
            raise