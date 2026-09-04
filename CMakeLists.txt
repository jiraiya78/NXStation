cmake_minimum_required(VERSION 3.16)

# Borealis lives as a git submodule under library/borealis
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)

if (NOT EXISTS "${BOREALIS_LIBRARY}/cmake/commonOption.cmake")
    message(FATAL_ERROR
        "Borealis not found. Run:\n"
        "  git submodule add https://github.com/xfangfang/borealis.git library/borealis\n"
        "  git submodule update --init --recursive")
endif ()

include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

# Optional FFmpeg video decode (requires switch-ffmpeg / desktop ffmpeg)
option(SF_ENABLE_FFMPEG "Enable FFmpeg video preview decoding" ON)
option(SF_ENABLE_MUPDF "Enable MuPDF game-manual PDF rendering" ON)

include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(NXStation)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_ALTER "0")
set(VERSION_BUILD "0")

# Generated header so the in-app "About" version always matches the build.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/app/Version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/app/Version.hpp
    @ONLY
)
set(PROJECT_AUTHOR "NXStation")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(PROJECT_TITLEID "0104444005354000")

if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif ()

add_subdirectory(library)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_BINARY_DIR}/generated
)

file(GLOB_RECURSE MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp
)

list(APPEND MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen/qrcodegen.cpp)
list(APPEND APP_PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qrcodegen)

if (PLATFORM_SWITCH)
    file(GLOB_RECURSE YATI_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yati/source/nx/keys.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yati/source/nx/ncm.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yati/source/nx/ns.cpp
    )
    list(APPEND MAIN_SRC ${YATI_SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minIni/source/minIni.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minIni/source/minGlue-nx.c
    )
    list(APPEND APP_PLATFORM_INCLUDE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yati/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/yati/include/yati/nx/nxdumptool
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sphaira_shim
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/minIni/include
    )
else ()
    list(FILTER MAIN_SRC EXCLUDE REGEX "/source/forwarder/ForwarderNca\\.cpp$")
endif ()

set(PLATFORM_OPTION)
if (PLATFORM_SWITCH)
    set(APP_PLATFORM_LIB
        glfw3 EGL glapi drm_nouveau
        nx m
        curl
        mbedtls mbedx509 mbedcrypto
    )

    if (SF_ENABLE_FFMPEG)
        list(APPEND APP_PLATFORM_LIB
            avformat avcodec avutil swscale swresample
            dav1d bz2
        )
        list(APPEND PLATFORM_OPTION -DSF_HAS_FFMPEG=1)
    endif ()

    if (SF_ENABLE_MUPDF)
        set(MUPDF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mupdf)
        set(MUPDF_LIB ${MUPDF_ROOT}/build/release/libmupdf.a)
        set(MUPDF_THIRD_LIB ${MUPDF_ROOT}/build/release/libmupdf-third.a)
        if (NOT EXISTS "${MUPDF_LIB}")
            message(STATUS "Building MuPDF for Switch (first time only)…")
            execute_process(
                COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_mupdf_switch.sh
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE MUPDF_BUILD_RESULT
            )
            if (NOT MUPDF_BUILD_RESULT EQUAL 0 OR NOT EXISTS "${MUPDF_LIB}")
                message(WARNING "MuPDF build failed; manual PDF viewing disabled")
            endif ()
        endif ()
        if (EXISTS "${MUPDF_LIB}")
            list(APPEND APP_PLATFORM_LIB ${MUPDF_LIB})
            if (EXISTS "${MUPDF_THIRD_LIB}")
                list(APPEND APP_PLATFORM_LIB ${MUPDF_THIRD_LIB})
            endif ()
            list(APPEND APP_PLATFORM_INCLUDE ${MUPDF_ROOT}/include)
            list(APPEND PLATFORM_OPTION -DSF_HAVE_MUPDF=1)
        endif ()
    endif ()

    # zlib must be last for static FFmpeg/curl symbol resolution
    list(APPEND APP_PLATFORM_LIB minizip z)

    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
    list(APPEND APP_PLATFORM_INCLUDE ${DEVKITPRO}/portlibs/switch/include)
elseif (PLATFORM_DESKTOP)
    find_package(CURL REQUIRED)
    list(APPEND APP_PLATFORM_LIB CURL::libcurl)
    if (SF_ENABLE_FFMPEG)
        find_package(PkgConfig)
        if (PkgConfig_FOUND)
            pkg_check_modules(FFMPEG libavformat libavcodec libavutil libswscale)
            if (FFMPEG_FOUND)
                list(APPEND APP_PLATFORM_LIB ${FFMPEG_LIBRARIES})
                list(APPEND APP_PLATFORM_INCLUDE ${FFMPEG_INCLUDE_DIRS})
                list(APPEND PLATFORM_OPTION -DSF_HAS_FFMPEG=1)
            endif ()
        endif ()
    endif ()
endif ()

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

# Ship roms_config.json inside romfs resources
# (touch this file so GLOB picks up newly added sources)
add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_hbl_embed.py
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/roms_config.json
        ${PROJECT_RESOURCES}/roms_config.json
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_themes.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_icon.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_system_placeholders.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_metric_icons.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_brand_assets.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_nav_sfx.py
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_round_font.py
)

if (PLATFORM_SWITCH)
    if (USE_DEKO3D)
        gen_dksh("${PROJECT_RESOURCES}/shaders")
    endif ()
    add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}"
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}"
            ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro
            --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp
            --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
elseif (PLATFORM_DESKTOP)
    add_custom_target(${PROJECT_NAME}.data
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources
    )
    if (NOT USE_LIBROMFS)
        add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
    endif ()

    set(ANALYTICS_TEST_SRC
        tests/playtime_analytics_test.cpp
        source/analytics/PersonalityMetrics.cpp
        source/analytics/PlaytimeHabits.cpp
        source/util/Json.cpp
        source/util/Utf8.cpp
    )
    add_executable(playtime_analytics_test ${ANALYTICS_TEST_SRC})
    target_include_directories(playtime_analytics_test PRIVATE ${APP_PLATFORM_INCLUDE})
    target_compile_definitions(playtime_analytics_test PRIVATE PLATFORM_DESKTOP=1)
    set_target_properties(playtime_analytics_test PROPERTIES CXX_STANDARD 23)
    target_compile_options(playtime_analytics_test PRIVATE -ffunction-sections -fdata-sections)
    add_test(NAME playtime_analytics_test COMMAND playtime_analytics_test)
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE
    -ffunction-sections -fdata-sections
    ${PLATFORM_OPTION}
    ${APP_PLATFORM_OPTION}
)
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
