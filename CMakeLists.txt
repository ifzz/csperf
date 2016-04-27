cmake_minimum_required(VERSION 2.8)
project(csperf)

include_directories(
        "${PROJECT_SOURCE_DIR}")

set(LIBEVENT_DIR  ${PROJECT_SOURCE_DIR}/packages/libevent-2.0.21-stable)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")

include(ExternalProject)
# Build libevent
ExternalProject_Add(libevent2
        SOURCE_DIR ${LIBEVENT_DIR} 
        PREFIX ${CMAKE_BINARY_DIR}/libevent2
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
)
ExternalProject_Get_Property(libevent2 install_dir)
add_library(libevent STATIC IMPORTED)
set_property(TARGET libevent PROPERTY IMPORTED_LOCATION
        ${install_dir}/lib/libevent.a)
add_dependencies(libevent libevent2)
include_directories(${install_dir}/include)

FILE(GLOB csperf_sources ${PROJECT_SOURCE_DIR}/src/*.c)
FILE(GLOB csperf_headers ${PROJECT_SOURCE_DIR}/src/*.h)

add_executable(csperf ${csperf_sources})
target_link_libraries(csperf libevent rt)

install(TARGETS csperf RUNTIME DESTINATION ${TOP_BUILD_DIR}/bin)
install(TARGETS csperf RUNTIME DESTINATION /usr/local/bin)
