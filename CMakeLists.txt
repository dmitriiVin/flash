cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(UsbBuilder VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "UsbBuilder is a Windows 10/11 desktop utility and must be built on Windows.")
endif()

option(USB_BUILDER_DEPLOY_QT "Deploy Qt DLLs with windeployqt after build" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(UsbBuilder WIN32
    src/BuildWorker.cpp
    src/BuildWorker.h
    src/DiskFormatter.cpp
    src/DiskFormatter.h
    src/FileCopier.cpp
    src/FileCopier.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/ProcessRunner.cpp
    src/ProcessRunner.h
    src/UsbDeviceManager.cpp
    src/UsbDeviceManager.h
    src/WinPEBuilder.cpp
    src/WinPEBuilder.h
    src/WindowsUtils.cpp
    src/WindowsUtils.h
    src/main.cpp
)

target_compile_features(UsbBuilder PRIVATE cxx_std_20)

target_compile_definitions(UsbBuilder PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(UsbBuilder PRIVATE
    Qt6::Widgets
    Advapi32
)

if(MSVC)
    target_link_options(UsbBuilder PRIVATE
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
    )

    set_property(TARGET UsbBuilder PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

if(USB_BUILDER_DEPLOY_QT)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${QT_BIN_DIR}"
    )

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(FATAL_ERROR "windeployqt was not found. Check CMAKE_PREFIX_PATH and Qt installation.")
    endif()

    add_custom_command(TARGET UsbBuilder POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            $<IF:$<CONFIG:Debug>,--debug,--release>
            --compiler-runtime
            --no-translations
            "$<TARGET_FILE:UsbBuilder>"
        COMMENT "Deploying Qt runtime"
        VERBATIM
    )
endif()

if(MSVC)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)

    foreach(RUNTIME_DLL ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET UsbBuilder POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RUNTIME_DLL}"
                "$<TARGET_FILE_DIR:UsbBuilder>"
            COMMENT "Copying MSVC runtime: ${RUNTIME_DLL}"
            VERBATIM
        )
    endforeach()
endif()

set_target_properties(UsbBuilder PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
