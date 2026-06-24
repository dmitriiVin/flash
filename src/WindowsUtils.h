#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

namespace WindowsUtils
{
std::string FormatSystemError(DWORD errorCode);
std::string WideToUtf8(const std::wstring &value);
std::string OemBytesToUtf8(const std::string &value);
std::string PathToUtf8(const std::filesystem::path &path);
bool IsProcessElevated();
} // namespace WindowsUtils
