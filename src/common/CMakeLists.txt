FILE(GLOB csperf_sources *.c)
FILE(GLOB csperf_headers *.h)
add_library(common ${csperf_sources} ${csperf_headers}) 
