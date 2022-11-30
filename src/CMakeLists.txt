set(include_dir ${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME})


# Packages
include(FetchContent)
FetchContent_Declare(asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG "147f7225a96d45a2807a64e443177f621844e51c"
)
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG "a33701196adfad74917046096bf5a2aa0ab0bb50"
)
FetchContent_Declare(rtc
    GIT_REPOSITORY https://github.com/rturrado/rtc.git
    GIT_TAG "d3c6dc594a3a7c2f3d7939bb40c17cc47f881453"
)
FetchContent_MakeAvailable(
    asio
    fmt
    rtc
)
find_package(Threads REQUIRED)


# Sources
set(lib_sources
)
set(app_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
)
list(APPEND app_sources ${lib_sources})


# asio library
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_link_libraries(asio INTERFACE Threads::Threads)


# Main executable
add_executable(${PROJECT_NAME} ${app_sources})
target_include_directories(${PROJECT_NAME} PUBLIC
    "$<BUILD_INTERFACE:${include_dir}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PUBLIC
    asio
    fmt
    rtc
)


# Target compile options
if(MSVC)
    set(msvc_compile_options
        /W3 /WX /w34996
        /D_CONSOLE /DCONSOLE
        /D_UNICODE /DUNICODE
        /diagnostics:column /EHsc /FC /fp:precise /Gd /GS /MP /sdl /utf-8 /Zc:inline
    )
    target_compile_options(${PROJECT_NAME} PRIVATE ${msvc_compile_options})
endif()
