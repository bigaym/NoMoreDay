#pragma once

#include <cstddef>
#include <utility>

#include <spdlog/fmt/fmt.h>

namespace NoMoreDay::utils {

template <size_t N, typename... Args>
void FormatToBuffer(char (&buffer)[N], fmt::format_string<Args...> format,
                    Args &&...args) {
  static_assert(N > 0, "Format buffer must not be empty");
  const auto result =
      fmt::format_to_n(buffer, N - 1, format, std::forward<Args>(args)...);
  const size_t written = result.size < (N - 1) ? result.size : (N - 1);
  buffer[written] = '\0';
}

template <typename... Args>
void FormatToBuffer(char *buffer, size_t bufferSize,
                    fmt::format_string<Args...> format, Args &&...args) {
  if (buffer == nullptr || bufferSize == 0) {
    return;
  }
  const auto result = fmt::format_to_n(buffer, bufferSize - 1, format,
                                       std::forward<Args>(args)...);
  const size_t written =
      result.size < (bufferSize - 1) ? result.size : (bufferSize - 1);
  buffer[written] = '\0';
}

} // namespace NoMoreDay::utils
