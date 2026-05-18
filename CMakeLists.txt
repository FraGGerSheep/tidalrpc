cmake_minimum_required(VERSION 3.16)
project(tidalrpc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    # --- Windows: Tray-App, C++/WinRT + Win32 ---
    enable_language(RC)
    add_executable(tidalrpc WIN32 src/main.cpp src/tidalrpc.rc)
    target_include_directories(tidalrpc PRIVATE src)
    target_compile_definitions(tidalrpc PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(tidalrpc PRIVATE
        winhttp windowsapp ole32 shell32 user32)
    if(MSVC)
        set_property(TARGET tidalrpc PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded")
        target_compile_options(tidalrpc PRIVATE /EHsc /O2 /utf-8)
    endif()
else()
    # --- Linux/macOS: Terminal-App, Unix-Socket + libcurl ---
    if(APPLE)
        enable_language(OBJCXX)
        set(MEDIA_SRC src/posix/media_mac.mm)
    else()
        set(MEDIA_SRC src/posix/media_linux.cpp)
    endif()

    add_executable(tidalrpc src/posix/main.cpp ${MEDIA_SRC})
    target_compile_options(tidalrpc PRIVATE -O2 -Wall)

    find_package(CURL REQUIRED)
    target_link_libraries(tidalrpc PRIVATE CURL::libcurl)

    if(APPLE)
        # MediaRemote-Wrapper unter ARC kompilieren.
        set_source_files_properties(src/posix/media_mac.mm
            PROPERTIES COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(tidalrpc PRIVATE "-framework Foundation")
    endif()
endif()
