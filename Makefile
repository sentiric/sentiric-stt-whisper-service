.PHONY: help up-cpu up-gpu down logs clean test

help:
	@echo "🎧 Sentiric STT Whisper Service (C++) Yönetim Paneli"
	@echo "---------------------------------------------------"
	@echo "make up-cpu   : Servisi CPU modunda başlatır (Local Dev)"
	@echo "make up-gpu   : Servisi GPU modunda başlatır (Local Dev)"
	@echo "make down     : Servisi ve ağları temizler"
	@echo "make logs     : Canlı logları izler"
	@echo "make test     : E2E Test senaryosunu çalıştırır"
	@echo "make clean    : Tüm build artıklarını ve konteynerleri siler"

# CPU Modu: Base + CPU + Override (Local Mounts)
up-cpu:
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.override.yml up --build -d

# GPU Modu: Base + GPU + Override (Local Mounts)
up-gpu:
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml -f docker-compose.override.yml up --build -d

# Temizlik
down:
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.gpu.yml -f docker-compose.override.yml down --remove-orphans

# DÜZELTME: Logs komutuna da tüm konfigürasyon dosyalarını ekledik.
logs:
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.gpu.yml -f docker-compose.override.yml logs -f stt-whisper-service

test:
	./e2e-test.sh

clean:
	rm -rf build/
	@make down
	@echo "🧹 Temizlik tamamlandı."