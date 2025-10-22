## 🚨 CRITICAL - CUDA & DEPENDENCY FIXES

### 📦 CUDA 12.1 + cuDNN 8 UYUMLULUK
**BU SERVİS KESİNLİKLE CUDA 12.1 + cuDNN 8 RUNTIME KULLANMALIDIR!**

### ✅ ÇALIŞAN VERSİYONLAR (2025-01-20 CUDA FIX)
```txt
# Core Framework
fastapi==0.104.1
uvicorn[standard]==0.24.0
httpx==0.27.0

# STT Engine - CUDA 12.1  
faster-whisper==1.0.2
torch==2.3.0+cu121  # ✅ CUDA 12.1 UYUMLU

# Audio Processing
librosa==0.10.1
soundfile==0.12.1
pydub==0.25.1
numpy==1.26.4

# Configuration & Logging
pydantic-settings==2.4.0
pydantic==2.12.3
structlog==23.3.0

# Monitoring
prometheus-client==0.20.0
prometheus-fastapi-instrumentator==6.0.0

# gRPC & Contracts
grpcio>=1.62.0
protobuf>=5.26.1,<6.30.0  # ✅ 6.33.0 YASAK!
sentiric-contracts-py@git+https://github.com/sentiric/sentiric-contracts.git@v1.9.3
```

### 🐳 DOCKER BASE IMAGE FIX
```dockerfile
# ✅ DOĞRU: cuDNN runtime içeren image
FROM nvidia/cuda:12.1.1-cudnn8-runtime-ubuntu22.04

# ❌ YANLIŞ: cuDNN olmayan image  
FROM nvidia/cuda:12.1.1-base-ubuntu22.04
```

### ❌ YAPILMAYACAKLAR
- `protobuf==6.33.0` → **YASAK!** Range kullan: `<6.30.0`
- `cuda:12.1.1-base` → **cuDNN EKSİK!** `cuda:12.1.1-cudnn8-runtime` kullan
- `torch` CUDA olmadan → **libcudnn HATASI!**
