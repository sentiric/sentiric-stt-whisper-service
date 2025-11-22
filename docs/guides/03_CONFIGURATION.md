# ⚙️ Yapılandırma Rehberi

Servis, aşağıdaki ortam değişkenleri ile yapılandırılır. `sentiric-config` standartlarına tam uyumludur.

### Ağ (Network)
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_IPV4_ADDRESS` | Dinlenecek IP adresi. | `0.0.0.0` |
| `STT_WHISPER_SERVICE_GRPC_PORT` | gRPC Portu. | `15031` |
| `STT_WHISPER_SERVICE_HTTP_PORT` | HTTP (Health/Studio) Portu. | `15030` |
| `STT_WHISPER_SERVICE_METRICS_PORT` | Prometheus Metrics Portu. | `15032` |

### Model ve Performans
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | Kullanılacak model dosyası (örn: `ggml-medium.bin`). | `ggml-medium.bin` |
| `STT_WHISPER_SERVICE_THREADS` | CPU Thread sayısı. | `4` |
| `STT_WHISPER_SERVICE_DEVICE` | `cuda` veya `cpu`. (Build sırasında otomatik seçilir). | `auto` |

### Transkripsiyon Ayarları
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_LANGUAGE` | **Varsayılan dil.** API isteğinde `language` belirtilmezse bu değer kullanılır. | `auto` |
| `STT_WHISPER_SERVICE_BEAM_SIZE` | Beam search genişliği. (1 = Greedy). | `5` |
| `STT_WHISPER_SERVICE_NO_SPEECH_THRESHOLD` | VAD hassasiyeti (0.0 - 1.0). | `0.6` |
| `STT_WHISPER_SERVICE_LOGPROB_THRESHOLD` | Düşük güvenli tahminleri filtreleme. | `-1.0` |