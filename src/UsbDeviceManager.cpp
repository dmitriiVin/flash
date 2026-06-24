#include "UsbDeviceManager.h"

#include "WindowsUtils.h"

#include <windows.h>
#include <winioctl.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <set>
#include <string>

namespace
{
std::wstring PhysicalDrivePath(int number) {
    return L"\\\\.\\PhysicalDrive" + std::to_wstring(number);
}

std::string Trim(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

std::optional<int> DeviceNumberFromDriveLetter(wchar_t letter) {
    std::wstring volumePath = L"\\\\.\\";
    volumePath.push_back(letter);
    volumePath += L":";

    HANDLE handle = CreateFileW(volumePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    STORAGE_DEVICE_NUMBER deviceNumber{};
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &deviceNumber, sizeof(deviceNumber), &bytesReturned, nullptr);
    CloseHandle(handle);

    if (!ok) {
        return std::nullopt;
    }

    return static_cast<int>(deviceNumber.DeviceNumber);
}

uint64_t QueryDiskSize(int number) {
    HANDLE handle = CreateFileW(PhysicalDrivePath(number).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    GET_LENGTH_INFORMATION length{};
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &length, sizeof(length), &bytesReturned, nullptr);
    CloseHandle(handle);

    return ok ? static_cast<uint64_t>(length.Length.QuadPart) : 0;
}

struct DiskDescriptor {
    bool valid = false;
    bool removable = false;
    bool usb = false;
    std::string model;
};

std::string DescriptorString(const STORAGE_DEVICE_DESCRIPTOR *descriptor, DWORD offset) {
    if (offset == 0 || offset >= descriptor->Size) {
        return {};
    }

    const auto *begin = reinterpret_cast<const char *>(descriptor) + offset;
    return Trim(begin);
}

DiskDescriptor QueryDiskDescriptor(int number) {
    HANDLE handle = CreateFileW(PhysicalDrivePath(number).c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return {};
    }

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE buffer[4096]{};
    DWORD bytesReturned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer, sizeof(buffer), &bytesReturned, nullptr);
    CloseHandle(handle);

    if (!ok || bytesReturned < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        return {};
    }

    const auto *descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR *>(buffer);
    DiskDescriptor result;
    result.valid = true;
    result.removable = descriptor->RemovableMedia != FALSE;
    result.usb = descriptor->BusType == BusTypeUsb;

    const auto vendor = DescriptorString(descriptor, descriptor->VendorIdOffset);
    const auto product = DescriptorString(descriptor, descriptor->ProductIdOffset);
    const auto revision = DescriptorString(descriptor, descriptor->ProductRevisionOffset);

    result.model = Trim(vendor + " " + product + " " + revision);
    if (result.model.empty()) {
        result.model = "PhysicalDrive" + std::to_string(number);
    }

    return result;
}

std::set<int> EnumerateMountedRemovableDeviceNumbers() {
    std::set<int> numbers;
    const DWORD mask = GetLogicalDrives();

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        const DWORD bit = 1u << (letter - L'A');
        if ((mask & bit) == 0) {
            continue;
        }

        wchar_t root[] = {letter, L':', L'\\', L'\0'};
        const UINT driveType = GetDriveTypeW(root);
        if (driveType != DRIVE_REMOVABLE) {
            continue;
        }

        if (auto number = DeviceNumberFromDriveLetter(letter)) {
            numbers.insert(*number);
        }
    }

    return numbers;
}
} // namespace

std::vector<UsbDevice> EnumerateUsbDevices() {
    std::set<int> candidates = EnumerateMountedRemovableDeviceNumbers();

    for (int number = 0; number < 64; ++number) {
        const DiskDescriptor descriptor = QueryDiskDescriptor(number);
        if (!descriptor.valid) {
            continue;
        }

        if (descriptor.usb || descriptor.removable) {
            candidates.insert(number);
        }
    }

    std::vector<UsbDevice> devices;
    devices.reserve(candidates.size());

    for (int number : candidates) {
        const DiskDescriptor descriptor = QueryDiskDescriptor(number);
        if (!descriptor.valid) {
            continue;
        }

        UsbDevice device;
        device.number = number;
        device.model = descriptor.model;
        device.sizeBytes = QueryDiskSize(number);
        devices.push_back(device);
    }

    std::sort(devices.begin(), devices.end(), [](const UsbDevice &lhs, const UsbDevice &rhs) { return lhs.number < rhs.number; });

    return devices;
}
