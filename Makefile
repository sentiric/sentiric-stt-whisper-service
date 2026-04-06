.PHONY: all setup check lint build clean test

all: setup clean lint build

setup:
	@echo "🛠️ Kalite araçları kontrol ediliyor..."
	@command -v clang-format >/dev/null 2>&1 || { echo >&2 "clang-format eksik. Kuruluyor..."; sudo apt-get install -y clang-format; }
	@command -v cmake >/dev/null 2>&1 || { echo >&2 "cmake eksik. Kuruluyor..."; sudo apt-get install -y cmake ninja-build; }
	@if [ ! -f .clang-format ]; then echo "BasedOnStyle: Google" > .clang-format; fi

check:
	@echo "🔍 CMake Check başlatılıyor..."

lint:
	@echo "🧹 Linter (clang-format) çalıştırılıyor..."
	find src -name "*.cpp" -o -name "*.h" | xargs clang-format -i -style=file

build: check
	@echo "🏗️ Build alınıyor..."

test:
	@echo "🧪 E2E Testi Başlatılıyor..."

clean:
	@echo "🗑️ Eski build artıkları temizleniyor..."