/*
*    Copyright (C) 2016 Nikhil AP
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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

int
csperf_common_is_valid_ip(char *address)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, address, &(sa.sin_addr));
}

int
csperf_common_is_valid_hostname(char *hostname)
{
    return 1;
}
