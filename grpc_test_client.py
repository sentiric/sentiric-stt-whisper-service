# pip install grpcio grpcio-tools sentiric-contracts-py@git+https://github.com/sentiric/sentiric-contracts.git@v1.9.3
# Örnek:
# python grpc_test_client.py /mnt/c/sentiric/sentiric-assets/audio/tr/welcome.wav tr

import grpc
import sys
from sentiric.stt.v1 import whisper_pb2, whisper_pb2_grpc

def run_transcribe_test(server_address, audio_file_path, language_code=None):
    """gRPC WhisperTranscribe metodunu test eder."""
    try:
        with open(audio_file_path, 'rb') as f:
            audio_bytes = f.read()
    except FileNotFoundError:
        print(f"HATA: Ses dosyası bulunamadı -> {audio_file_path}")
        return

    # Sunucuya güvensiz bir kanal oluştur (lokal test için)
    with grpc.insecure_channel(server_address) as channel:
        stub = whisper_pb2_grpc.SttWhisperServiceStub(channel)
        
        print(f"'{audio_file_path}' dosyası sunucuya gönderiliyor...")
        
        try:
            request = whisper_pb2.WhisperTranscribeRequest(
                audio_data=audio_bytes,
                language=language_code if language_code else ""
            )
            
            # RPC çağrısını yap
            response = stub.WhisperTranscribe(request, timeout=60) # 60 saniye timeout
            
            print("\n✅ BAŞARILI GRPC YANITI:")
            print(f"  Metin: '{response.transcription}'")
            # print(f"  Dil: {response.language} (Olasılık: {response.language_probability:.4f})")
            # print(f"  Süre: {response.duration:.2f}s")

        except grpc.RpcError as e:
            print(f"\n❌ GRPC HATA ALINDI:")
            print(f"  Durum Kodu: {e.code()}")
            print(f"  Detay: {e.details()}")

if __name__ == '__main__':
    # Varsayılan gRPC portu 15031'dir. docker-compose'da map etmeyi unutmayın.
    # docker-compose.dev.yml veya gpu.yml'e "- 15031:15031" ekleyin.
    GRPC_SERVER = 'localhost:15031' 
    
    if len(sys.argv) < 2:
        print("Kullanım: python grpc_test_client.py <ses_dosyasi_yolu> [dil_kodu]")
        sys.exit(1)
        
    audio_file = sys.argv[1]
    lang = sys.argv[2] if len(sys.argv) > 2 else None
    
    run_transcribe_test(GRPC_SERVER, audio_file, lang)