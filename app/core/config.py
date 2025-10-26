from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field
import torch
import os

class Settings(BaseSettings):
    # Project Metadata
    PROJECT_NAME: str = "Sentiric STT Whisper Service"
    API_V1_STR: str = "/api/v1"
    ENV: str = "development"
    LOG_LEVEL: str = "INFO"

    # Build Information
    SERVICE_VERSION: str = Field("0.0.0", validation_alias="SERVICE_VERSION")
    GIT_COMMIT: str = Field("unknown", validation_alias="GIT_COMMIT")
    BUILD_DATE: str = Field("unknown", validation_alias="BUILD_DATE")

    # API Settings
    STT_WHISPER_SERVICE_HTTP_PORT: int = 15030
    STT_WHISPER_SERVICE_GRPC_PORT: int = 15031 # <-- YENİ EKLENDİ
    STT_WHISPER_SERVICE_METRICS_PORT: int = 15032
    
    # Whisper Model Settings
    STT_WHISPER_SERVICE_MODEL_SIZE: str = "medium"
    STT_WHISPER_SERVICE_DEVICE: str = "auto"
    STT_WHISPER_SERVICE_COMPUTE_TYPE: str = "int8"
    STT_WHISPER_SERVICE_TARGET_SAMPLE_RATE: int = 16000
    
    # Whisper Filtering Settings
    STT_WHISPER_SERVICE_LOGPROB_THRESHOLD: float = -1.0
    STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD: float = 0.6
    
    # Decoding Parameters
    STT_WHISPER_SERVICE_BEAM_SIZE: int = 5
    STT_WHISPER_SERVICE_TEMPERATURE: float = 0.0
    STT_WHISPER_SERVICE_BEST_OF: int = 5
    
    # Security & Performance
    STT_WHISPER_SERVICE_MAX_FILE_SIZE: int = 50 * 1024 * 1024  # 50MB
    STT_WHISPER_SERVICE_MODEL_LOAD_TIMEOUT: int = 300  # 5 dakika
    STT_WHISPER_SERVICE_REQUEST_TIMEOUT: int = 60  # 1 dakika

    # Cache Settings
    STT_WHISPER_SERVICE_MODEL_CACHE_DIR: str = "/app/model-cache"
    
    def get_device(self) -> str:
        """Otomatik cihaz seçimi (CUDA/CPU)"""
        if self.STT_WHISPER_SERVICE_DEVICE == "auto":
            try:
                return "cuda" if torch.cuda.is_available() else "cpu"
            except Exception:
                return "cpu"
        return self.STT_WHISPER_SERVICE_DEVICE

    def setup_environment(self):
        """Runtime environment optimizasyonları"""
        os.environ.update({
            'NUMBA_DEBUG': '0',
            'NUMBA_DISABLE_JIT': '0', 
            'PYTHONWARNINGS': 'ignore',
            'LIBROSA_CACHE_LEVEL': '0',
            'TF_CPP_MIN_LOG_LEVEL': '2',
            'HF_HUB_VERBOSITY': 'error'
        })

    model_config = SettingsConfigDict(
        env_file=".env",
        case_sensitive=False,
        extra='ignore'
    )

settings = Settings()
settings.setup_environment()