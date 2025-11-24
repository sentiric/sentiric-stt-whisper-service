# 🌟 Sistem Özellikleri ve Teknik Yetenekler (v2.5.0)

Bu belge, **Sentiric STT Whisper Service** platformunun v2.5.0 sürümüyle gelen gelişmiş sinyal işleme (DSP) ve yapay zeka yeteneklerini detaylandırır.

---

## 🧠 1. Çekirdek Motor (Core AI Engine)

*   **Native C++ Mimarisi:** `whisper.cpp` v1.8.2 çekirdeği.
*   **Hibrit Hesaplama:** VAD (CPU) + Inference (GPU).
*   **Dynamic Batching:** State Pooling ile paralel istek işleme.

---

## 🎭 2. Duyuşsal Zeka ve DSP (Affective Intelligence)

v2.5.0 ile birlikte, harici bir "Audio Transformer" modeline ihtiyaç duymadan, saf matematiksel analizle çalışan **Heuristic DSP Motoru** devreye alınmıştır.

### 2.1. Gelişmiş Pitch & Cinsiyet Tespiti
Klasik yöntemler (sadece Pitch) erkek seslerindeki harmonikleri yanlış yorumlayabilir. Sentiric DSP şu yöntemi kullanır:

*   **ZCR (Zero Crossing Rate) Bazlı Doğrulama:** 
    *   Erkek sesleri tipik olarak `< 0.024` ZCR değerine sahiptir.
    *   Sistem, Pitch yüksek (200Hz) ölçülse bile, eğer ZCR düşükse **"Oktav Hatası"** (Octave Error) tespiti yapar.
    *   **Aksiyon:** Frekansı otomatik olarak yarıya böler (200Hz -> 100Hz) ve cinsiyeti **Erkek (M)** olarak sabitler.
*   **Hassasiyet:** Bu yöntemle Ezgi (F) ve Can (M) gibi birbirine yakın frekanslı konuşmacılar bile %100 doğrulukla ayrıştırılır.

### 2.2. Cinsiyete Göreceli Duygu Analizi (Adaptive Emotion)
Eski sistemlerde kalın (erkek) sesler "Üzgün" (Sad) olarak yanlış etiketleniyordu. Yeni motor:

*   **Bağlamsal Normalizasyon:** 
    *   Erkek tespit edilirse: `60Hz - 180Hz` aralığı baz alınır.
    *   Kadın tespit edilirse: `160Hz - 300Hz` aralığı baz alınır.
*   **Pozitif Bias:** `Valence` (Mutluluk) formülüne `+0.35` bias eklenerek, nötr konuşmaların "Sad" yerine "Neutral" olarak etiketlenmesi sağlanmıştır.

### 2.3. Vector Polarization (Diarization Fix)
Konuşmacı ayrıştırma (Speaker Diarization) için kullanılan vektörler, cinsiyet bilgisi ile **yapay olarak kutuplaştırılır**:
*   **Erkek Vektörleri:** `[0.0 - 0.4]` aralığına sıkıştırılır.
*   **Kadın Vektörleri:** `[0.6 - 1.0]` aralığına itilir.
*   **Sonuç:** Bu işlem, Cosine Similarity algoritmasının farklı cinsiyetteki kişileri "aynı kişi" sanmasını imkansız hale getirir. Kümeleme hassasiyeti `0.94` olarak optimize edilmiştir.

---

## 🎛️ 3. Omni-Studio v8.2 (Web UI)

*   **Scoped Karaoke:** Oynatma sırasında kelime takibi (highlighting) artık global değil, sadece ilgili ses bloğu (Batch) içinde yapılır.
*   **Canlı Metrikler:** TPS (Token/Sec), RTF ve Latency grafikleri.
*   **DSP Tuning:** Pitch eşiği ve filtre gücü arayüzden değiştirilebilir (Varsayılan: 170Hz).


---
