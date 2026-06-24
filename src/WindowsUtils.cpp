#include "WindowsUtils.h"

#include <vector>

namespace WindowsUtils
{
std::string WideToUtf8(const std::wstring &value) {
    if (value.empty()) {
        return {};
    }

    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), bytes, nullptr, nullptr);
    return result;
}

std::string OemBytesToUtf8(const std::string &value) {
    if (value.empty()) {
        return {};
    }

    const int chars = MultiByteToWideChar(CP_OEMCP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (chars <= 0) {
        return value;
    }

    std::wstring wide(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, value.data(), static_cast<int>(value.size()), wide.data(), chars);
    return WideToUtf8(wide);
}

std::string FormatSystemError(DWORD errorCode) {
    wchar_t *message = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    if (length == 0 || message == nullptr) {
        return "Windows error " + std::to_string(errorCode);
    }

    std::wstring wide(message, length);
    LocalFree(message);

    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ')) {
        wide.pop_back();
    }

    return WideToUtf8(wide);
}

std::string PathToUtf8(const std::filesystem::path &path) {
    return WideToUtf8(path.wstring());
}

bool IsProcessElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD bytesReturned = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytesReturned);
    CloseHandle(token);

    return ok && elevation.TokenIsElevated != 0;
}
} // namespace WindowsUtils
