# sentiric-stt-whisper-service/app/core/config.py
from pydantic_settings import BaseSettings, SettingsConfigDict
from typing import Optional

class Settings(BaseSettings):
    PROJECT_NAME: str = "Sentiric STT Whisper Service"
    API_V1_STR: str = "/api/v1"
    
    ENV: str = "production"
    LOG_LEVEL: str = "INFO"
    SERVICE_VERSION: str = "0.1.0"
    
    # Whisper Motoruna Özgü Ayarlar
    WHISPER_MODEL_SIZE: str = "medium"
    WHISPER_DEVICE: str = "cpu"
    WHISPER_COMPUTE_TYPE: str = "int8"
    
    # Streaming için kullanılacak Redis URL'si
    REDIS_URL: Optional[str] = None 

    model_config = SettingsConfigDict(
        env_file=".env", 
        env_file_encoding='utf-8',
        extra='ignore'
    )

settings = Settings()