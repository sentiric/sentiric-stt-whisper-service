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
