# 🤫 Sentiric STT Whisper Service

[![Status](https://img.shields.io/badge/status-active-success.svg)]()
[![Python Version](https://img.shields.io/badge/python-3.11-blue.svg)]()
[![Engine](https://img.shields.io/badge/engine-FasterWhisper-yellow.svg)]()
[![Interface](https://img.shields.io/badge/interface-REST%20%26%20gRPC-brightgreen.svg)]()

**Sentiric STT Whisper Service**, yerel donanım üzerinde (on-premise) yüksek hızlı ve yüksek doğruluklu Konuşma Tanıma (STT) sağlayan uzman bir AI motorudur. `Faster-Whisper` kütüphanesini kullanarak OpenAI'ın Whisper modelini GPU veya CPU üzerinde optimize bir şekilde çalıştırır.

Bu servis, genellikle `stt-gateway-service` gibi üst katman servisleri tarafından, düşük gecikmeli veya maliyet etkin dosya transkripsiyonu ihtiyaçları için çağrılır.

## ✨ Temel Özellikler

*   **Yüksek Performans:** `CTranslate2` backend'i sayesinde optimize edilmiş transkripsiyon.
*   **Donanım Hızlandırma:** NVIDIA GPU'lar (CUDA) için tam destek ve optimize edilmiş CPU kullanımı.
*   **Çift Arayüz:** Hem **REST (HTTP)** hem de **gRPC** protokolleri üzerinden istek kabul eder.
*   **Gelişmiş Gözlemlenebilirlik:** Prometheus metrikleri ve yapılandırılmış JSON loglama (`structlog`).
*   **Dinamik Sağlık Kontrolü:** Modelin yüklenip hazır olup olmadığını bildiren `/health` endpoint'i.
*   **Sağlamlık:** Asenkron model yükleme, istek zaman aşımları ve dosya boyutu/tipi doğrulamaları.
*   **Otomatik Cihaz Seçimi:** `STT_WHISPER_SERVICE_DEVICE="auto"` ayarı ile ortamda GPU varsa otomatik olarak kullanır.

## 🛠️ Teknoloji Yığını

*   **Dil:** Python 3.11
*   **Web Framework:** FastAPI
*   **RPC Framework:** gRPC
*   **Çekirdek Kütüphane:** Faster-Whisper, CTranslate2
*   **Gözlemlenebilirlik:** Prometheus, Structlog
*   **Paket Yönetimi:** Poetry

## 🔌 API Arayüzleri

Servis, aynı işlevselliği sunan iki ana arayüz sağlar:

#### 1. REST API

*   **Endpoint:** `POST /api/v1/transcribe`
*   **Request:** `multipart/form-data`
    *   `file`: Ses dosyası (binary)
    *   `language`: Dil kodu (örn: `tr`, `en`). Belirtilmezse otomatik algılar.
*   **Response:** JSON
    ```json
    {
      "text": "transkripsiyon metni burada yer alır",
      "language": "tr",
      "language_probability": 0.98,
      "duration": 15.34
    }
    ```


#### 2. gRPC API

*   **Servis:** `SttWhisperService`
*   **RPC Metodu:** `WhisperTranscribe`
*   **Protokol Tanımı:** `sentiric-contracts` deposundan gelir.

#### 3. Operasyonel Endpoints

*   **Sağlık Kontrolü:** `GET /health` - Modelin hazır olup olmadığını ve GPU durumunu döndürür.
*   **Metrikler:** `GET /metrics` - Prometheus formatında metrikler sunar.

---

### Test Komutları

Varsayılan olarak servisin `localhost` üzerinde `15030` portunda çalıştığını varsayıyoruz. Eğer farklı bir IP veya port kullanıyorsanız, komutlardaki `http://localhost:15030` kısmını güncellemeniz yeterlidir.

#### Örnek 1: Dil Belirterek Transkripsiyon (`welcome.wav` dosyası için)

Bu komut, `welcome.wav` dosyasını sunucuya gönderir ve dilin Türkçe (`tr`) olduğunu belirtir.

```bash
curl -X POST \
  -F "file=@/mnt/c/sentiric/sentiric-assets/audio/tr/welcome.wav" \
  -F "language=tr" \
  http://localhost:15030/api/v1/transcribe
```

#### Örnek 2: Otomatik Dil Algılama ile Transkripsiyon (`cant_hear_you.wav` için)

Bu komut, `language` parametresini göndermez. Servis, sesin dilini kendisi algılamaya çalışacaktır.

```bash
curl -X POST \
  -F "file=@/mnt/c/sentiric/sentiric-assets/audio/tr/system/cant_hear_you.wav" \
  http://localhost:15030/api/v1/transcribe
```

---

### Komutların Açıklaması

*   `curl`: Komut satırı aracı.
*   `-X POST`: HTTP isteğinin `POST` metoduyla yapılacağını belirtir.
*   `-F "file=@/path/to/your/file.wav"`: Bu en önemli kısımdır.
    *   `-F`, `multipart/form-data` formatında veri göndereceğinizi belirtir. Bu, dosya yüklemeleri için standart formattır.
    *   `file=`, API'nin beklediği alan adıdır (`file: UploadFile`).
    *   `@` işareti, `curl`'e bu alana bir dosyanın içeriğini yüklemesini söyler.
*   `-F "language=tr"`: `language` adında bir form alanı ve değeri olarak `tr` gönderir.
*   `http://localhost:15030/api/v1/transcribe`: İsteğin yapılacağı tam URL.

### Beklenen Çıktı

Başarılı bir istek sonucunda aşağıdaki gibi bir JSON çıktısı almalısınız:

```json
{
  "text": "Sentiric'e hoş geldiniz",
  "language": "tr",
  "language_probability": 0.99853515625,
  "duration": 1.56
}
```

---

### Önce Sağlık Durumunu Kontrol Edin!

Transkripsiyonu denemeden önce, servisin ve modelin hazır olup olmadığını kontrol etmek her zaman iyi bir fikirdir. Bunu `/health` endpoint'i ile yapabilirsiniz:

```bash
curl http://localhost:15030/health
```

Eğer model başarıyla yüklendiyse, şöyle bir çıktı alırsınız:

```json
{
  "status": "healthy",
  "model_ready": true,
  "gpu_available": true,
  "gpu_info": {
    "gpu_count": 1,
    "current_device": 0,
    "device_name": "NVIDIA GeForce RTX 3080"
  },
  "service_version": "1.0.0",
  "model_size": "medium"
}
```

Eğer `model_ready` değeri `false` ise, transkripsiyon istekleriniz `503 Service Unavailable` hatası verecektir. Bu durumda modelin yüklenmesini beklemeniz gerekir.

---

---

## 🔬 Test Etme

### REST API Testi

`curl` komutunu kullanarak REST API'yi test edebilirsiniz:

```bash
curl -X POST \
  -F "file=@/mnt/c/sentiric/sentiric-assets/audio/tr/welcome.wav" \
  -F "language=tr" \
  http://localhost:15030/api/v1/transcribe
```

### gRPC API Testi

gRPC arayüzünü test etmek için `grpc_test_client.py` script'ini kullanabilirsiniz.

1.  **Gerekli İstemci Kütüphanelerini Kurun:**
    ```bash
    pip install grpcio grpcio-tools sounddevice numpy sentiric-contracts-py@git+https://github.com/sentiric/sentiric-contracts.git@v1.9.4
    ```

2.  **Test Script'ini Çalıştırın:**
    Servisin `docker-compose` dosyasında `15031` portunun map edildiğinden emin olun (`- "15031:15031"`).

    ```bash
    python3 grpc_test_client.py /mnt/c/sentiric/sentiric-assets/audio/tr/welcome.wav tr
    ```

    Başarılı bir istek sonucunda aşağıdaki gibi bir çıktı almalısınız:

    ```
    ✅ BAŞARILI GRPC YANITI:
      Metin: 'transkripsiyon metni burada yer alır'
      Süre: 9.20s
    ```

## ⚙️ Yapılandırma (Environment Değişkenleri)

Servis, ortam değişkenleri ile yapılandırılır. Başlıca ayarlar:

| Değişken                               | Açıklama                                       | Varsayılan |
| -------------------------------------- | ---------------------------------------------- | ---------- |
| `STT_WHISPER_SERVICE_HTTP_PORT`        | REST API için port.                            | `15030`    |
| `STT_WHISPER_SERVICE_GRPC_PORT`        | gRPC API için port.                            | `15031`    |
| `STT_WHISPER_SERVICE_MODEL_SIZE`       | Whisper model boyutu (`tiny`, `medium`, `large-v3`). | `medium`   |
| `STT_WHISPER_SERVICE_DEVICE`           | Çalışma cihazı (`cuda`, `cpu`, `auto`).        | `auto`     |
| `STT_WHISPER_SERVICE_COMPUTE_TYPE`     | Hesaplama hassasiyeti (`float16`, `int8`).     | `int8`     |
| `STT_WHISPER_SERVICE_LOG_LEVEL`        | Log seviyesi (`INFO`, `DEBUG`).                | `INFO`     |

## 🏛️ Anayasal Konum

Bu servis, [Sentiric Anayasası'nın](https://github.com/sentiric/sentiric-governance) **AI Engine Layer**'ında yer alan uzman bir bileşendir.