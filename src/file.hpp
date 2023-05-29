#pragma once
#include <filesystem>

class FileManager {
  private:
    using Path = std::filesystem::path;

    std::string savedir;

  public:
    auto get_next_path() const -> Path;

    FileManager(const std::string_view savedir)
        : savedir(savedir) {}
};
