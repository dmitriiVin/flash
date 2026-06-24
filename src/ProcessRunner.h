#pragma once

#include <string>

#include <windows.h>

struct ProcessResult {
    DWORD exitCode = 1;
    std::string output;
};

bool RunProcessCapture(const std::wstring &commandLine, ProcessResult &result, std::string &error);
