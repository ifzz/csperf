#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIM(x) (sizeof(x)/sizeof(*(x)))

static const char     *sizes[]   = { "TB", "GB", "MB", "KB", "B" };
static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL;

void
csperf_common_calculate_size(char *result, uint64_t size)
{   
    uint64_t  multiplier = exbibytes;
    int i;

    if (!size) {
        strcpy(result, "0 B");
        return;
    }

    for (i = 0; i < DIM(sizes); i++, multiplier /= 1024)
    {   
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            sprintf(result, "%" PRIu64 " %s", size / multiplier, sizes[i]);
        else
            sprintf(result, "%.3f %s", (float) size / multiplier, sizes[i]);
        return;
    }
    strcpy(result, "0 B");
    return;
}
