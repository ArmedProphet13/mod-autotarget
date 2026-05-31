# AutoTarget client DLL.
#
# Builds a 32-bit proxy DLL (shipped as version.dll) for the WoW 3.3.5a client,
# plus an off-client unit-test executable for the pure engine layer.
#
# Configure (from this directory):
#   cmake -B build -A Win32
#   cmake --build build --config Release
#
# Outputs:
#   build/Release/version.dll       - the proxy DLL to drop in the WoW folder
#   build/Release/autotarget_tests  - the engine unit tests

cmake_minimum_required(VERSION 3.20)
project(AutoTarget C CXX)   # C is needed for the vendored MinHook sources

# --- Toolchain guards -------------------------------------------------------
# Every offset in GameInterface/Offsets.h assumes the 32-bit Win32 client.
if(NOT MSVC)
    message(FATAL_ERROR "AutoTarget must be built with MSVC.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "AutoTarget must be built 32-bit. Re-configure with: -A Win32")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT: the DLL drops into the WoW folder with no VC++ redistributable.
# CMP0091 is NEW at our 3.20 minimum, so this propagates to every target below.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- MinHook ----------------------------------------------------------------
# Fetched as source and built here with our own toolchain settings, rather than
# via MinHook's own CMakeLists - that keeps the CRT choice and 32-bit target
# consistent and avoids depending on the tag carrying build files.
include(FetchContent)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)  # keep FetchContent_Populate usable
endif()
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde32.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

# --- The proxy DLL ----------------------------------------------------------
file(GLOB_RECURSE AUTOTARGET_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(autotarget SHARED
    ${AUTOTARGET_SOURCES}
    src/Bootstrap/version.def   # supplies the proxied version.dll export names
)

target_include_directories(autotarget PRIVATE src)
target_link_libraries(autotarget PRIVATE minhook user32)

target_compile_definitions(autotarget PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# Ship as version.dll so the WoW loader picks it up via the DLL search order.
# (version.dll is statically imported by WoW.exe for GetFileVersionInfo*, and is
# rarely replaced by graphics wrappers - much lower install-collision risk than
# d3d9.dll, which DXVK / HD packs / ReShade routinely overwrite.)
set_target_properties(autotarget PROPERTIES
    OUTPUT_NAME "version"
    PREFIX ""
)

# --- Engine + handler unit tests (pure logic, off-client) -------------------
# The engine layer + mechanism handlers + ToggleManager depend on nothing but
# the standard library + the in-process Logger/FrameScript free functions (we
# stub the latter two below). No MinHook, no Windows.
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS src/Engine/*.cpp)
file(GLOB MECHANISM_SOURCES CONFIGURE_DEPENDS src/Engine/Mechanisms/*.cpp)

add_executable(autotarget_tests
    ${ENGINE_SOURCES}
    ${MECHANISM_SOURCES}
    src/Orchestration/ToggleManager.cpp
    tests/EngineTests.cpp
    tests/MechanismHandlerTests.cpp
    tests/ToggleManagerTests.cpp
    tests/TestStubs.cpp
)
target_include_directories(autotarget_tests PRIVATE src)

enable_testing()
add_test(NAME EngineTests COMMAND autotarget_tests)
