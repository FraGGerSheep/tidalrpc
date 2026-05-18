cmake_minimum_required(VERSION 3.20)
project(tidalrpc CXX)

enable_language(RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WIN32 -> /SUBSYSTEM:WINDOWS, Einstieg wWinMain, kein Konsolenfenster.
add_executable(tidalrpc WIN32 src/main.cpp src/tidalrpc.rc)
target_include_directories(tidalrpc PRIVATE src)
target_compile_definitions(tidalrpc PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_link_libraries(tidalrpc PRIVATE winhttp windowsapp ole32 shell32 user32)

if(MSVC)
    # Statische CRT -> standalone exe, keine Redist noetig.
    set_property(TARGET tidalrpc PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(tidalrpc PRIVATE /EHsc /O2 /utf-8)
endif()
