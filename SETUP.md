### 🚀 1. NVIDIA Container Toolkit’in yüklü olduğundan emin ol

GPU’yu Docker içinde kullanmak için **NVIDIA Container Toolkit** kurulu olmalı.

Ubuntu örneği:

```bash
sudo apt install -y nvidia-container-toolkit
sudo systemctl restart docker
```

Ardından şu komutla test et:

```bash
docker run --rm --gpus all nvidia/cuda:12.2.0-base-ubuntu22.04 nvidia-smi
```

Eğer GPU bilgilerini görüyorsan, sistem hazır demektir ✅

---

### 🐳 2. `docker-compose.gpu.yml` dosyasını kullanarak başlat

Proje dizininde olduğundan emin ol:

```bash
cd /mnt/c/sentiric/sentiric-stt-whisper-service
```

Sonra servisi şu şekilde başlat:

```bash
docker compose -f docker-compose.gpu.yml up
```

Ya da arka planda (daemon olarak) çalıştırmak istersen:

```bash
docker compose -f docker-compose.gpu.yml up -d
```

---

### 🧠 3. Servisin durumunu kontrol et

Çalışıp çalışmadığını görmek için:

```bash
docker compose -f docker-compose.gpu.yml ps
```

Logları görmek istersen:

```bash
docker compose -f docker-compose.gpu.yml logs -f
```

---

### 💡 4. Sağlık kontrolü (healthcheck)

Dosyada healthcheck şu endpoint’e bakıyor:

```
http://localhost:15030/health
```

Yani tarayıcıda veya terminalde test edebilirsin:

```bash
curl http://localhost:15030/health
```

Yanıt `{"status":"healthy"}` gibi bir şey olmalıdır.

---
