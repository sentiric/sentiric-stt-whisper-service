#!/bin/bash
set -e

# Renkler
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo "🧪 Sentiric STT Service - End-to-End Test"
echo "========================================="

# 1. Servis Ayakta mı? (Health Check)
echo -n "1. Health Check Kontrolü... "
HEALTH=$(curl -s http://localhost:15030/health | grep "healthy")

if [ -z "$HEALTH" ]; then
    echo -e "${RED}BAŞARISIZ${NC}"
    echo "Servis çalışmıyor veya yanıt vermiyor."
    exit 1
else
    echo -e "${GREEN}BAŞARILI${NC}"
fi

# 2. Test Dosyası Hazırlığı
echo -n "2. Test Dosyası İndiriliyor (JFK)... "
if [ ! -f jfk.wav ]; then
    wget -q -O jfk.wav https://github.com/ggerganov/whisper.cpp/raw/master/samples/jfk.wav
fi
echo -e "${GREEN}HAZIR${NC}"

# 3. Dosyayı Konteynere Kopyala (CLI Testi İçin)
echo -n "3. Dosya Konteynere Yükleniyor... "
CONTAINER_ID=$(docker compose ps -q stt-whisper-service)
docker cp jfk.wav $CONTAINER_ID:/app/jfk.wav
echo -e "${GREEN}TAMAM${NC}"

# 4. CLI Transkripsiyon Testi
echo "4. Transkripsiyon Testi Başlatılıyor..."
OUTPUT=$(docker compose exec stt-whisper-service stt_cli file /app/jfk.wav)

echo "--- ÇIKTI ---"
echo "$OUTPUT"
echo "-------------"

if echo "$OUTPUT" | grep -q "ask not what your country"; then
    echo -e "\n🎉 ${GREEN}TEST BAŞARILI: Beklenen metin bulundu!${NC}"
    # Temizlik
    rm jfk.wav
    exit 0
else
    echo -e "\n❌ ${RED}TEST BAŞARISIZ: Beklenen metin bulunamadı.${NC}"
    exit 1
fi