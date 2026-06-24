#pragma once

#include <filesystem>
#include <string>

class FileCopier {
  public:
    bool CopyDirectory(const std::filesystem::path &source, const std::filesystem::path &destination);

    const std::string &LastError() const;

  private:
    std::string lastError_;
};
