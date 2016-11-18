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

#ifndef __CS_PERF_DEFAULTS_H
#define __CS_PERF_DEFAULTS_H

enum csperf_role {
    CS_CLIENT,
    CS_SERVER
};

/* General constants */
#define DEFAULT_DATA_BLOCKLEN (1024) 
#define DEFAULT_SERVER_PORT   5001
#define DEFAULT_DATA_BLOCKS   1
#define DEFAULT_CLIENT_OUTPUT_FILE "csperf_client_out.txt" 
#define DEFAULT_SERVER_OUTPUT_FILE "csperf_server_out.txt" 

#endif /* __CS_PERF_DEFAULTS_H */
