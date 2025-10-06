### 📄 File: `README.md` | 🏷️ Markdown

```markdown
# 🤫 Sentiric STT Whisper Service

[![Status](https://img.shields.io/badge/status-active-success.svg)]()
[![Language](https://img.shields.io/badge/language-Python-blue.svg)]()
[![Engine](https://img.shields.io/badge/engine-FasterWhisper-yellow.svg)]()

**Sentiric STT Whisper Service**, yerel donanım üzerinde yüksek hızlı ve yüksek doğruluklu Konuşma Tanıma (STT) sağlayan uzman bir motordur. `Faster-Whisper` kütüphanesini kullanarak Open AI'ın Whisper modelini çalıştırır.

Bu servis, genellikle `stt-gateway-service` tarafından düşük gecikmeli veya maliyet etkin dosya transkripsiyonu için çağrılır.

## 🎯 Temel Sorumluluklar

*   **Transkripsiyon:** Ham ses (byte dizisi) alır ve metne çevirir.
*   **Model Yönetimi:** Yapılandırma ile belirtilen modeli (medium, large, vs.) yükler ve yönetir.
*   **Donanım Hızlandırma:** GPU (CUDA) veya optimize edilmiş CPU çekirdeklerini kullanır.

## 🛠️ Teknoloji Yığını

*   **Dil:** Python 3.11
*   **Çekirdek Kütüphane:** Faster-Whisper, CTranslate2
*   **Arayüz:** FastAPI (HTTP) veya gRPC (Tonic)
*   **Model Yapılandırması:** `WHISPER_MODEL_SIZE`, `WHISPER_DEVICE`

## 🔌 API Etkileşimleri

*   **Gelen (Sunucu):**
    *   `sentiric-stt-gateway-service` (HTTP POST / gRPC): `WhisperTranscribe` RPC'si.
*   **Giden (İstemci):**
    *   (Yok) - Harici API'lere bağımlı değildir; kendi modelini çalıştırır.

---
## 🏛️ Anayasal Konum

Bu servis, [Sentiric Anayasası'nın](https://github.com/sentiric/sentiric-governance) **AI Engine Layer**'ında yer alan uzman bir bileşendir.