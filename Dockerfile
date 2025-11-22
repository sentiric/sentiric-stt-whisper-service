# --- Derleme Aşaması ---
FROM ubuntu:24.04 AS builder

# 1. Temel bağımlılıklar
RUN apt-get update && apt-get install -y --no-install-recommends \
    git cmake build-essential curl zip unzip tar \
    pkg-config ninja-build ca-certificates python3

# 2. Vcpkg Kurulumu
ARG VCPKG_VERSION=2024.05.24
RUN curl -L "https://github.com/microsoft/vcpkg/archive/refs/tags/${VCPKG_VERSION}.tar.gz" -o vcpkg.tar.gz && \
    mkdir -p /opt/vcpkg && \
    tar xzf vcpkg.tar.gz -C /opt/vcpkg --strip-components=1 && \
    rm vcpkg.tar.gz && \
    /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /app

# 3. Bağımlılıkları Derle (vcpkg)
COPY vcpkg.json .
RUN /opt/vcpkg/vcpkg install --triplet x64-linux

# 4. whisper.cpp'yi Klonla (Sabit Commit)
# Not: Llama.cpp yerine Whisper.cpp kullanıyoruz.
ARG WHISPER_CPP_VERSION=v1.7.1
RUN git clone https://github.com/ggerganov/whisper.cpp.git whisper.cpp && \
    cd whisper.cpp && \
    git checkout ${WHISPER_CPP_VERSION}

# 5. Kaynak Kodunu Kopyala
COPY src ./src
COPY CMakeLists.txt .

# 6. Projeyi Derle
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DWHISPER_BUILD_TESTS=OFF \
    -DWHISPER_BUILD_EXAMPLES=OFF
RUN cmake --build build --target all -j $(nproc)

# --- Çalışma Aşaması ---
FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates libgomp1 curl libsndfile1 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/stt_service /usr/local/bin/
COPY --from=builder /app/build/stt_cli /usr/local/bin/

# Paylaşılan kütüphaneleri taşı
COPY --from=builder /app/vcpkg_installed/x64-linux/lib/*.so* /usr/local/lib/
COPY --from=builder /app/build/bin/*.so /usr/local/lib/
RUN ldconfig

WORKDIR /app
RUN mkdir -p /models

# STT Servisi Portları (Eski Python servisi ile aynı)
EXPOSE 15030 15031

CMD ["stt_service"]