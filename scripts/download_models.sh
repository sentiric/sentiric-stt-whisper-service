#!/bin/bash
set -e

# Kullanım: ./download_models.sh [model_name]
# Modeller: tiny, base, small, medium, large-v3

MODEL_NAME=${1:-"base"}
MODEL_DIR="/models" # Docker içindeki path veya volume
if [ ! -d "$MODEL_DIR" ]; then MODEL_DIR="./models"; fi

# HuggingFace GGerganov Reposu (En güncel ve güvenilir kaynak)
BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"

# Dizin oluştur
mkdir -p "$MODEL_DIR"

echo "🎧 Sentiric Model Downloader"
echo "---------------------------"
echo "Hedef Model: $MODEL_NAME"
echo "Hedef Dizin: $MODEL_DIR"

# 1. Ana Model İndir (ggml-base.bin)
FILENAME="ggml-${MODEL_NAME}.bin"
FILEPATH="${MODEL_DIR}/${FILENAME}"

if [ -f "$FILEPATH" ] && [ $(stat -c%s "$FILEPATH") -gt 100000 ]; then
    echo "✅ Model dosyası mevcut ve geçerli boyutta: $FILENAME"
else
    echo "⬇️ Ana Model İndiriliyor: $FILENAME ..."
    curl -L "${BASE_URL}/${FILENAME}" -o "$FILEPATH"
    
    if [ $? -ne 0 ] || [ ! -s "$FILEPATH" ]; then 
        echo "❌ İndirme başarısız!"; rm -f "$FILEPATH"; exit 1; 
    fi
    echo "✅ Ana model indirildi."
fi

# 2. VAD Modeli İndir (ggml-silero-vad.bin)
VAD_FILENAME="ggml-silero-vad.bin"
VAD_FILEPATH="${MODEL_DIR}/${VAD_FILENAME}"

# Kontrol: Dosya var mı VE boyutu mantıklı mı? (LFS pointerlar genelde < 1KB olur)
if [ -f "$VAD_FILEPATH" ] && [ $(stat -c%s "$VAD_FILEPATH") -gt 10000 ]; then
    echo "✅ VAD model dosyası geçerli."
else
    echo "⬇️ VAD Modeli İndiriliyor (Silero)..."
    
    # ESKİ (HATALI): GitHub Raw (LFS pointer dönebilir)
    # YENİ (DOĞRU): HuggingFace Direct Download
    VAD_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/${VAD_FILENAME}"
    
    curl -L "$VAD_URL" -o "$VAD_FILEPATH"
    
    # İndirme sonrası boyut kontrolü
    FILESIZE=$(stat -c%s "$VAD_FILEPATH")
    if [ "$FILESIZE" -lt 10000 ]; then
        echo "❌ HATA: İndirilen VAD dosyası çok küçük ($FILESIZE bytes). Muhtemelen bozuk veya LFS pointer."
        echo "İçerik önizleme:"
        head -n 5 "$VAD_FILEPATH"
        rm -f "$VAD_FILEPATH"
        exit 1
    fi
    
    echo "✅ VAD modeli başarıyla indirildi."
fi

echo "🎉 Tüm modeller hazır!"