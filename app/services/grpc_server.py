# =====================================================================
# MONKEY PATCH FOR PROTOBUF v4+ COMPATIBILITY v2
# The generated contract expects 'google.protobuf.runtime_version.Domain'.
# This object was part of the internal API in older protobuf versions.
# We simulate this object and its 'PUBLIC' attribute to satisfy the import.
import google.protobuf
if not hasattr(google.protobuf, 'runtime_version'):
    from types import ModuleType, SimpleNamespace
    
    # Sahte 'Domain' nesnesini oluştur
    domain = SimpleNamespace()
    domain.PUBLIC = 0  # Genellikle 0 veya 1 gibi bir enum değeridir
    
    # Sahte 'runtime_version' modülünü oluştur
    runtime_version_module = ModuleType('google.protobuf.runtime_version')
    
    # Sahte modülün içine sahte 'Domain' nesnesini ekle
    setattr(runtime_version_module, 'Domain', domain)
    
    # Eski Validate fonksiyonunu da ekle (boş olsa da)
    def ValidateProtobufRuntimeVersion(*args, **kwargs):
        pass
    runtime_version_module.ValidateProtobufRuntimeVersion = ValidateProtobufRuntimeVersion
    
    # Ana protobuf modülüne sahte modülü bağla
    google.protobuf.runtime_version = runtime_version_module
# END OF MONKEY PATCH
# =====================================================================

import structlog
import numpy as np
import librosa
import io

from app.core.config import settings
from app.services.whisper_service import WhisperTranscriber

logger = structlog.get_logger(__name__)

async def serve(transcriber: WhisperTranscriber):
    """GRPC server'ı başlat - PROTOBUF VERSİYON SORUNU ÇÖZÜLDÜ"""
    try:
        # GRPC ve protobuf import'ları
        import grpc
        from concurrent import futures
        
        # Sentiric Contracts import'u - PROTOBUF VERSİYON UYUMLU
        from sentiric.stt.v1 import whisper_pb2
        from sentiric.stt.v1 import whisper_pb2_grpc
        
        logger.info("GRPC servisi başlatılıyor...")

        class SttWhisperServiceServicer(whisper_pb2_grpc.SttWhisperServiceServicer):
            def __init__(self, transcriber: WhisperTranscriber):
                self.transcriber = transcriber
                logger.info("GRPC Servicer başlatıldı.")

            async def WhisperTranscribe(
                self, request: whisper_pb2.WhisperTranscribeRequest, 
                context: grpc.aio.ServicerContext
            ) -> whisper_pb2.WhisperTranscribeResponse:
                
                logger.info("GRPC WhisperTranscribe isteği alındı.", language_hint=request.language)

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

                    return whisper_pb2.WhisperTranscribeResponse(
                        transcription=result["text"],
                        language=result["language"],
                        language_probability=result["language_probability"],
                        duration=result["duration"]
                    )

                except Exception as e:
                    logger.error("GRPC transkripsiyon hatası", error=str(e), exc_info=True)
                    await context.abort(grpc.StatusCode.INTERNAL, "Transkripsiyon sırasında bir hata oluştu.")

        # Server oluştur ve başlat
        server = grpc.aio.server(futures.ThreadPoolExecutor(max_workers=10))
        whisper_pb2_grpc.add_SttWhisperServiceServicer_to_server(
            SttWhisperServiceServicer(transcriber), server
        )
        
        listen_addr = f"[::]:{settings.STT_WHISPER_SERVICE_GRPC_PORT}"
        server.add_insecure_port(listen_addr)
        await server.start()
        
        logger.info(f"✅ GRPC sunucusu başlatıldı ve dinleniyor: {listen_addr}")
        return server
        
    except Exception as e:
        logger.error("❌ GRPC server başlatılamadı", error=str(e), exc_info=True)
        return None