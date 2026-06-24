# UsbBuilder

UsbBuilder is a C++20 / Qt6 Widgets utility for preparing a Windows installation USB drive with two partitions:

- `WINPE` FAT32 boot partition for Microsoft WinPE files.
- `DATA` NTFS partition for `install.esd`, `InstallerAgent.exe`, `config.json`, `Software`, and `Drivers`.

The application targets Windows 10/11 and MSVC 2022.

## Build

Requirements:

- Windows 10/11
- Visual Studio 2022 with MSVC
- Qt 6 for MSVC
- CMake 3.21+

Example:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
cmake --build build --config Release
```

## Runtime Notes

Run UsbBuilder as Administrator. Disk preparation is performed with `diskpart.exe`, and all data on the selected disk is deleted.

The selected WinPE source should point to either the WinPE media root or a directory containing a `media` subdirectory produced by Microsoft ADK tools.
