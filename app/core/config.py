from pydantic_settings import BaseSettings
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
    API_PORT: int = 15031
    
    # Performans Ayarları
    MAX_AUDIO_LENGTH: int = 600  # maksimum 10 dakika
    TARGET_SAMPLE_RATE: int = 16000
    
    class Config:
        env_file = ".env"
        case_sensitive = False

settings = Settings()