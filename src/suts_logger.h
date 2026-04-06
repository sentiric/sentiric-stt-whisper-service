// Dosya: src/suts_logger.h
#pragma once
#include <fmt/core.h>

#include <chrono>
#include <cstdlib>
#include <ctime>

#include "nlohmann/json.hpp"
#include "spdlog/pattern_formatter.h"
#include "spdlog/spdlog.h"

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

namespace suts {
template <typename... Args>
inline std::string build(const std::string& event, const std::string& trace_id,
                         const std::string& span_id,
                         const std::string& tenant_id,
                         fmt::format_string<Args...> fmt, Args&&... args) {
  nlohmann::json j;
  j["_is_suts"] = true;
  j["event"] = event;

  if (trace_id.empty() || trace_id == "unknown")
    j["trace_id"] = nullptr;
  else
    j["trace_id"] = trace_id;
  if (span_id.empty() || span_id == "unknown")
    j["span_id"] = nullptr;
  else
    j["span_id"] = span_id;
  if (tenant_id.empty() || tenant_id == "unknown")
    j["tenant_id"] = nullptr;
  else
    j["tenant_id"] = tenant_id;

  j["message"] = fmt::format(fmt, std::forward<Args>(args)...);
  return j.dump();
}

class SutsFormatter : public spdlog::formatter {
 public:
  void format(const spdlog::details::log_msg& msg,
              spdlog::memory_buf_t& dest) override {
    nlohmann::json j;
    j["schema_v"] = "1.0.0";

    auto time_ms =
        std::chrono::time_point_cast<std::chrono::milliseconds>(msg.time);
    auto time_t = std::chrono::system_clock::to_time_t(time_ms);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t);
#else
    gmtime_r(&time_t, &tm_buf);
#endif
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    int ms = time_ms.time_since_epoch().count() % 1000;
    j["ts"] = fmt::format("{}.{:03}Z", time_str, ms);

    std::string severity(spdlog::level::to_string_view(msg.level).data());
    std::transform(severity.begin(), severity.end(), severity.begin(),
                   ::toupper);
    if (severity == "ERR") severity = "ERROR";
    j["severity"] = severity;

    const char* env_p = std::getenv("ENV");
    const char* host = std::getenv("HOSTNAME");
    j["resource"] = {{"service.name", "stt-whisper-service"},
                     {"service.version", APP_VERSION},
                     {"service.env", env_p ? env_p : "production"},
                     {"host.name", host ? host : "unknown"}};

    std::string payload = fmt::to_string(msg.payload);

    if (!payload.empty() && payload[0] == '{') {
      try {
        auto parsed = nlohmann::json::parse(payload);
        if (parsed.contains("_is_suts") && parsed["_is_suts"].get<bool>()) {
          j["event"] = parsed.value("event", "LOG_EVENT");
          j["trace_id"] = parsed["trace_id"];
          j["span_id"] = parsed["span_id"];
          j["tenant_id"] = parsed["tenant_id"];
          j["message"] = parsed.value("message", "");

          std::string out = j.dump() + "\n";
          dest.append(out.data(), out.data() + out.size());
          return;
        }
      } catch (...) {
      }
    }

    j["trace_id"] = nullptr;
    j["span_id"] = nullptr;
    j["tenant_id"] = nullptr;
    j["event"] = "LOG_EVENT";
    j["message"] = payload;

    std::string out = j.dump() + "\n";
    dest.append(out.data(), out.data() + out.size());
  }

  std::unique_ptr<spdlog::formatter> clone() const override {
    return std::make_unique<SutsFormatter>();
  }
};
}  // namespace suts

#define SUTS_INFO(event, tid, sid, ten, ...) \
  spdlog::info(suts::build(event, tid, sid, ten, __VA_ARGS__))
#define SUTS_ERROR(event, tid, sid, ten, ...) \
  spdlog::error(suts::build(event, tid, sid, ten, __VA_ARGS__))
#define SUTS_WARN(event, tid, sid, ten, ...) \
  spdlog::warn(suts::build(event, tid, sid, ten, __VA_ARGS__))
#define SUTS_DEBUG(event, tid, sid, ten, ...) \
  spdlog::debug(suts::build(event, tid, sid, ten, __VA_ARGS__))