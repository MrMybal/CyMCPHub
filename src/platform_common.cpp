#include "platform.hpp"

#include <ctime>

namespace hub {
fs::path from_utf8(std::string_view value) { return fs::path(std::u8string(value.begin(), value.end())); }

std::string local_time(std::time_t time) {
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &time) != 0) return {};
#else
    if (!localtime_r(&time, &local)) return {};
#endif
    char value[16]{};
    std::strftime(value, sizeof(value), "%H:%M:%S", &local);
    return value;
}
} // namespace hub
