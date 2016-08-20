cmake_minimum_required(VERSION 2.8)
project(csperf)

set(CONFIG_DIR "/etc/csperf")
set(MAN_PAGE_DIR "share/man/man1")

set(LOG_CONFIG ${PROJECT_SOURCE_DIR}/src/common/log.conf)
set(MAN_PAGE   ${PROJECT_SOURCE_DIR}/docs/csperf.1)

include_directories(
        "${PROJECT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/src/common")

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

# Build zlog
ExternalProject_Add(zlog
        URL ${PROJECT_SOURCE_DIR}/packages/zlog.tar.gz
        PREFIX ${CMAKE_BINARY_DIR}/external/zlog
        CONFIGURE_COMMAND ""
        PATCH_COMMAND patch -p1 < ${PROJECT_SOURCE_DIR}/patches/zlog_error.patch
        UPDATE_COMMAND ""
        BUILD_COMMAND make -C <SOURCE_DIR>
        INSTALL_COMMAND make -C <SOURCE_DIR> install PREFIX=<INSTALL_DIR>
)
ExternalProject_Get_Property(zlog install_dir)
add_library(libzlog STATIC IMPORTED)
set_property(TARGET libzlog PROPERTY IMPORTED_LOCATION
        ${install_dir}/lib/libzlog.a)
add_dependencies(libzlog zlog)
include_directories(${install_dir}/include)

# Add sub-directories
    add_subdirectory(src/common)

FILE(GLOB csperf_sources ${PROJECT_SOURCE_DIR}/src/*.c)
FILE(GLOB csperf_headers ${PROJECT_SOURCE_DIR}/src/*.h)

add_executable(csperf ${csperf_sources})
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_link_libraries(csperf common libevent libzlog rt)
else ()
    target_link_libraries(csperf libevent libzlog)
endif ()

install(FILES ${MAN_PAGE} DESTINATION ${MAN_PAGE_DIR}
    PERMISSIONS OWNER_WRITE OWNER_READ
    GROUP_READ WORLD_READ)

install(FILES ${LOG_CONFIG} DESTINATION ${CONFIG_DIR}
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
    GROUP_EXECUTE GROUP_WRITE GROUP_READ
    WORLD_EXECUTE WORLD_WRITE WORLD_READ)

install(TARGETS csperf RUNTIME DESTINATION /usr/local/bin)
