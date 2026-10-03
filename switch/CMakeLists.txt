cmake_minimum_required(VERSION 3.13)

# borealis (UI in stile Switch) viene scaricato da scripts/setup.sh in library/borealis
set(BOREALIS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)
if (NOT EXISTS ${BOREALIS_LIBRARY}/CMakeLists.txt)
    message(FATAL_ERROR "borealis mancante: esegui prima scripts/setup.sh")
endif ()

include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)
if (PLATFORM_IOS)
    # the app reads its files (translations, CA bundle) from disk: ship them in the .app instead of libromfs
    set(USE_LIBROMFS OFF)
endif ()

project(AnikkuNX)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "6")
set(VERSION_ALTER "2")
set(PROJECT_AUTHOR "Marco")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)
set(PACKAGE_NAME "com.ad3333m.anikkunx")

add_subdirectory(${BOREALIS_LIBRARY} borealis)

find_package(Threads REQUIRED)
list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# ---- libmpv + libcurl
if (PLATFORM_SWITCH)
    find_package(PkgConfig REQUIRED)
    pkg_search_module(MPV REQUIRED mpv)
    pkg_search_module(CURL REQUIRED libcurl)
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS})
    list(APPEND APP_PLATFORM_LIB ${MPV_STATIC_LIBRARIES} ${CURL_STATIC_LIBRARIES})
    link_directories(${MPV_LIBRARY_DIRS} ${CURL_LIBRARY_DIRS})
    # copertine WebP (AnimeSaturn & co.)
    pkg_search_module(WEBP libwebp)
    if (WEBP_FOUND)
        list(APPEND APP_PLATFORM_INCLUDE ${WEBP_INCLUDE_DIRS})
        list(APPEND APP_PLATFORM_LIB ${WEBP_STATIC_LIBRARIES})
        list(APPEND APP_PLATFORM_OPTION -DHAVE_WEBP)
    endif ()
elseif (PLATFORM_IOS)
    # prebuilt static libraries (MPVKit + libcurl), see scripts/ios/fetch_deps.sh
    set(IOS_DEPS ${CMAKE_CURRENT_SOURCE_DIR}/library/ios-deps)
    file(GLOB IOS_DEP_LIBS ${IOS_DEPS}/lib/*.a)
    if (NOT IOS_DEP_LIBS)
        message(FATAL_ERROR "iOS dependencies missing: run scripts/ios/fetch_deps.sh")
    endif ()
    list(APPEND APP_PLATFORM_INCLUDE ${IOS_DEPS}/include)
    list(APPEND APP_PLATFORM_LIB ${IOS_DEP_LIBS}
        "-framework AVFoundation" "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation"
        "-framework CoreGraphics" "-framework CoreMedia" "-framework CoreText" "-framework CoreVideo"
        "-framework Foundation" "-framework IOSurface" "-framework Metal" "-framework OpenGLES"
        "-framework QuartzCore" "-framework Security" "-framework SystemConfiguration" "-framework UIKit"
        "-framework VideoToolbox" bz2 iconv expat resolv xml2 z c++)
else ()
    # build desktop (Linux/macOS/Windows) utile per provare l'interfaccia sul PC
    find_package(PkgConfig REQUIRED)
    pkg_search_module(MPV REQUIRED mpv)
    find_package(CURL REQUIRED)
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_INCLUDE_DIRS})
    list(APPEND APP_PLATFORM_LIB ${MPV_LINK_LIBRARIES} CURL::libcurl)
endif ()

file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# gumbo (parser HTML5 di Google) compilato come C: sostituisce Jsoup delle estensioni
file(GLOB GUMBO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/library/gumbo/*.c)
add_library(gumbo STATIC ${GUMBO_SRC})
set_target_properties(gumbo PROPERTIES C_STANDARD 99 C_EXTENSIONS ON)
target_include_directories(gumbo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/library/gumbo)
target_compile_options(gumbo PRIVATE -w)
if (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
endif ()

program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections ${APP_PLATFORM_OPTION})
# versione mostrata nelle Impostazioni e repository da cui scaricare gli aggiornamenti
set(UPDATE_REPO "ad3333m/AnikkuNX" CACHE STRING "Repository GitHub delle release")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" UPDATE_REPO="${UPDATE_REPO}")
target_link_libraries(${PROJECT_NAME} PRIVATE borealis gumbo ${APP_PLATFORM_LIB})
target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})

if (PLATFORM_SWITCH)
    add_custom_target(${PROJECT_NAME}.nro ALL
        DEPENDS ${PROJECT_NAME}
        COMMAND ${NX_NACPTOOL_EXE} --create "Anikku NX" "${PROJECT_AUTHOR}"
            "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
        # sulla Switch si usa il font di sistema
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/resources/font/switch_font.ttf
        COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON}
            --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
    )
elseif (PLATFORM_IOS)
    ios_bundle(
        "${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard"
        "${CMAKE_CURRENT_SOURCE_DIR}/ios/Images.xcassets"
        "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        "AnikkuNX"
        "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/resources")
else ()
    add_custom_target(${PROJECT_NAME}.data ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_CURRENT_BINARY_DIR}/resources)
    add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}.data)
endif ()
