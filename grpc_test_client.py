# File: grpc_test_client.py
# Bu test istemcisi, bir ses dosyasını okur ve gRPC stream üzerinden gönderir.
# Kullanım: python3 grpc_test_client.py /path/to/your/audio.wav
import asyncio
import grpc
import sys
import wave

# Protobuf uyumluluğu için monkey patch
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

from sentiric.stt.v1 import whisper_pb2, whisper_pb2_grpc

SERVER_ADDRESS = 'localhost:15031'
CHUNK_SIZE = 8000  # 16kHz, 16-bit PCM için 0.25 saniyelik parça

async def audio_stream_generator(file_path: str):
    """Bir WAV dosyasını okur ve chunk'lar halinde yield eder."""
    try:
        with wave.open(file_path, 'rb') as wf:
            if wf.getframerate() != 16000 or wf.getsampwidth() != 2 or wf.getnchannels() != 1:
                print(f"HATA: Ses dosyası 16kHz, 16-bit, mono PCM formatında olmalıdır. Mevcut: {wf.getframerate()}Hz, {wf.getsampwidth()*8}-bit, {wf.getnchannels()} kanal")
                return
            
            print(f"🎤 '{file_path}' dosyası okunuyor ve stream ediliyor...")
            while True:
                data = wf.readframes(CHUNK_SIZE)
                if not data:
                    break
                yield whisper_pb2.WhisperTranscribeStreamRequest(audio_chunk=data)
                await asyncio.sleep(0.1) # Gerçek zamanlı akışı simüle etmek için küçük bir bekleme
    except FileNotFoundError:
        print(f"HATA: Ses dosyası bulunamadı -> {file_path}")
    except Exception as e:
        print(f"HATA: Ses dosyası okunurken hata oluştu: {e}")

async def run(file_path: str):
    async with grpc.aio.insecure_channel(SERVER_ADDRESS) as channel:
        stub = whisper_pb2_grpc.SttWhisperServiceStub(channel)
        print("🎧 Sunucudan transkripsiyon bekleniyor...")
        
        try:
            async for response in stub.WhisperTranscribeStream(audio_stream_generator(file_path)):
                end_char = '\n' if response.is_final else '\r'
                print(f"   ↳ [Transkript]: {response.transcription.strip()}", end=end_char, flush=True)
            print("\nStream tamamlandı.")
        except grpc.aio.AioRpcError as e:
            print(f"\n❌ gRPC Hatası: Sunucuya bağlanılamadı veya stream sırasında hata oluştu.")
            print(f"   Detay: {e.details()}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Kullanım: python3 grpc_test_client.py <wav_dosyasi_yolu>")
        sys.exit(1)
    
    audio_file = sys.argv[1]
    
    try:
        asyncio.run(run(audio_file))
    except KeyboardInterrupt:
        print("\nProgramdan çıkılıyor.")