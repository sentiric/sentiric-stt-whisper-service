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
```

---

## 3. Karşılaşılan Kritik Sorunlar ve Çözümleri

### a. Sorun: `libcudnn_ops.so` Bulunamadı / `Empty reply from server`
*   **Neden:** `torch` versiyonunun, Docker imajındaki `cuDNN 8` yerine `cuDNN 9` beklemesi.
*   **Çözüm:** `faster-whisper` ve `ctranslate2` kütüphaneleri, `cuDNN 8` ile daha iyi uyumluluk sağlayan daha yeni versiyonlara güncellendi. Bu, `torch`'un doğru cuDNN fonksiyonlarını bulmasını sağladı.

### b. Sorun: `gRPC` Sunucusu Başlamıyor (`Mismatched Protobuf Versions` veya `ImportError`)
*   **Neden:** `sentiric-contracts-py` paketi, `protobuf` kütüphanesinin eski bir versiyonu (`<4.0`) ile derlenmiş `protoc` tarafından oluşturulmuş. Bu, `protobuf 4.x` versiyonlarında kaldırılan `runtime_version` ve `Domain` gibi iç API'lere erişmeye çalışmasına neden oluyordu.
*   **Çözüm:** `protobuf` versiyonunu `4.25.3`'e yükselttik ve `app/services/grpc_server.py` dosyasının başına, `sentiric-contracts-py`'nin beklediği eski API'leri taklit eden bir **"Monkey Patch"** eklendi. Bu, kontrat reposunu değiştirmeden sorunu çözmemizi sağladı.

### c. Sorun: `numba` Cache Hatası (`cannot cache function`)
*   **Neden:** `librosa`'nın kullandığı `numba` kütüphanesi, Docker içinde JIT derlenmiş fonksiyonları önbelleğe almaya çalışırken hata veriyordu.
*   **Çözüm:** Dockerfile'lara `ENV NUMBA_CACHE_DIR=/tmp` ortam değişkeni eklendi. Bu, `numba`'yı kalıcı önbellek yerine geçici bir dizin kullanmaya zorlayarak sorunu çözdü.

### d. Sorun: Docker Build Hataları (`bad interpreter`, `git not found`, `tzdata prompt`)
*   **Neden:** Multi-stage build'lerde `venv`'in taşınabilir olmaması, `git`'in `builder` aşamasında eksik olması ve `apt`'nin interaktif girdi beklemesi.
*   **Çözüm:**
    1.  `venv` kopyalamak yerine, Python paketleri `pip wheel` ile `builder`'da indirilip `final` imajda kuruldu.
    2.  `git` paketi `builder` aşamasına eklendi.
    3.  `ENV DEBIAN_FRONTEND=noninteractive` değişkeni ile `apt`'nin interaktif modda çalışması engellendi.

---

