.PHONY: help up-cpu up-gpu down logs clean

help:
	@echo "🎧 Sentiric STT Whisper Service (C++)"
	@echo "-------------------------------------"
	@echo "make up-cpu   : Servisi CPU modunda başlatır"
	@echo "make up-gpu   : Servisi GPU modunda başlatır"
	@echo "make down     : Servisi durdurur"
	@echo "make logs     : Logları izler"
	@echo "make clean    : Temizlik"

up-cpu:
	docker compose -f docker-compose.yml -f docker-compose.cpu.yml up --build -d

up-gpu:
	docker compose -f docker-compose.yml -f docker-compose.gpu.yml up --build -d

down:
	docker compose down --remove-orphans

logs:
	docker compose logs -f stt-whisper-service

clean:
	rm -rf build/
	docker compose down -v