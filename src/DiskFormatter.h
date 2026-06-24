#pragma once

#include <filesystem>
#include <string>

class DiskFormatter {
  public:
    bool PrepareUsb(int diskNumber);

    const std::filesystem::path &WinPeRootPath() const;
    const std::filesystem::path &DataRootPath() const;
    const std::string &LastError() const;

  private:
    std::filesystem::path winPeRootPath_;
    std::filesystem::path dataRootPath_;
    std::string lastError_;
};
