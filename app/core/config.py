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

    # --- Whisper Model Ayarları ---
    WHISPER_MODEL_SIZE: str = "medium"
    # "auto", GPU varsa CUDA'yı, yoksa CPU'yu kullanmasını sağlar.
    WHISPER_DEVICE: str = "auto" 
    WHISPER_COMPUTE_TYPE: str = "int8"
    TARGET_SAMPLE_RATE: int = 16000
    
    # --- Whisper Filtreleme Ayarları ---
    WHISPER_LOGPROB_THRESHOLD: float = -1.0
    WHISPER_NO_SPEECH_THRESHOLD: float = 0.6    
    
    # --- API Ayarları ---
    API_PORT: int = 15030
    GRPC_PORT: int = 15031
    METRICS_PORT: int = 15032

    # Çalışma zamanında GPU'nun varlığını kontrol et
    def get_device(self) -> str:
        if self.WHISPER_DEVICE == "auto":
            return "cuda" if torch.cuda.is_available() else "cpu"
        return self.WHISPER_DEVICE

    model_config = SettingsConfigDict(
        env_file=".env",
        case_sensitive=False,
        extra='ignore'
    )

settings = Settings()