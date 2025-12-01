# ⚙️ Yapılandırma Rehberi (v2.5.1)

Servis, aşağıdaki ortam değişkenleri ile yapılandırılır.

### Ağ (Network)
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_IPV4_ADDRESS` | Dinlenecek IP adresi. | `0.0.0.0` |
| `STT_WHISPER_SERVICE_GRPC_PORT` | gRPC Portu. | `15031` |
| `STT_WHISPER_SERVICE_HTTP_PORT` | HTTP (Health/Studio) Portu. | `15030` |
| `STT_WHISPER_SERVICE_METRICS_PORT` | Prometheus Metrics Portu. | `15032` |

### Model Yönetimi (Auto-Provisioning)
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_MODEL_FILENAME` | Ana model dosyası. | `ggml-medium.bin` |
| `STT_WHISPER_SERVICE_VAD_MODEL` | VAD model dosyası. | `ggml-silero-vad.bin` |
| `STT_WHISPER_SERVICE_VAD_URL` | VAD model indirme linki. | `ttps://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v6.2.0.bin` |

### Motor ve Performans (Batching & Queue)
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_DEVICE` | `cuda` veya `cpu`. | `auto` |
| `STT_WHISPER_SERVICE_THREADS` | CPU Thread sayısı. | `4` |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | Aynı anda GPU'da işlenecek istek sayısı. | `2` |
| `STT_WHISPER_SERVICE_QUEUE_TIMEOUT_MS` | **(KRİTİK)** Havuz doluyken bekleme süresi. CPU için `60000`, GPU için `5000` önerilir. | `5000` |
| `STT_WHISPER_SERVICE_FLASH_ATTN` | Flash Attention optimizasyonu. | `true` |

### Transkripsiyon ve Zeka
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_LANGUAGE` | Varsayılan dil. | `auto` |
| `STT_WHISPER_SERVICE_ENABLE_VAD` | VAD (Sessizlik Filtresi) aktif mi? | `true` |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION` | Konuşmacı ayrıştırma (Speaker Turn Detection). | `true` |
| `STT_WHISPER_SERVICE_CLUSTER_THRESHOLD` | Diyarizasyon hassasiyeti. `0.94` idealdir. | `0.94` |
| `STT_WHISPER_SERVICE_BEAM_SIZE` | Beam search genişliği. | `5` |