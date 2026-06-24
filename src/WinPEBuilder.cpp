#include "WinPEBuilder.h"

#include "FileCopier.h"
#include "WindowsUtils.h"

#include <system_error>

namespace
{
std::filesystem::path ResolveWinPeMediaRoot(const std::filesystem::path &source) {
    std::error_code ec;
    const auto media = source / L"media";
    if (std::filesystem::exists(media / L"EFI", ec) && std::filesystem::exists(media / L"boot", ec) && std::filesystem::exists(media / L"sources" / L"boot.wim", ec)) {
        return media;
    }

    return source;
}
} // namespace

bool WinPEBuilder::DeployWinPE(const std::filesystem::path &source, const std::filesystem::path &destination) {
    lastError_.clear();

    std::error_code ec;
    const auto root = ResolveWinPeMediaRoot(source);
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        lastError_ = "WinPE source directory does not exist: " + WindowsUtils::PathToUtf8(source);
        return false;
    }

    const auto efi = root / L"EFI";
    const auto boot = root / L"boot";
    const auto bootmgr = root / L"bootmgr";
    const auto bootWim = root / L"sources" / L"boot.wim";

    if (!std::filesystem::exists(efi, ec) || !std::filesystem::exists(boot, ec) || !std::filesystem::exists(bootmgr, ec) || !std::filesystem::exists(bootWim, ec)) {
        lastError_ = "WinPE source must contain EFI, boot, bootmgr, and sources\\boot.wim.";
        return false;
    }

    FileCopier copier;
    if (!copier.CopyDirectory(root, destination)) {
        lastError_ = copier.LastError();
        return false;
    }

    return true;
}

const std::string &WinPEBuilder::LastError() const {
    return lastError_;
}
