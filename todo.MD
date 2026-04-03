## 📋 Görev Kartı (Task Note)

**Başlık:** [Research & Discovery] Speaker Diarization Sapmalarından Duygu Durum Analizi Türetilmesi
**Servis:** Whisper / STT Engine
**Etiketler:** `#Discovery` `#AcousticAnalysis` `#EmotionAI` `#Backlog`

### 🔍 Keşfedilen Bulgu (Finding)

Whisper/STT servisinin diarization (konuşmacı ayrıştırma) aşamasında, kullanıcı aynı kişi olmasına rağmen ses karakteristiği (hız, ton, rezonans) değiştiğinde sistemin bunu **"Konuşmacı B"** olarak etiketlediği gözlemlenmiştir.

* **Anomali:** Tek kullanıcı, çift Speaker ID.
* **İçgörü:** Bu bir hata değil, kullanıcının "Analitik Mod"dan "Meditatif/Akış Modu"na geçtiğini gösteren akustik bir sinyaldir.

### 💡 Fikir: "Deep Waters" (Derin Sular) Modu

Sistemde Speaker ID değişimi algılandığında (ve sesin kullanıcıya ait olduğu cross-check ile doğrulandığında), bu verinin bir **"Mood Shift"** (Mod Değişimi) parametresi olarak işlenmesi.

### 🛠️ Uygulama Planı (Drafting)

1. **Diarization Meta-Veri Analizi:** Speaker ID değişim noktalarındaki ses parametrelerini (pitch, pause duration, word rate) logla.
2. **Mod Tanımlama:** * **Speaker A:** Teknik, hızlı, veri odaklı.
* **Speaker B:** Felsefi, yavaş, "akışta" (Deep Waters).


3. **LLM Entegrasyonu:** Bu mod değişimini asistanın (LLM) sistem promptuna "User is currently in a reflective state" notuyla anlık olarak besle.

### 📌 Not

> *"Bu konuşmayı boşa gidermeyelim; sistemin bizi bizden daha iyi tanıması için bu 'hata'yı bir 'yetenek' olarak kurgulayalım."*

---
LLM Entegrasyonunda kullanılabilecek bir örnek
```txt
Kullanıcı, yüksek tempoda fikir üreten bir mimar. Konuşmalarındaki Diarization hatalarını (A/B speaker karmaşasını) duygu durum değişimi olarak analiz et. Kullanıcı 'akış' (meditatif) moduna geçtiğinde daha felsefi ve derinlemesine yanıtlar ver; 'analitik' modda olduğunda ise teknik dökümantasyon netliğinde konuş. Onu onaylamak yerine, fikirlerindeki mantıksal boşlukları (çelişki dedektörü) nazikçe göster.
```
---
Ek Not: Bunlar stt servisi görevleri değildir. Buraan taşınacaktır. 
```
graph TD
    %% Katman 1: Giriş ve Algılama
    subgraph Input_Layer [Giriş ve Akustik Analiz]
        A[Kullanıcı Sesi] -->|Flutter App| B(VAD: Voice Activity Detection)
        B -->|Aktif Ses| C{STT Engine: Whisper}
    end

    %% Katman 2: Veri İşleme ve Meta-Veri
    subgraph Processing_Layer [Diarization & Duygu Motoru]
        C -->|Metin Çıktısı| D[Diarization: Speaker ID A/B]
        C -->|Akustik Metrikler| E[Pitch, Power, Word Rate Analysis]
        D & E --> F{Duygu Durum Sınıflandırıcı}
        F -->|Analitik/Meditatif| G[Contextual Metadata]
    end

    %% Katman 3: Hafıza ve Zeka
    subgraph Intelligence_Layer [Hafıza ve LLM]
        G & D -->|Embeddings| H[(Qdrant Vector DB)]
        H -->|Benzerlik Araması| I[Çelişki & İlişki Dedektörü]
        I -->|Geçmiş Bağlam| J{Offline LLM - On-Prem GPU}
    end

    %% Katman 4: Çıktı ve Feedback
    subgraph Feedback_Layer [Bilişsel Feedback]
        J --> K[Bilişsel Feedback: Özet/Uyarı/Analiz]
        K -->|Dashboard| L[Beyin Haritası Görselleştirme]
    end

    %% İlişkiler
    L -.->|Kullanıcı Gelişimi| A
```

---
* Mantıksal Boşluklar
Sessizlik Eşiği Paradoksu: Teknik olarak 1500ms sessizlik süresini (timeout) çok uzun buluyorsun (kayıtların bölünmemesinden şikayetçisin) ama aynı zamanda düşüncelerinin kaybolmasından korkuyorsun. Sessizlik süresini azaltırsan, derin düşünme anlarındaki duraksamaların sistemi kapatır ve veri kaybın artar. Artırırsan da şu an yaşadığın "ayrıştırılamayan uzun bloklar" sorunu devam eder. Sistemin sabit bir ms değeriyle değil, duygu durumu bazlı dinamik bir timeout ile çalışması gerektiğini ıskalıyorsun.


---


## 🌊 FAZ 3: Deep Waters & Gelişmiş Özellikler (The Killer Features)
*Sistemi rakiplerden ayıracak bilişsel (cognitive) yeteneklerin koda dökülmesi.*

-[ ] **Dinamik VAD ve Sessizlik Eşiği Paradoksunun Çözümü:**
  - Whisper STT'nin sabit 1500ms bekleme süresi iptal edilecek.
  - Whisper'dan gelen akustik `arousal` (tempo/uyarılmışlık) verisi okunacak. Kullanıcı "Akış/Meditatif" moda geçtiğinde VAD timeout süresi anlık olarak (örn: 2500ms'ye) uzatılacak, "Analitik/Hızlı" modda ise (örn: 800ms'ye) kısaltılacak.


  OLAY GEÇMİŞİ
  ---

  ---
  

- [ ] **`sentiric-crystalline-service` Projesinin Başlatılması:**
  - Bu servis aktif çağrıya KARIŞMAYACAK. Sessizce RabbitMQ'dan `acoustic.mood.shifted` eventlerini (Whisper Diarization A/B sapmaları, duygu değişimleri) dinleyecek.
  - Kullanıcının "Mantıksal Boşluklarını" ve "Duygusal Profilini" anlık Qdrant Vector DB'ye yazacak.

- [ ] **Yeni `sip-uac`'ın (Eski Mobile) Register Yeteneği:**
  - Sadece peer-to-peer (Trunk) değil, standart bir PBX abonesi gibi **REGISTER** olma yeteneği eklenecek. Linphone ile test edilecek. Profilleme, rehber ve geçmiş eklenecek.

`sentiric-contracts`, `sentiric-stt-whisper-service`, `sentiric-telephony-action-service`.
>  *"Whisper servisindeki sabit VAD timeout mantığını iptal et. Whisper'ın ürettiği arousal/valence ve speaker_vec verilerini kullanarak dinamik VAD timeout hesaplayan algoritmayı yaz. Ayrıca bu duygu/mod değişimini (Analitikten Meditatife geçiş) 'AcousticMoodShiftedEvent' adında yeni bir Protobuf event'i olarak tanımla ve RabbitMQ'ya basacak kodu ekle."*

---