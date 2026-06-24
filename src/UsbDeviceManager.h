#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct UsbDevice {
    int number;
    std::string model;
    uint64_t sizeBytes;
};

std::vector<UsbDevice> EnumerateUsbDevices();
