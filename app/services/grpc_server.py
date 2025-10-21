import structlog
from app.core.config import settings
from app.services.whisper_service import WhisperTranscriber

logger = structlog.get_logger(__name__)

async def serve(transcriber: WhisperTranscriber):
    """GRPC server'ı başlat - hata durumunda None döndür"""
    try:
        import grpc
        from concurrent import futures
        
        # Sentiric Contracts import'u
        from sentiric.stt.v1 import whisper_pb2
        from sentiric.stt.v1 import whisper_pb2_grpc
        
        class SttWhisperServiceServicer(whisper_pb2_grpc.SttWhisperServiceServicer):
            def __init__(self, transcriber: WhisperTranscriber):
                self.transcriber = transcriber
                logger.info("GRPC Servicer başlatıldı.")

            async def WhisperTranscribe(self, request, context):
                # Implementasyon buraya gelecek
                pass

        server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=10))
        whisper_pb2_grpc.add_SttWhisperServiceServicer_to_server(
            SttWhisperServiceServicer(transcriber), server
        )
        listen_addr = f"[::]:{settings.STT_WHISPER_SERVICE_GRPC_PORT}"
        server.add_insecure_port(listen_addr)
        await server.start()
        logger.info(f"GRPC sunucusu başlatıldı: {listen_addr}")
        return server
        
    except Exception as e:
        logger.error("GRPC server başlatılamadı", error=str(e))
        return None