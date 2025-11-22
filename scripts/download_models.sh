#!/bin/bash
set -e

# Kullanım: ./download_models.sh [model_name]
# Modeller: tiny, base, small, medium, large-v3

MODEL_NAME=${1:-"base"}
MODEL_DIR="./models"
BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
# Dizin oluştur
mkdir -p "$MODEL_DIR"

echo "🎧 Sentiric Model Downloader"
echo "---------------------------"
echo "Hedef Model: $MODEL_NAME"
echo "Hedef Dizin: $MODEL_DIR"

# Dosya adı belirle
FILENAME="ggml-${MODEL_NAME}.bin"
FILEPATH="${MODEL_DIR}/${FILENAME}"

if [ -f "$FILEPATH" ]; then
    echo "✅ Model dosyası zaten mevcut: $FILENAME"
else
    echo "⬇️ İndiriliyor: $FILENAME ..."
    curl -L "${BASE_URL}/${FILENAME}" -o "$FILEPATH"
    
    if [ $? -eq 0 ]; then
        echo "✅ İndirme tamamlandı."
    else
        echo "❌ İndirme başarısız!"
        rm -f "$FILEPATH"
        exit 1
    fi
fi

echo "Hazır! Config dosyanızda 'STT_WHISPER_SERVICE_MODEL_FILENAME=$FILENAME' ayarını kullanın."