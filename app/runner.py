# File: app/runner.py
import asyncio
import uvicorn
import structlog
from app.main import app
from app.services.grpc_server import serve
from app.core.config import settings

logger = structlog.get_logger(__name__)

async def main():
    """
    Uvicorn (HTTP) ve gRPC sunucularını aynı anda asenkron olarak çalıştırır.
    """
    # Lifespan manager'ın çalışabilmesi için app'i bu şekilde başlatıyoruz.
    uvicorn_config = uvicorn.Config(
        "app.main:app", 
        host="0.0.0.0", 
        port=settings.STT_WHISPER_SERVICE_HTTP_PORT,
        log_config=None,
        access_log=False
    )
    server = uvicorn.Server(uvicorn_config)
    
    # Uvicorn server'ı ve gRPC server'ı (lifespan içinde başlatılan) paralel olarak yönet
    await server.serve()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Sunucular kapatılıyor.")