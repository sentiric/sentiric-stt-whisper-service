cmake_minimum_required(VERSION 3.20)

# CUDA tespiti ve Proje Tanımı
if(DEFINED GGML_CUDA AND GGML_CUDA)
    project(stt-whisper-service CXX CUDA)
else()
    project(stt-whisper-service CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Güvenlik ve Uyumluluk Bayrakları
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wno-unused-function")

# --- MİMARİ DÜZELTME: PORTABILITY FIX (Exit Code 132 Prevention) ---
# Varsayılan olarak whisper.cpp "-march=native" kullanır. Bu, imajın derlendiği
# makine ile çalıştığı makine farklıysa (örn: CI vs Eski Desktop) SIGILL hatasına yol açar.
# Bunu engellemek için NATIVE optimizasyonu kapatıyoruz.
set(GGML_NATIVE OFF CACHE BOOL "Disable native instruction set optimization for portability" FORCE)

# Eğer MSVC değilse (Linux/GCC), generic x86-64 seviyesini belirleyebiliriz.
# Ancak GGML_NATIVE=OFF yeterlidir, cmake varsayılanları kullanır.
if(NOT MSVC)
    # Opsiyonel: Daha geniş uyumluluk için.
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64 -mtune=generic")
endif()
# -------------------------------------------------------------------

# --- Bağımlılıklar (vcpkg) ---
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(prometheus-cpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(SampleRate CONFIG REQUIRED) 

if(GGML_CUDA)
    find_package(CUDAToolkit REQUIRED)
endif()

# --- Whisper.cpp Entegrasyonu ---
set(WHISPER_BUILD_COMMON ON CACHE BOOL "Force building of common library" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)

add_subdirectory(whisper.cpp)

set(WHISPER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/include)
set(WHISPER_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/common)
set(WHISPER_EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/examples)

# --- gRPC Contracts (Sentiric) ---
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.12.0
)
FetchContent_MakeAvailable(sentiric_contracts)

set(PROTO_FILES
    "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/stt/v1/whisper.proto"
    "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/stt/v1/gateway.proto"
)
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_GENERATED_SRCS
    "${GENERATED_DIR}/sentiric/stt/v1/whisper.pb.cc"
    "${GENERATED_DIR}/sentiric/stt/v1/whisper.grpc.pb.cc"
    "${GENERATED_DIR}/sentiric/stt/v1/gateway.pb.cc"
    "${GENERATED_DIR}/sentiric/stt/v1/gateway.grpc.pb.cc"
)
set(PROTO_GENERATED_HDRS
    "${GENERATED_DIR}/sentiric/stt/v1/whisper.pb.h"
    "${GENERATED_DIR}/sentiric/stt/v1/whisper.grpc.pb.h"
    "${GENERATED_DIR}/sentiric/stt/v1/gateway.pb.h"
    "${GENERATED_DIR}/sentiric/stt/v1/gateway.grpc.pb.h"
)

add_custom_command(
    OUTPUT ${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND $<TARGET_FILE:protobuf::protoc>
        --proto_path=${sentiric_contracts_SOURCE_DIR}/proto
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

add_library(proto_lib STATIC
    ${PROTO_GENERATED_SRCS}
    ${PROTO_GENERATED_HDRS}
)
set_source_files_properties(${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS} PROPERTIES GENERATED TRUE)
target_include_directories(proto_lib PUBLIC ${GENERATED_DIR})
target_link_libraries(proto_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

# --- Ana Servis Executable ---
add_executable(stt_service
    src/main.cpp
    src/stt_engine.cpp
    src/model_manager.cpp
    src/grpc_server.cpp
    src/http_server.cpp
    src/prosody_extractor.cpp
    src/speaker_cluster.cpp
)
add_dependencies(stt_service proto_lib)

target_include_directories(stt_service PRIVATE
    src
    ${WHISPER_INCLUDE_DIR}
    ${WHISPER_COMMON_INCLUDE_DIR}
    ${WHISPER_EXAMPLES_DIR}
)

target_link_libraries(stt_service PRIVATE
    proto_lib
    whisper
    spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib c-ares::cares Threads::Threads
    prometheus-cpp::core prometheus-cpp::pull
    SampleRate::samplerate
    fmt::fmt
)

if(GGML_CUDA)
    target_link_libraries(stt_service PRIVATE CUDA::cudart CUDA::cuda_driver)
endif()

# --- CLI Tool (Test İçin) ---
add_executable(stt_cli
    src/cli/main.cpp
    src/cli/audio_client.cpp
)
add_dependencies(stt_cli proto_lib)
target_include_directories(stt_cli PRIVATE src)
target_link_libraries(stt_cli PRIVATE proto_lib spdlog::spdlog nlohmann_json::nlohmann_json Threads::Threads fmt::fmt)