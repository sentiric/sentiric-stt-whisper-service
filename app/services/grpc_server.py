# File: app/services/grpc_server.py
import asyncio
import io
import librosa
import numpy as np
import structlog
from concurrent import futures
from typing import AsyncIterator # <-- YENİ: Doğru tip tanımı buradan import edildi

# === PROTOBUF v4+ UYUMLULUK MONKEY PATCH ===
import google.protobuf
if not hasattr(google.protobuf, 'runtime_version'):
    from types import ModuleType, SimpleNamespace
    domain = SimpleNamespace()
    domain.PUBLIC = 0
    runtime_version_module = ModuleType('google.protobuf.runtime_version')
    setattr(runtime_version_module, 'Domain', domain)
    def ValidateProtobufRuntimeVersion(*args, **kwargs): pass
    runtime_version_module.ValidateProtobufRuntimeVersion = ValidateProtobufRuntimeVersion
    google.protobuf.runtime_version = runtime_version_module
# === PATCH SONU ===

import grpc
from sentiric.stt.v1 import whisper_pb2, whisper_pb2_grpc
from app.services.whisper_service import WhisperTranscriber
from app.core.config import settings

logger = structlog.get_logger(__name__)

class SttWhisperServiceServicer(whisper_pb2_grpc.SttWhisperServiceServicer):
    def __init__(self, transcriber: WhisperTranscriber):
        self.transcriber = transcriber
        logger.info("gRPC Servicer for Whisper initialized.")

    async def WhisperTranscribe(self, request: whisper_pb2.WhisperTranscribeRequest, context: grpc.aio.ServicerContext):
        logger.info("gRPC WhisperTranscribe isteği alındı.", language_hint=request.language)
        if not self.transcriber or not self.transcriber.model_loaded:
            await context.abort(grpc.StatusCode.UNAVAILABLE, "Model henüz hazır değil.")
        
        try:
            audio_array, _ = librosa.load(
                io.BytesIO(request.audio_data),
                sr=settings.STT_WHISPER_SERVICE_TARGET_SAMPLE_RATE,
                mono=True
            )
            audio_array = audio_array.astype(np.float32)

            result = self.transcriber.transcribe(
                audio_data=audio_array,
                language=request.language if request.language else None
            )

            return whisper_pb2.WhisperTranscribeResponse(
                transcription=result["text"],
                language=result["language"],
                language_probability=result["language_probability"],
                duration=result["duration"]
            )
        except Exception as e:
            logger.error("gRPC transkripsiyon hatası", error=str(e), exc_info=True)
            await context.abort(grpc.StatusCode.INTERNAL, "Transkripsiyon sırasında bir hata oluştu.")

    # --- DÜZELTME BURADA ---
    async def WhisperTranscribeStream(self, request_iterator: AsyncIterator[whisper_pb2.WhisperTranscribeStreamRequest], context: grpc.aio.ServicerContext):
        if not self.transcriber or not self.transcriber.model_loaded:
            await context.abort(grpc.StatusCode.UNAVAILABLE, "Model henüz hazır değil.")

        logger.info("gRPC streaming isteği başlatıldı.")

        async def audio_chunk_generator():
            async for req in request_iterator:
                audio_float = np.frombuffer(req.audio_chunk, dtype=np.int16).astype(np.float32) / 32768.0
                yield audio_float
        
        try:
            # faster-whisper'ın generator'ları doğrudan kabul etme yeteneğini kullanıyoruz
            segments, info = self.transcriber.model.transcribe(
                audio_chunk_generator(),
                beam_size=settings.STT_WHISPER_SERVICE_BEAM_SIZE,
                vad_filter=True,
                vad_parameters=dict(min_silence_duration_ms=700) # Daha iyi segmentasyon için VAD ayarı
            )

            # Segmentleri asenkron olarak işlemek için bir yardımcı fonksiyon
            async def process_segments(segments_iterator):
                for segment in segments_iterator:
                    yield segment
            
            async for segment in process_segments(segments):
                logger.debug("Stream segmenti üretildi", text=segment.text.strip())
                yield whisper_pb2.WhisperTranscribeStreamResponse(
                    transcription=segment.text.strip(),
                    is_final=True # faster-whisper stream'i segment bazlı çalışır, her segment final'dır.
                )
        except Exception as e:
            logger.error("gRPC stream işlenirken hata.", error=str(e), exc_info=True)
            await context.abort(grpc.StatusCode.INTERNAL, "Stream işlenirken hata oluştu.")
        finally:
            logger.info("gRPC streaming isteği tamamlandı.")


async def serve(transcriber: WhisperTranscriber) -> grpc.aio.Server:
    server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=10))
    whisper_pb2_grpc.add_SttWhisperServiceServicer_to_server(
        SttWhisperServiceServicer(transcriber), server
    )
    listen_addr = f'[::]:{settings.STT_WHISPER_SERVICE_GRPC_PORT}'
    server.add_insecure_port(listen_addr)
    logger.info(f"gRPC sunucusu başlatılıyor...", address=listen_addr)
    await server.start()
    return server