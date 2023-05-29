#include <chrono>

#include "file.hpp"
#include "util/misc.hpp"

auto FileManager::get_next_path() const -> Path {
    const auto duration = std::chrono::system_clock::now().time_since_epoch();
    const auto ms       = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    const auto sec = std::time_t(ms / 1000);
    const auto lt  = std::localtime(&sec);

    auto buf = std::array<char, 24>();
    sprintf(buf.data(), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            lt->tm_year + 1900,
            lt->tm_mon + 1,
            lt->tm_mday,
            lt->tm_hour,
            lt->tm_min,
            lt->tm_sec,
            int(ms % 1000));

    return Path(savedir) / buf.data();
}
