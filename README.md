# 🎧 Sentiric STT Whisper Service (v2.5.1)

[![Production Ready](https://img.shields.io/badge/status-production%20ready-success.svg)]()
[![License](https://img.shields.io/badge/license-AGPLv3-blue.svg)]()
[![Docker](https://img.shields.io/badge/docker-cpu%2Fgpu-orange.svg)]()

**Sentiric STT**, OpenAI Whisper modelini kullanan, **C++ tabanlı**, GPU hızlandırmalı ve **Duyuşsal Zeka (Affective Intelligence)** yeteneklerine sahip yüksek performanslı bir konuşmadan yazıya (Speech-to-Text) motorudur.

Bu servis, **Sentiric İletişim İşletim Sistemi**'nin bir parçası olarak tasarlanmış olsa da, **tamamen bağımsız (standalone)** bir Whisper API sunucusu olarak da kullanılabilir.

## 🚀 Neden Bu Servis? (Unique Selling Points)

1.  **Saf C++ Performansı:** Python, PyTorch veya ağır frameworkler içermez. `whisper.cpp` çekirdeği sayesinde minimum RAM ve CPU ile maksimum hız (RTF) sağlar.
2.  **Duyuşsal Zeka (Zero-Latency DSP):** Ek bir AI modeli çalıştırmadan, sinyal işleme (DSP) ile milisaniyeler içinde:
    *   **Cinsiyet Tespiti** (Erkek/Kadın)
    *   **Duygu Analizi** (Heyecanlı, Üzgün, Nötr, Kızgın)
    *   **Konuşmacı Ayrıştırma** (Speaker Diarization) yapar.
3.  **Production Grade:**
    *   **Dynamic Batching:** GPU üzerinde aynı anda birden fazla isteği paralel işler.
    *   **Smart VAD:** CPU tabanlı ses aktivite algılama ile GPU'yu sadece konuşma olduğunda yorar.
    *   **Auto-Provisioning:** Gerekli modelleri açılışta otomatik indirir ve doğrular.

---

## 🛠️ Kurulum ve Çalıştırma

### Seçenek 1: Docker (Önerilen)

**GPU (NVIDIA) ile:**
```bash
make up-gpu
# Veya manuel:
docker run -d --gpus all -p 15030:15030 -p 15031:15031 ghcr.io/sentiric/sentiric-stt-whisper-service:latest-gpu
```

**CPU ile:**
```bash
make up-cpu
# Veya manuel:
docker run -d -p 15030:15030 -p 15031:15031 ghcr.io/sentiric/sentiric-stt-whisper-service:latest
```

### Seçenek 2: Bağımsız Kullanım (Standalone API)

Servis ayağa kalktığında, standart OpenAI Whisper API'sine benzer (ancak daha zengin) bir REST API sunar.

**Örnek İstek:**
```bash
curl http://localhost:15030/v1/transcribe \
  -F "file=@kayit.wav" \
  -F "language=tr" \
  -F "diarization=true"
```

**Örnek Yanıt (Zenginleştirilmiş):**
```json
{
  "text": "Merhaba, bugün nasılsınız?",
  "language": "tr",
  "duration": 2.5,
  "segments": [
    {
      "text": "Merhaba, bugün nasılsınız?",
      "start": 0.0,
      "end": 2.5,
      "gender": "F",           // DSP ile tespit edildi
      "emotion": "neutral",    // DSP ile tespit edildi
      "speaker_id": "spk_0",   // Kümeleme ile atandı
      "words": [...]
    }
  ]
}
```

---

## ⚙️ Yapılandırma (Environment Variables)

Servis, `.env` dosyası veya Docker ortam değişkenleri ile tamamen yönetilebilir.

| Değişken | Varsayılan | Açıklama |
| :--- | :--- | :--- |
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | `ggml-medium.bin` | Kullanılacak model boyutu (tiny, base, small, medium, large-v3). |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | `2` | GPU üzerinde aynı anda işlenecek maksimum paralel istek sayısı. |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION`| `true` | Konuşmacı ayrıştırma özelliğini aç/kapat. |
| `STT_WHISPER_SERVICE_CLUSTER_THRESHOLD` | `0.94` | Konuşmacı ayrım hassasiyeti (Düşük=Birleştirir, Yüksek=Ayırır). |

---

## 🏗️ Mimari ve Entegrasyon

*   **gRPC (Port 15031):** Yüksek performanslı iç iletişim için (`sentiric-contracts` uyumlu).
*   **HTTP (Port 15030):** Dış dünya entegrasyonu ve Web UI için.
*   **Metrics (Port 15032):** Prometheus uyumlu metrikler (`/metrics`).

Bu proje, Sentiric ekosisteminin bir parçasıdır ancak tek başına bir mikroservis olarak dağıtılabilir ve kullanılabilir.


## 📜 Lisans
AGPLv3 License.