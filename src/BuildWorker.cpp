#include "BuildWorker.h"

#include "DiskFormatter.h"
#include "FileCopier.h"
#include "WinPEBuilder.h"
#include "WindowsUtils.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace
{
std::filesystem::path ToPath(const QString &value) {
    return std::filesystem::path(value.toStdWString());
}

QString ToQString(const std::string &value) {
    return QString::fromUtf8(value.c_str());
}

bool CopyFileAs(const std::filesystem::path &source, const std::filesystem::path &destination, std::string &error) {
    std::error_code ec;
    if (!std::filesystem::exists(source, ec) || !std::filesystem::is_regular_file(source, ec)) {
        error = "Source file does not exist: " + WindowsUtils::PathToUtf8(source);
        return false;
    }

    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        error = "Failed to create destination directory: " + WindowsUtils::PathToUtf8(destination.parent_path()) + ". " + ec.message();
        return false;
    }

    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        error = "Failed to copy file " + WindowsUtils::PathToUtf8(source) + " to " + WindowsUtils::PathToUtf8(destination) + ". " + ec.message();
        return false;
    }

    return true;
}

bool VerifyExists(const std::filesystem::path &path, std::string &error) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return true;
    }

    error = "Required item is missing: " + WindowsUtils::PathToUtf8(path);
    return false;
}
} // namespace

BuildWorker::BuildWorker(BuildRequest request, QObject *parent) : QObject(parent), request_(std::move(request)) {
}

void BuildWorker::Run() {
    emit LogMessage("Preparing disk...");
    emit LogMessage("Formatting FAT32...");
    emit LogMessage("Formatting NTFS...");

    DiskFormatter formatter;
    if (!formatter.PrepareUsb(request_.diskNumber)) {
        emit Finished(false, ToQString(formatter.LastError()));
        return;
    }

    const auto winPeRoot = formatter.WinPeRootPath();
    const auto dataRoot = formatter.DataRootPath();

    emit LogMessage("Copying WinPE...");
    WinPEBuilder winPeBuilder;
    if (!winPeBuilder.DeployWinPE(ToPath(request_.winPeSource), winPeRoot)) {
        emit Finished(false, ToQString(winPeBuilder.LastError()));
        return;
    }

    if (!request_.installerLauncher.trimmed().isEmpty()) {
        emit LogMessage("Copying InstallerLauncher...");
        std::string error;
        if (!CopyFileAs(ToPath(request_.installerLauncher), winPeRoot / L"InstallerLauncher.exe", error)) {
            emit Finished(false, ToQString(error));
            return;
        }
    }

    emit LogMessage("Copying install.esd...");
    std::string error;
    if (!CopyFileAs(ToPath(request_.installEsd), dataRoot / L"install.esd", error)) {
        emit Finished(false, ToQString(error));
        return;
    }

    emit LogMessage("Copying InstallerAgent...");
    if (!CopyFileAs(ToPath(request_.installerAgent), dataRoot / L"InstallerAgent.exe", error)) {
        emit Finished(false, ToQString(error));
        return;
    }

    emit LogMessage("Copying config.json...");
    if (!CopyFileAs(ToPath(request_.configJson), dataRoot / L"config.json", error)) {
        emit Finished(false, ToQString(error));
        return;
    }

    emit LogMessage("Copying Software...");
    FileCopier copier;
    if (!copier.CopyDirectory(ToPath(request_.softwareDirectory), dataRoot / L"Software")) {
        emit Finished(false, ToQString(copier.LastError()));
        return;
    }

    emit LogMessage("Copying Drivers...");
    std::error_code ec;
    if (request_.driversDirectory.trimmed().isEmpty()) {
        std::filesystem::create_directories(dataRoot / L"Drivers", ec);
        if (ec) {
            emit Finished(false, ToQString("Failed to create Drivers directory: " + ec.message()));
            return;
        }
    }
    else if (!copier.CopyDirectory(ToPath(request_.driversDirectory), dataRoot / L"Drivers")) {
        emit Finished(false, ToQString(copier.LastError()));
        return;
    }

    emit LogMessage("Verifying files...");
    const std::filesystem::path requiredItems[] = {
        winPeRoot / L"EFI", winPeRoot / L"bootmgr", winPeRoot / L"sources" / L"boot.wim", dataRoot / L"install.esd", dataRoot / L"InstallerAgent.exe",
    };

    for (const auto &item : requiredItems) {
        if (!VerifyExists(item, error)) {
            emit Finished(false, ToQString(error));
            return;
        }
    }

    emit LogMessage("Completed.");
    emit Finished(true, "USB creation completed successfully.");
}
