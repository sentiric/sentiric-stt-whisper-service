import grpc
import asyncio
import structlog
import numpy as np
import librosa
import io

from concurrent import futures
from app.core.config import settings
from app.services.whisper_service import WhisperTranscriber

# Sentiric Contracts'tan gelen derlenmiş Python kodlarını import et
from sentiric.stt.v1 import whisper_pb2
from sentiric.stt.v1 import whisper_pb2_grpc

logger = structlog.get_logger(__name__)

class SttWhisperServiceServicer(whisper_pb2_grpc.SttWhisperServiceServicer):
    def __init__(self, transcriber: WhisperTranscriber):
        self.transcriber = transcriber
        logger.info("gRPC Servicer başlatıldı.")

    async def WhisperTranscribe(
        self, request: whisper_pb2.WhisperTranscribeRequest, context: grpc.aio.ServicerContext
    ) -> whisper_pb2.WhisperTranscribeResponse:
        
        logger.info("gRPC WhisperTranscribe isteği alındı.", language_hint=request.language)

        if not self.transcriber or not self.transcriber.model_loaded:
            await context.abort(grpc.StatusCode.UNAVAILABLE, "Model henüz hazır değil.")
        
        try:
            # Gelen byte'ları librosa ile standart formata çevir
            audio_array, _ = librosa.load(
                io.BytesIO(request.audio_data),
                sr=settings.STT_WHISPER_SERVICE_TARGET_SAMPLE_RATE,
                mono=True
            )
            audio_array = audio_array.astype(np.float32)

            # Transkripsiyon işlemini gerçekleştir
            result = self.transcriber.transcribe(
                audio_data=audio_array,
                language=request.language if request.language else None
            )

            return whisper_pb2.WhisperTranscribeResponse(transcription=result["text"])

        except Exception as e:
            logger.error("gRPC transkripsiyon hatası", error=str(e), exc_info=True)
            await context.abort(grpc.StatusCode.INTERNAL, "Transkripsiyon sırasında bir hata oluştu.")


async def serve(transcriber: WhisperTranscriber) -> grpc.aio.Server:
    server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=10))
    whisper_pb2_grpc.add_SttWhisperServiceServicer_to_server(
        SttWhisperServiceServicer(transcriber), server
    )
    listen_addr = f"[::]:{settings.STT_WHISPER_SERVICE_GRPC_PORT}"
    server.add_insecure_port(listen_addr)
    await server.start()
    logger.info(f"gRPC sunucusu başlatıldı ve dinleniyor: {listen_addr}")
    return server