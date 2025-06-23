cmake_minimum_required(VERSION 3.26)

project(smtplib)

set(CMAKE_CXX_STANDARD 20)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/permissive->")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>")

option(
    SMTPLIB_EXAMPLES
    "build examples for smtplib"
    OFF
)
option(
    SMTPLIB_EXAMPLES_USE_BOOST_ASIO
    "build examples with Boost.Asio instead of standalone Asio"
    OFF
)
option(
    SMTPLIB_EXAMPLES_USE_FMT_FORMAT
    "build examples with `fmt::format` instead of `std::format`"
    OFF
)

add_library(
    ${PROJECT_NAME}
    INTERFACE
    "${PROJECT_SOURCE_DIR}/include/smtplib/smtplib.h"
)

target_include_directories(
    ${PROJECT_NAME}
    INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${PROJECT_NAME}/include>
)

install(
    DIRECTORY ${PROJECT_SOURCE_DIR}/include
    DESTINATION ${PROJECT_NAME}
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}-targets
)

install(
    EXPORT ${PROJECT_NAME}-targets
    FILE ${PROJECT_NAME}-targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${PROJECT_NAME}/lib/cmake/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}-config.cmake.in
    ${PROJECT_NAME}-config.cmake
    INSTALL_DESTINATION ${PROJECT_NAME}/lib/cmake/${PROJECT_NAME}
)

install(
    FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
    DESTINATION ${PROJECT_NAME}/lib/cmake/${PROJECT_NAME}
)


if (SMTPLIB_EXAMPLES)

    if (SMTPLIB_EXAMPLES_USE_BOOST_ASIO)
        find_package(Boost "1.87" REQUIRED COMPONENTS asio)
        target_link_libraries(
            ${PROJECT_NAME}
            INTERFACE
            Boost::asio
        )
        target_compile_definitions(
            ${PROJECT_NAME}
            INTERFACE
            -DSMTPLIB_USE_BOOST_ASIO
        )
    else()
        find_package(asio "1.28" REQUIRED)
        target_link_libraries(
            ${PROJECT_NAME}
            INTERFACE
            asio::asio
        )
    endif()

    if (SMTPLIB_EXAMPLES_USE_FMT_FORMAT)
        find_package(fmt "10.1" REQUIRED)
        target_link_libraries(
            ${PROJECT_NAME}
            INTERFACE
            fmt::fmt-header-only
        )
        target_compile_definitions(
            ${PROJECT_NAME}
            INTERFACE
            -DSMTPLIB_USE_FMT_FORMAT
        )
    endif()

    find_package(OpenSSL "0.9.8" REQUIRED)
    target_link_libraries(
        ${PROJECT_NAME}
        INTERFACE
        OpenSSL::SSL
    )

    add_subdirectory(examples)
endif()