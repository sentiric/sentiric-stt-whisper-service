# 🤫 Sentiric STT Whisper Service - Bağımlılık Yönetimi ve Kritik Notlar

Bu doküman, bu servisin hassas bağımlılıklarını, karşılaşılan sorunları ve uygulanan çözümleri özetler. Projeyi kurarken veya güncellerken bu doküman birincil referans olmalıdır.

---

## 1. Altın Kural: CUDA ve cuDNN Uyumluluğu

Bu servis, **NVIDIA CUDA 12.1** ve **cuDNN 8** üzerine inşa edilmiştir. Tüm AI/ML bağımlılıkları bu temel üzerine seçilmiştir.

*   **Docker Base Image:** `nvidia/cuda:12.1.1-cudnn8-runtime-ubuntu22.04`
*   **PyTorch Sürümü:** `torch==2.3.0` (`cu121` ekstra index'i ile)

Bu iki temel bileşenin değiştirilmesi, tüm bağımlılık zincirinin yeniden değerlendirilmesini gerektirir.

---

## 2. Onaylanmış ve Çalışan Bağımlılık Listesi (`requirements.gpu.txt` - 2025-10-22)

Aşağıdaki liste, projenin hem REST hem de gRPC arayüzleriyle stabil çalıştığı **kanıtlanmış** versiyonları içerir.

```txt
# === SENTIRIC STT SERVICE - GPU OPTIMIZED (CUDA 12.1 / cuDNN 8) - FINAL ===
# Core Framework
fastapi==0.104.1
uvicorn[standard]==0.24.0
httpx==0.27.0

# STT Engine - CUDA 12.1 / cuDNN 8 uyumlu
--extra-index-url https://download.pytorch.org/whl/cu121
torch==2.3.0
torchvision==0.18.0
torchaudio==2.3.0
ctranslate2==4.3.1
faster-whisper==1.0.3

# Audio Processing
librosa==0.10.1
# ... diğerleri

# gRPC & Contracts - Protobuf v4 ile uyumlu
grpcio>=1.62.0
protobuf==4.25.3
sentiric-contracts-py@git+https://github.com/sentiric/sentiric-contracts.git@v1.9.3