# ⚙️ Yapılandırma Rehberi (v2.2.0)

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
| `STT_WHISPER_SERVICE_VAD_URL` | VAD model indirme linki. | `https://huggingface.co/.../ggml-silero-v5.1.2.bin` |

### Motor ve Performans (Batching)
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_DEVICE` | `cuda` veya `cpu`. | `auto` |
| `STT_WHISPER_SERVICE_THREADS` | CPU Thread sayısı. | `4` |
| `STT_WHISPER_SERVICE_PARALLEL_REQUESTS` | **(YENİ)** Aynı anda GPU'da işlenecek istek sayısı. VRAM'e göre artırılabilir. | `2` |
| `STT_WHISPER_SERVICE_FLASH_ATTN` | Flash Attention optimizasyonu. | `true` |

### Transkripsiyon ve Zeka
| Değişken | Açıklama | Varsayılan |
|---|---|---|
| `STT_WHISPER_SERVICE_LANGUAGE` | Varsayılan dil. | `auto` |
| `STT_WHISPER_SERVICE_ENABLE_VAD` | VAD (Sessizlik Filtresi) aktif mi? | `true` |
| `STT_WHISPER_SERVICE_ENABLE_DIARIZATION` | **(YENİ)** Konuşmacı ayrıştırma (Speaker Turn Detection). | `false` |
| `STT_WHISPER_SERVICE_BEAM_SIZE` | Beam search genişliği. | `5` |