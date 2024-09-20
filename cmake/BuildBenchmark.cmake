set(CMAKE_CXX_STANDARD 20)           # choose 14, 17, 20
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS NO)

set(BENCHMARK_VERSION v1.9.0)
set(BENCHMARK_HASH 21a2604efeded8b4cbabc72f3e1c7a2a)
set(BENCHMARK_INSTALL_DIR ${BUNDLED_LIBS_INSTALL_DIR}/benchmark-prefix)
set(BENCHMARK_INCLUDE_DIR ${BENCHMARK_INSTALL_DIR}/dest/include)

set(BENCHMARK_LIB ${BENCHMARK_INSTALL_DIR}/build/src/libbenchmark.a)
set(BENCHMARK_LIB_MAIN ${BENCHMARK_INSTALL_DIR}/build/src/libbenchmark_main.a)

list(APPEND BENCHMARK_CMAKE_FLAGS "-DBENCHMARK_ENABLE_TESTING=OFF")
list(APPEND BENCHMARK_CMAKE_FLAGS "-DBENCHMARK_ENABLE_LTO=OFF")
list(APPEND BENCHMARK_CMAKE_FLAGS "-DBENCHMARK_USE_LIBCXX=OFF")
list(APPEND BENCHMARK_CMAKE_FLAGS "-DBENCHMARK_ENABLE_GTEST_TESTS=OFF")
list(APPEND BENCHMARK_CMAKE_FLAGS "-DBENCHMARK_ENABLE_LIBPFM=OFF")

include(ExternalProject)
ExternalProject_Add(bundled-benchmark-project
    PREFIX ${BENCHMARK_INSTALL_DIR}
    SOURCE_DIR ${BENCHMARK_INSTALL_DIR}/src
    BINARY_DIR ${BENCHMARK_INSTALL_DIR}/build
    STAMP_DIR ${BENCHMARK_INSTALL_DIR}/stamp
    URL_MD5 ${BENCHMARK_HASH}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/google/benchmark/archive/refs/tags/${BENCHMARK_VERSION}.tar.gz
    CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -B <BINARY_DIR> -S <SOURCE_DIR>
            -G ${CMAKE_GENERATOR} ${BENCHMARK_CMAKE_FLAGS}
    BUILD_COMMAND cd <BINARY_DIR> && ${CMAKE_MAKE_PROGRAM} -j
    INSTALL_COMMAND ""
    CMAKE_GENERATOR ${CMAKE_GENERATOR}
    BUILD_BYPRODUCTS ${BENCHMARK_LIB} ${BENCHMARK_LIB_MAIN}
)

add_library(bundled-benchmark STATIC IMPORTED GLOBAL)
set_target_properties(bundled-benchmark PROPERTIES IMPORTED_LOCATION
  ${BENCHMARK_LIB})

add_library(bundled-benchmark-main STATIC IMPORTED GLOBAL)
set_target_properties(bundled-benchmark-main PROPERTIES IMPORTED_LOCATION
  ${BENCHMARK_LIB_MAIN})

# https://stackoverflow.com/questions/48396863/how-to-include-external-shared-library-with-several-dlls-dylibs
# add_library(benchmark::benchmark INTERFACE IMPORTED)
# set_property(TARGET benchmark::benchmark PROPERTY
#   INTERFACE_LINK_LIBRARIES ${BENCHMARK_LIB})

# add_library(benchmark::benchmark_main INTERFACE IMPORTED)
# set_property(TARGET benchmark::benchmark_main PROPERTY
#   INTERFACE_LINK_LIBRARIES ${BENCHMARK_LIB_MAIN})

set(BENCHMARK_FOUND TRUE)
set(BENCHMARK_LIBRARIES bundled-benchmark-main bundled-benchmark)
# set(BENCHMARK_LIBRARIES benchmark::benchmark benchmark::benchmark_main)
set(BENCHMARK_INCLUDE_DIRS ${BENCHMARK_INCLUDE_DIR})

unset(BENCHMARK_VERSION)
unset(BENCHMARK_HASH)
unset(BENCHMARK_INSTALL_DIR)
unset(BENCHMARK_INCLUDE_DIR)
unset(BENCHMARK_CMAKE_FLAGS)

message(STATUS "Using bundled Google Benchmark")
