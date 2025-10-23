# 🤫 Sentiric STT Whisper Service - Bağımlılık Yönetimi ve Kritik Notlar

Bu doküman, bu servisin hassas bağımlılıklarını, karşılaşılan sorunları ve uygulanan çözümleri özetler. Projeyi kurarken veya güncellerken bu doküman birincil referans olmalıdır.

---

## 1. Altın Kural: CUDA ve cuDNN Uyumluluğu

Bu servis, **NVIDIA CUDA 12.1** ve **cuDNN 8** üzerine inşa edilmiştir. Tüm AI/ML bağımlılıkları bu temel üzerine seçilmiştir.

*   **Docker Base Image:** `nvidia/cuda:12.1.1-cudnn8-runtime-ubuntu22.04`
*   **PyTorch Sürümü:** `torch==2.3.0` (`cu121` ekstra index'i ile)

Bu iki temel bileşenin değiştirilmesi, tüm bağımlılık zincirinin yeniden değerlendirilmesini gerektirir.

---

## 2. Onaylanmış ve Çalışan Bağımlılık Listesi (`requirements.gpu.txt` - 2025-10-23)

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
*   **Neden:** `torch` versiyonunun, Docker imajındaki `cuDNN 8` yerine daha yeni bir `cuDNN` versiyonu beklemesi.
*   **Çözüm:** `faster-whisper` ve `ctranslate2` kütüphaneleri, `cuDNN 8` ile tam uyumluluk sağlayan güncel versiyonlara (`1.0.3` ve `4.3.1`) yükseltildi. Bu, `torch`'un doğru cuDNN fonksiyonlarını bulmasını sağladı.

### b. Sorun: `gRPC` Sunucusu Başlamıyor (`Mismatched Protobuf Versions` veya `ImportError`)
*   **Neden:** `sentiric-contracts-py` paketi, `protobuf` kütüphanesinin eski bir versiyonu (`<4.0`) ile derlenmiş `protoc` tarafından oluşturulmuş. Bu, `protobuf 4.x` versiyonlarında kaldırılan `runtime_version` ve `Domain` gibi iç API'lere erişmeye çalışmasına neden oluyordu.
*   **Çözüm:** `protobuf` versiyonu `4.25.3`'e yükseltildi ve `app/services/grpc_server.py` dosyasının başına, `sentiric-contracts-py`'nin beklediği eski API'leri taklit eden bir **"Monkey Patch"** eklendi. Bu, kontrat reposunu değiştirmeden sorunu çözmemizi sağladı.

### c. Sorun: `No space left on device` (CI Ortamında)
*   **Neden:** Standart GitHub Actions runner'larının (`~14 GB`) disk alanı, özellikle GPU imajının build işlemi sırasında (`torch` kurulumu) oluşan devasa ara katmanlar için yetersiz kaldı.
*   **Çözüm:** GPU `Dockerfile`'ı, kurulum ve temizlik işlemlerini tek bir `RUN` katmanında birleştiren "tek katmanlı kurulum" mimarisine geçirildi. Bu, build sırasındaki maksimum disk kullanımını düşürerek CI runner'ının limitleri içinde kalmasını sağladı.

### d. Zararsız Uyarı: `UserWarning: pkg_resources is deprecated`
*   **Neden:** `librosa` kütüphanesi, Python'da eski kabul edilen `pkg_resources` modülünü kullanmaktadır.
*   **Durum:** Bu bir hata değil, sadece bilgilendirici bir uyarıdır. Uygulamanın çalışmasına hiçbir olumsuz etkisi yoktur ve güvenle göz ardı edilebilir.

---

Bu doküman, projenin "yaşayan hafızası" olarak güncel tutulmalıdır. Yeni bir bağımlılık eklendiğinde veya büyük bir versiyon güncellemesi yapıldığında, buradaki notlar dikkate alınmalıdır.

