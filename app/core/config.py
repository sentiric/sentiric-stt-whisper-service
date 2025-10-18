from pydantic_settings import BaseSettings, SettingsConfigDict
from typing import Optional

class Settings(BaseSettings):
    # Servis Kimliği
    PROJECT_NAME: str = "Sentiric STT Whisper Service"
    SERVICE_VERSION: str = "1.0.0"
    
    # Whisper Model Ayarları
    WHISPER_MODEL_SIZE: str = "medium"
    WHISPER_DEVICE: str = "cpu"  # "cuda" for GPU
    WHISPER_COMPUTE_TYPE: str = "int8"
    
    # API Ayarları
    API_HOST: str = "0.0.0.0"
    API_PORT: int = 15030
    
    # Performans Ayarları
    TARGET_SAMPLE_RATE: int = 16000
    
    model_config = SettingsConfigDict(
        env_file=".env",
        case_sensitive=False,
        extra='ignore'
    )

settings = Settings()