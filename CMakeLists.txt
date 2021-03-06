cmake_minimum_required(VERSION 3.1.0) # Threads::Threads is needed

project(fastfetch)

option(BUILD_TESTS "Build test programs" OFF)
option(BUILD_DEBUG "Build with debug symbols" OFF)

execute_process(
    COMMAND git rev-list --count HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REV_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_REV_PARSE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process (
    COMMAND bash -c "awk -F= '/^ID=/{print $2}' /etc/os-release |tr -d '\n' | tr -d '\"'"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE BUILD_DISTRIBUTION_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(PROJECT_VERSION "r${GIT_REV_LIST}.${GIT_REV_PARSE}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-O3")

if(BUILD_TESTS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native -mtune=native -pipe -fno-plt -Wconversion -Wpedantic")
endif(BUILD_TESTS)

if(BUILD_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
endif(BUILD_DEBUG)

configure_file(src/fastfetch_config.h.in fastfetch_config.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package (PkgConfig)

if(${PkgConfig_FOUND})
    pkg_check_modules (GIO2 gio-2.0)
    pkg_check_modules (DCONF dconf)
endif(${PkgConfig_FOUND})

if(NOT DEFINED GIO2_INCLUDE_DIRS)
    set(GIO2_INCLUDE_DIRS
        /usr/include/glib-2.0/
        /usr/lib/glib-2.0/include/
    )
endif(NOT DEFINED GIO2_INCLUDE_DIRS)

if(NOT DEFINED DCONF_INCLUDE_DIRS)
    set(DCONF_INCLUDE_DIRS
        /usr/include/dconf/
    )
endif(NOT DEFINED DCONF_INCLUDE_DIRS)

include_directories(
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${GIO2_INCLUDE_DIRS}
    ${DCONF_INCLUDE_DIRS}
)

link_libraries(
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

set(SRCS
    src/util/FFstrbuf.c
    src/util/FFlist.c
    src/common/init.c
    src/common/threading.c
    src/common/io.c
    src/common/processing.c
    src/common/logo.c
    src/common/format.c
    src/common/parsing.c
    src/common/settings.c
    src/common/detectPlasma.c
    src/common/detectGTK.c
    src/common/detectWMDE.c
    src/common/detectTerminalShell.c
    src/modules/break.c
    src/modules/custom.c
    src/modules/title.c
    src/modules/separator.c
    src/modules/os.c
    src/modules/host.c
    src/modules/kernel.c
    src/modules/uptime.c
    src/modules/packages.c
    src/modules/shell.c
    src/modules/resolution.c
    src/modules/de.c
    src/modules/wm.c
    src/modules/wmtheme.c
    src/modules/theme.c
    src/modules/icons.c
    src/modules/font.c
    src/modules/cursor.c
    src/modules/terminal.c
    src/modules/terminalfont.c
    src/modules/cpu.c
    src/modules/gpu.c
    src/modules/memory.c
    src/modules/disk.c
    src/modules/battery.c
    src/modules/locale.c
    src/modules/colors.c
)

add_executable(fastfetch
    src/fastfetch.c
    src/util/FFvaluestore.c
    ${SRCS}
)

add_executable(flashfetch
    src/flashfetch.c
    ${SRCS}
)

target_compile_definitions(flashfetch PUBLIC
    FASTFETCH_BUILD_FLASHFETCH
)

if(BUILD_TESTS)
    add_executable(fastfetch-test-performance
        tests/performance.c
        ${SRCS}
    )
endif(BUILD_TESTS)
