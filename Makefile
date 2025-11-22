.PHONY: help up-cpu up-gpu down logs clean

help:
	@echo "🎧 Sentiric STT Whisper Service (C++)"
	@echo "-------------------------------------"
	@echo "make up-cpu   : Servisi CPU modunda başlatır (Dev Mode)"
	@echo "make up-gpu   : Servisi GPU modunda başlatır"
	@echo "make down     : Servisi durdurur"
	@echo "make logs     : Logları izler"
	@echo "make clean    : Temizlik"

up-cpu:
	# Override dosyasını da dahil et ki yerel 'models' klasörü mount edilsin.
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.override.yml up --build -d

up-gpu:
	# GPU için de override dosyasını eklemek iyi fikirdir.
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml  up --build -d

down:
	# Down ederken de tüm dosyaları belirtmek en temizidir.
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml -f docker-compose.gpu.yml -f docker-compose.override.yml down --remove-orphans

logs:
	docker compose logs -f stt-whisper-service

clean:
	rm -rf build/
	docker compose down -v