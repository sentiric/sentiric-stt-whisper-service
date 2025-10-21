## 🚨 CRITICAL - DEPENDENCY MANAGEMENT (LESSONS LEARNED)

### 📦 PROTOBUF VERSION STANDARD
**BU SERVİS KESİNLİKLE `protobuf>=5.26.1,<7.0.0` KULLANMALIDIR!**

### ✅ ÇALIŞAN VERSİYONLAR (2025-01-20)
```txt
# Core Framework
fastapi==0.104.1
uvicorn[standard]==0.24.0

# STT Engine  
faster-whisper==1.0.2
torch==2.3.0  # ✅ KRİTİK: faster-whisper ile BİRLİKTE kullanılmalı

# Audio Processing
librosa==0.10.1
soundfile==0.12.1
pydub==0.25.1
numpy==1.26.4

# Configuration & Logging
pydantic-settings==2.4.0
pydantic==2.12.3
structlog==23.3.0

# Monitoring (FASTAPI 0.104 İLE UYUMLU)
prometheus-client==0.20.0
prometheus-fastapi-instrumentator==6.0.0  # ✅ 7.0.0 DEĞİL!

# gRPC & Contracts
grpcio>=1.62.0
protobuf>=5.26.1,<7.0.0  # ✅ RANGE KULLAN - SABİT VERSİYON DEĞİL!
sentiric-contracts-py@git+https://github.com/sentiric/sentiric-contracts.git@v1.9.3
```

### ❌ YAPILMAYACAKLAR
- `protobuf==6.33.0` → **YASAK!** Range kullan
- `prometheus-fastapi-instrumentator==7.0.0` → FastAPI 0.104 ile uyumsuz
- `torch` olmadan `faster-whisper` kullanma → **CRASH!**
- `pydantic-settings` olmadan config kullanma → **IMPORT ERROR!**

### 🛠️ Teknoloji Yığını
*   **Dil:** Python 3.11
*   **Web Framework:** FastAPI
*   **RPC Framework:** gRPC  
*   **Çekirdek Kütüphane:** Faster-Whisper, CTranslate2, Torch
*   **Gözlemlenebilirlik:** Prometheus, Structlog
*   **Paket Yönetimi:** Setuptools (PEP 621) ✅ **POETRY DEĞİL!**