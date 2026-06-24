#include "DiskFormatter.h"

#include "ProcessRunner.h"
#include "WindowsUtils.h"

#include <windows.h>

#include <fstream>
#include <optional>
#include <set>
#include <sstream>

namespace
{
bool IsLetterAvailable(wchar_t letter) {
    const DWORD mask = GetLogicalDrives();
    const DWORD bit = 1u << (letter - L'A');
    return (mask & bit) == 0;
}

std::optional<wchar_t> PickDriveLetter(wchar_t preferred, const std::set<wchar_t> &reserved) {
    if (IsLetterAvailable(preferred) && reserved.find(preferred) == reserved.end()) {
        return preferred;
    }

    for (wchar_t letter = L'Z'; letter >= L'E'; --letter) {
        if (IsLetterAvailable(letter) && reserved.find(letter) == reserved.end()) {
            return letter;
        }
    }

    return std::nullopt;
}

std::filesystem::path RootPathForLetter(wchar_t letter) {
    std::wstring value;
    value.push_back(letter);
    value += L":\\";
    return std::filesystem::path(value);
}

bool WaitForPath(const std::filesystem::path &path) {
    for (int attempt = 0; attempt < 50; ++attempt) {
        const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            return true;
        }

        Sleep(200);
    }

    return false;
}

std::filesystem::path CreateTemporaryScriptPath(std::string &error) {
    wchar_t tempPathBuffer[MAX_PATH]{};
    const DWORD tempPathLength = GetTempPathW(MAX_PATH, tempPathBuffer);
    if (tempPathLength == 0 || tempPathLength > MAX_PATH) {
        error = WindowsUtils::FormatSystemError(GetLastError());
        return {};
    }

    wchar_t filePathBuffer[MAX_PATH]{};
    if (GetTempFileNameW(tempPathBuffer, L"usb", 0, filePathBuffer) == 0) {
        error = WindowsUtils::FormatSystemError(GetLastError());
        return {};
    }

    return std::filesystem::path(filePathBuffer);
}

std::string BuildDiskPartScript(int diskNumber, wchar_t winPeLetter, wchar_t dataLetter) {
    std::ostringstream script;
    script << "select disk " << diskNumber << "\n";
    script << "clean\n";
    script << "convert mbr\n";
    script << "create partition primary size=4096\n";
    script << "format fs=fat32 quick label=WINPE\n";
    script << "assign letter=" << static_cast<char>(winPeLetter) << "\n";
    script << "active\n";
    script << "create partition primary\n";
    script << "format fs=ntfs quick label=DATA\n";
    script << "assign letter=" << static_cast<char>(dataLetter) << "\n";
    script << "select partition 1\n";
    script << "active\n";
    script << "exit\n";
    return script.str();
}

std::wstring QuotePath(const std::filesystem::path &path) {
    std::wstring quoted = L"\"";
    quoted += path.wstring();
    quoted += L"\"";
    return quoted;
}
} // namespace

bool DiskFormatter::PrepareUsb(int diskNumber) {
    lastError_.clear();
    winPeRootPath_.clear();
    dataRootPath_.clear();

    std::set<wchar_t> reserved;
    const auto winPeLetter = PickDriveLetter(L'P', reserved);
    if (!winPeLetter) {
        lastError_ = "No available drive letter for WINPE partition.";
        return false;
    }
    reserved.insert(*winPeLetter);

    const auto dataLetter = PickDriveLetter(L'D', reserved);
    if (!dataLetter) {
        lastError_ = "No available drive letter for DATA partition.";
        return false;
    }

    std::string tempError;
    const std::filesystem::path scriptPath = CreateTemporaryScriptPath(tempError);
    if (scriptPath.empty()) {
        lastError_ = "Failed to create temporary DiskPart script: " + tempError;
        return false;
    }

    {
        std::ofstream script(scriptPath, std::ios::binary | std::ios::trunc);
        if (!script) {
            lastError_ = "Failed to write DiskPart script: " + WindowsUtils::PathToUtf8(scriptPath);
            DeleteFileW(scriptPath.wstring().c_str());
            return false;
        }

        script << BuildDiskPartScript(diskNumber, *winPeLetter, *dataLetter);
    }

    ProcessResult result;
    std::string processError;
    const std::wstring command = L"diskpart.exe /s " + QuotePath(scriptPath);
    const bool started = RunProcessCapture(command, result, processError);
    DeleteFileW(scriptPath.wstring().c_str());

    if (!started) {
        lastError_ = "Failed to start DiskPart: " + processError;
        return false;
    }

    if (result.exitCode != 0) {
        lastError_ = "DiskPart failed with exit code " + std::to_string(result.exitCode);
        if (!result.output.empty()) {
            lastError_ += "\n" + result.output;
        }
        return false;
    }

    winPeRootPath_ = RootPathForLetter(*winPeLetter);
    dataRootPath_ = RootPathForLetter(*dataLetter);

    if (!WaitForPath(winPeRootPath_) || !WaitForPath(dataRootPath_)) {
        lastError_ = "DiskPart completed, but assigned partition drive letters are not available yet.";
        return false;
    }

    return true;
}

const std::filesystem::path &DiskFormatter::WinPeRootPath() const {
    return winPeRootPath_;
}

const std::filesystem::path &DiskFormatter::DataRootPath() const {
    return dataRootPath_;
}

const std::string &DiskFormatter::LastError() const {
    return lastError_;
}
