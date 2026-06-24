#pragma once

#include <filesystem>
#include <string>

class WinPEBuilder {
  public:
    bool DeployWinPE(const std::filesystem::path &source, const std::filesystem::path &destination);

    const std::string &LastError() const;

  private:
    std::string lastError_;
};
