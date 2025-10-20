import structlog
import numpy as np
from typing import Optional, Dict, Any
from faster_whisper import WhisperModel
from app.core.config import settings

logger = structlog.get_logger(__name__)

class WhisperTranscriber:
    def __init__(self):
        self.model: Optional[WhisperModel] = None
        self.model_loaded = False
        self.device = settings.get_device()

    def load_model(self):
        """Whisper modelini yükle"""
        if self.model_loaded:
            logger.info("Model already loaded")
            return
            
        try:
            logger.info(
                "Loading Whisper model...",
                model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
                target_device=self.device,
                compute_type=settings.STT_WHISPER_SERVICE_COMPUTE_TYPE,
                model_cache_dir=settings.STT_WHISPER_SERVICE_MODEL_CACHE_DIR
            )
            
            self.model = WhisperModel(
                model_size_or_path=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
                device=self.device,
                compute_type=settings.STT_WHISPER_SERVICE_COMPUTE_TYPE,
                download_root=settings.STT_WHISPER_SERVICE_MODEL_CACHE_DIR,
                num_workers=1  # Container ortamı için optimize
            )
            
            self.model_loaded = True
            logger.info(
                "✅ Whisper model loaded successfully", 
                on_device=self.device,
                model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE
            )
            
        except Exception as e:
            self.model_loaded = False
            logger.error(
                "❌ Failed to load Whisper model", 
                error=str(e), 
                exc_info=True
            )
            raise

    def transcribe(self, audio_data: np.ndarray, language: Optional[str] = None) -> Dict[str, Any]:
        """Ses verisini metne çevir"""
        if not self.model or not self.model_loaded:
            raise RuntimeError("Model is not loaded or initialization failed.")
        
        segments = None
        try:
            # Transkripsiyon parametreleri
            segments, info = self.model.transcribe(
                audio_data,
                language=language,
                beam_size=settings.STT_WHISPER_SERVICE_BEAM_SIZE,
                best_of=settings.STT_WHISPER_SERVICE_BEST_OF,
                temperature=settings.STT_WHISPER_SERVICE_TEMPERATURE,
                log_prob_threshold=settings.STT_WHISPER_SERVICE_LOGPROB_THRESHOLD,
                no_speech_threshold=settings.STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD,  # DÜZELTİLDİ
                vad_filter=True,  # Voice Activity Detection
                vad_parameters=dict(min_silence_duration_ms=500)
            )
            
            # Segmentleri birleştir
            segments_list = list(segments)
            full_text = " ".join(segment.text.strip() for segment in segments_list)
            
            logger.info(
                "Transcription completed",
                detected_language=info.language,
                language_probability=round(info.language_probability, 4),
                duration_seconds=round(info.duration, 2),
                segment_count=len(segments_list)
            )
            
            return {
                "text": full_text.strip(),
                "language": info.language,
                "language_probability": info.language_probability,
                "duration": info.duration,
                "segment_count": len(segments_list)
            }
            
        except Exception as e:
            logger.error(
                "Transcription model error", 
                error=str(e), 
                exc_info=True
            )
            raise
        finally:
            # Memory temizleme
            if segments:
                del segments