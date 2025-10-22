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
        self._setup_cuda_environment()

    def _setup_cuda_environment(self):
        """CUDA/cuDNN environment optimizasyonları"""
        import os
        import torch
        
        # CUDA environment ayarları
        os.environ.update({
            'CUDA_LAUNCH_BLOCKING': '1',
            'CUBLAS_WORKSPACE_CONFIG': ':4096:8',
            'CUDA_VISIBLE_DEVICES': '0',
            'TF_CPP_MIN_LOG_LEVEL': '2',
            'NUMBA_CUDA_CACHE_PATH': '/tmp/numba_cache'
        })
        
        # CUDA kullanılabilirliğini kontrol et
        if self.device == 'cuda':
            if not torch.cuda.is_available():
                logger.warning("CUDA not available, falling back to CPU")
                self.device = 'cpu'
            else:
                try:
                    # CUDA device bilgilerini logla
                    device_count = torch.cuda.device_count()
                    current_device = torch.cuda.current_device()
                    device_name = torch.cuda.get_device_name()
                    
                    logger.info(
                        "CUDA environment initialized",
                        device_count=device_count,
                        current_device=current_device,
                        device_name=device_name,
                        cuda_version=torch.version.cuda,
                        cudnn_version=torch.backends.cudnn.version() if torch.backends.cudnn.is_available() else "not available"
                    )
                    
                    # Memory ayarları
                    torch.backends.cudnn.benchmark = True
                    torch.backends.cuda.matmul.allow_tf32 = True
                    torch.backends.cudnn.allow_tf32 = True
                    
                except Exception as e:
                    logger.warning(f"CUDA initialization warning: {e}, falling back to CPU")
                    self.device = 'cpu'

    def load_model(self):
        """Whisper modelini yükle - CUDA/cuDNN optimizasyonu"""
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
            
            # Model parametreleri
            model_kwargs = {
                "model_size_or_path": settings.STT_WHISPER_SERVICE_MODEL_SIZE,
                "device": self.device,
                "compute_type": settings.STT_WHISPER_SERVICE_COMPUTE_TYPE,
                "download_root": settings.STT_WHISPER_SERVICE_MODEL_CACHE_DIR,
                "num_workers": 1,
                "cpu_threads": 2
            }
            
            # CUDA için ek optimizasyonlar
            if self.device == "cuda":
                model_kwargs.update({
                    "device_index": 0,
                    "local_files_only": False
                })
            
            self.model = WhisperModel(**model_kwargs)
            
            # Model yüklendikten sonra test transkripsiyonu
            if self.model_loaded:
                self._verify_model_operation()
            
            self.model_loaded = True
            logger.info(
                "✅ Whisper model loaded successfully", 
                on_device=self.device,
                model_size=settings.STT_WHISPER_SERVICE_MODEL_SIZE,
                compute_type=settings.STT_WHISPER_SERVICE_COMPUTE_TYPE
            )
            
        except Exception as e:
            self.model_loaded = False
            logger.error(
                "❌ Failed to load Whisper model", 
                error=str(e), 
                device=self.device,
                exc_info=True
            )
            raise

    def _verify_model_operation(self):
        """Model operasyonunu doğrula - test transkripsiyonu"""
        try:
            # Küçük bir test sesi oluştur
            test_audio = np.random.randn(16000).astype(np.float32) * 0.01  # Sessiz ses
            
            # Test transkripsiyonu
            segments, info = self.model.transcribe(
                test_audio,
                beam_size=1,
                best_of=1,
                temperature=0.0,
                vad_filter=False
            )
            
            # Sadece modelin çalıştığını doğrula, sonucu değil
            logger.debug(
                "Model operation verified",
                test_duration=info.duration,
                detected_language=info.language
            )
            
        except Exception as e:
            logger.warning(f"Model test transcription warning: {e}")

    def transcribe(self, audio_data: np.ndarray, language: Optional[str] = None) -> Dict[str, Any]:
        """Ses verisini metne çevir"""
        if not self.model or not self.model_loaded:
            raise RuntimeError("Model is not loaded or initialization failed.")
        
        segments = None
        try:
            # Transkripsiyon parametreleri
            transcribe_kwargs = {
                "beam_size": settings.STT_WHISPER_SERVICE_BEAM_SIZE,
                "best_of": settings.STT_WHISPER_SERVICE_BEST_OF,
                "temperature": settings.STT_WHISPER_SERVICE_TEMPERATURE,
                "log_prob_threshold": settings.STT_WHISPER_SERVICE_LOGPROB_THRESHOLD,
                "no_speech_threshold": settings.STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD,
                "vad_filter": True,
                "vad_parameters": dict(min_silence_duration_ms=500)
            }
            
            # Dil belirtilmişse ekle
            if language:
                transcribe_kwargs["language"] = language
            
            segments, info = self.model.transcribe(audio_data, **transcribe_kwargs)
            
            # Segmentleri birleştir
            segments_list = list(segments)
            full_text = " ".join(segment.text.strip() for segment in segments_list)
            
            logger.info(
                "Transcription completed",
                detected_language=info.language,
                language_probability=round(info.language_probability, 4),
                duration_seconds=round(info.duration, 2),
                segment_count=len(segments_list),
                device=self.device
            )
            
            return {
                "text": full_text.strip(),
                "language": info.language,
                "language_probability": info.language_probability,
                "duration": info.duration,
                "segment_count": len(segments_list),
                "device_used": self.device
            }
            
        except Exception as e:
            logger.error(
                "Transcription model error", 
                error=str(e),
                device=self.device,
                exc_info=True
            )
            raise
        finally:
            # Memory temizleme
            if segments:
                del segments
            if self.device == "cuda":
                import torch
                torch.cuda.empty_cache()