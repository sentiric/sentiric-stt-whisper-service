from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field
import torch # GPU kontrolü için

class Settings(BaseSettings):
    PROJECT_NAME: str = "Sentiric STT Whisper Service"
    API_V1_STR: str = "/api/v1"
    ENV: str = "production"
    LOG_LEVEL: str = "INFO"

    SERVICE_VERSION: str = Field("0.0.0", validation_alias="SERVICE_VERSION")
    GIT_COMMIT: str = Field("unknown", validation_alias="GIT_COMMIT")
    BUILD_DATE: str = Field("unknown", validation_alias="BUILD_DATE")

    # --- API Ayarları ---
    STT_WHISPER_SERVICE_HTTP_PORT: int = 15030
    STT_WHISPER_SERVICE_GRPC_PORT: int = 15031
    STT_WHISPER_SERVICE_METRICS_PORT: int = 15032
    
    # --- Whisper Model Ayarları ---
    STT_WHISPER_SERVICE_MODEL_SIZE: str = "medium"
    # "auto", GPU varsa CUDA'yı, yoksa CPU'yu kullanmasını sağlar.
    STT_WHISPER_SERVICE_DEVICE: str = "auto" 
    STT_WHISPER_SERVICE_COMPUTE_TYPE: str = "int8"
    STT_WHISPER_SERVICE_TARGET_SAMPLE_RATE: int = 16000
    
    # --- Whisper Filtreleme Ayarları ---
    STT_WHISPER_SERVICE_LOGPROB_THRESHOLD: float = -1.0
    STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD: float = 0.6    
    
    # Yeni model ayarları
    STT_WHISPER_SERVICE_BEAM_SIZE: int = 5
    STT_WHISPER_SERVICE_TEMPERATURE: float = 0.0
    STT_WHISPER_SERVICE_BEST_OF: int = 5
    
    # Yeni güvenlik ve performans ayarları
    STT_WHISPER_SERVICE_MAX_FILE_SIZE: int = 50 * 1024 * 1024  # 50MB
    STT_WHISPER_SERVICE_MODEL_LOAD_TIMEOUT: int = 300  # 5 dakika
    STT_WHISPER_SERVICE_REQUEST_TIMEOUT: int = 60  # 1 dakika

    # Cache ayarları
    STT_WHISPER_SERVICE_MODEL_CACHE_DIR: str = "/app/model-cache"
    
    def get_device(self) -> str:
        if self.STT_WHISPER_SERVICE_DEVICE == "auto":
            try:
                import torch
                return "cuda" if torch.cuda.is_available() else "cpu"
            except ImportError:
                return "cpu"
        return self.STT_WHISPER_SERVICE_DEVICE    

    # Çalışma zamanında GPU'nun varlığını kontrol et
    def get_device(self) -> str:
        if self.STT_WHISPER_SERVICE_DEVICE == "auto":
            return "cuda" if torch.cuda.is_available() else "cpu"
        return self.STT_WHISPER_SERVICE_DEVICE

    model_config = SettingsConfigDict(
        env_file=".env",
        case_sensitive=False,
        extra='ignore'
    )

settings = Settings()