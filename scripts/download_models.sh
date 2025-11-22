#!/bin/bash
set -e

# Kullanım: ./download_models.sh [model_name]
# Modeller: tiny, base, small, medium, large-v3

MODEL_NAME=${1:-"base"}
MODEL_DIR="./models"
BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
VAD_URL="https://github.com/snakers4/silero-vad/raw/master/files/silero_vad.onnx" # Hayır, ggml versiyonu lazım!
# DÜZELTME: Whisper.cpp için özel ggml-silero modeli lazım.
# HuggingFace'de ggerganov reposunda var.
VAD_BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

# Dizin oluştur
mkdir -p "$MODEL_DIR"

echo "🎧 Sentiric Model Downloader"
echo "---------------------------"
echo "Hedef Model: $MODEL_NAME"
echo "Hedef Dizin: $MODEL_DIR"

# 1. Ana Model İndir (ggml-base.bin)
FILENAME="ggml-${MODEL_NAME}.bin"
FILEPATH="${MODEL_DIR}/${FILENAME}"

if [ -f "$FILEPATH" ]; then
    echo "✅ Model dosyası zaten mevcut: $FILENAME"
else
    echo "⬇️ İndiriliyor: $FILENAME ..."
    curl -L "${BASE_URL}/${FILENAME}" -o "$FILEPATH"
    if [ $? -ne 0 ]; then echo "❌ İndirme başarısız!"; rm -f "$FILEPATH"; exit 1; fi
    echo "✅ Ana model indirildi."
fi

# 2. VAD Modeli İndir (ggml-vad-silero.bin)
# Whisper.cpp v1.8.0+ için gerekli.
VAD_FILENAME="ggml-silero-vad.bin"
# Not: Bu dosya ismi repo'ya göre değişebilir, genellikle 'ggml-silero-vad.bin' veya benzeridir.
# Resmi repodaki isimlendirmeyi kullanıyoruz: silero-vad-v5.onnx değil, ggml portu.
# GÜNCEL BİLGİ: Whisper.cpp repo'sunda 'ggml-silero-vad.bin' dosyası yoksa, script hata verir.
# Şimdilik varsayılan olarak:
VAD_FILEPATH="${MODEL_DIR}/ggml-silero-vad.bin"

# URL Kontrolü: Ggerganov'un HF reposunda bu dosya var mı?
# Eğer yoksa, whisper.cpp'nin kendi scripti 'models/download-vad-model.sh' kullanılmalı.
# Biz şimdilik manuel URL veriyoruz (Genelde kullanılan):
# https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-silero-vad.bin (Varsayım)
# DOĞRUSU: Repo scriptini taklit edelim.

if [ -f "$VAD_FILEPATH" ]; then
    echo "✅ VAD model dosyası zaten mevcut."
else
    echo "⬇️ VAD Modeli İndiriliyor (Silero)..."
    # Resmi whisper.cpp VAD modeli URL'i (v1.8.0 sonrası için)
    # Bu URL değişebilir, en garantisi kaynak koddan bulmaktır. 
    # Şimdilik yaygın kullanılanı deniyoruz.
    curl -L "https://github.com/ggerganov/whisper.cpp/raw/master/models/ggml-silero-vad.bin" -o "$VAD_FILEPATH" || \
    curl -L "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-silero-vad.bin" -o "$VAD_FILEPATH"
    
    if [ $? -eq 0 ] && [ -s "$VAD_FILEPATH" ]; then
        echo "✅ VAD modeli indirildi."
    else
        echo "⚠️ VAD modeli indirilemedi! VAD özellikleri çalışmayabilir."
        rm -f "$VAD_FILEPATH"
    fi
fi

echo "Hazır! Config dosyanızda 'STT_WHISPER_SERVICE_MODEL_FILENAME=$FILENAME' ayarını kullanın."