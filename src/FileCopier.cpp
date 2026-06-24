#include "FileCopier.h"

#include "WindowsUtils.h"

#include <system_error>

bool FileCopier::CopyDirectory(const std::filesystem::path &source, const std::filesystem::path &destination) {
    lastError_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(source, ec) || !std::filesystem::is_directory(source, ec)) {
        lastError_ = "Source directory does not exist: " + WindowsUtils::PathToUtf8(source);
        return false;
    }

    std::filesystem::create_directories(destination, ec);
    if (ec) {
        lastError_ = "Failed to create destination directory: " + WindowsUtils::PathToUtf8(destination) + ". " + ec.message();
        return false;
    }

    for (const auto &entry : std::filesystem::recursive_directory_iterator(source, ec)) {
        if (ec) {
            lastError_ = "Failed to enumerate directory: " + WindowsUtils::PathToUtf8(source) + ". " + ec.message();
            return false;
        }

        const auto relative = std::filesystem::relative(entry.path(), source, ec);
        if (ec) {
            lastError_ = "Failed to resolve relative path for: " + WindowsUtils::PathToUtf8(entry.path()) + ". " + ec.message();
            return false;
        }

        const auto target = destination / relative;

        if (entry.is_directory(ec)) {
            std::filesystem::create_directories(target, ec);
            if (ec) {
                lastError_ = "Failed to create directory: " + WindowsUtils::PathToUtf8(target) + ". " + ec.message();
                return false;
            }
        }
        else if (entry.is_regular_file(ec)) {
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                lastError_ = "Failed to create directory: " + WindowsUtils::PathToUtf8(target.parent_path()) + ". " + ec.message();
                return false;
            }

            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                lastError_ = "Failed to copy file " + WindowsUtils::PathToUtf8(entry.path()) + " to " + WindowsUtils::PathToUtf8(target) + ". " + ec.message();
                return false;
            }
        }
    }

    return true;
}

const std::string &FileCopier::LastError() const {
    return lastError_;
}
