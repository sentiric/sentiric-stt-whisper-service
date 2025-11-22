# 💡 KB-05: Whisper.cpp v1.8.2 API Referansı ve Migrasyon Rehberi

**DURUM:** Proje mevcut sürümden (v1.7.1), v1.8.2 sürümüne yükseltilmektedir.
**KAYNAK:** `whisper.h` (v1.8.2) ve Resmi Sürüm Notları.

Bu belge, v1.8.2 sürümüyle gelen yeni yapıları, değiştirilen parametreleri ve VAD (Voice Activity Detection) modülünü belgeler.

## 1. Kritik API Değişiklikleri (Breaking Changes)

`whisper_full_params` yapısındaki değişiklikler:

| Eski Parametre / Durum | Yeni Parametre (v1.8.2) | Açıklama |
| :--- | :--- | :--- |
| `suppress_non_speech_tokens` | **`suppress_nst`** | İsim değişikliği. `true` ayarlanırsa [Music], [Applause] gibi tokenları engeller. |
| (Yoktu) | **`no_speech_thold`** | Varsayılan: `0.6`. Eğer "konuşma yok" olasılığı bu eşiği geçerse, segmenti boş döner (Halüsinasyonu önler). |
| (Yoktu) | **`flash_attn`** | `whisper_context_params` içinde. Varsayılan `true`. GPU belleğini ve hızını optimize eder. |

## 2. Yeni VAD (Voice Activity Detection) Modülü

v1.8.2 ile birlikte, sesin işlenmeye değer olup olmadığını anlamak için harici bir VAD API'si eklenmiştir. Bu, `whisper_full` (ağır işlem) çağırmadan önce hafif bir kontrol yapmamızı sağlar.

### 2.1. VAD Yapılandırması (`whisper_vad_params`)
```cpp
struct whisper_vad_params {
    float threshold;               // Konuşma eşiği (Örn: 0.5)
    int   min_speech_duration_ms;  // Min konuşma süresi
    int   min_silence_duration_ms; // Konuşma bitti sayılması için gereken sessizlik
    float max_speech_duration_s;   // Max segment süresi
    // ...
};
```

### 2.2. Temel VAD Kullanımı
```cpp
// 1. VAD Context Başlatma
struct whisper_vad_context_params vparams = whisper_vad_default_context_params();
vparams.use_gpu = true;
struct whisper_vad_context* vctx = whisper_vad_init_from_file_with_params("ggml-base.bin", vparams);

// 2. Konuşma Var mı Kontrolü (Hafif İşlem)
bool is_speech = whisper_vad_detect_speech(vctx, pcm_data, n_samples);

if (is_speech) {
    // 3. Eğer konuşma varsa Ağır Transkripsiyonu Başlat
    whisper_full(ctx, wparams, pcm_data, n_samples);
}
```

## 3. Entegrasyon Stratejisi

`SttEngine` sınıfını güncellerken aşağıdaki parametre haritasını kullanacağız:

### 3.1. `whisper_context_params`
```cpp
struct whisper_context_params cparams = whisper_context_default_params();
cparams.use_gpu = true;
cparams.flash_attn = true; // v1.8.2 ile gelen performans artışı
```

### 3.2. `whisper_full_params`
```cpp
whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

// Halüsinasyon Önleme (Kritik)
wparams.suppress_nst = true;       // Ses olmayan tokenları bastır
wparams.no_speech_thold = 0.6f;    // Sessizlik eşiği (Config'den de alınabilir)

// Diğer Ayarlar
wparams.print_progress = false;
wparams.token_timestamps = true;
```

## 4. Önerilen Config Değişiklikleri (`src/config.h`)

Yeni özellikleri kontrol etmek için `Settings` yapısına şu alanlar eklenecektir:

```cpp
struct Settings {
    // ... mevcut ayarlar ...
    bool flash_attention = true;
    bool suppress_nst = true;
    float no_speech_threshold = 0.6f; // whisper.cpp varsayılanı
};
```
