/*
*    Copyright (C) 2015 Nikhil AP 
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

#ifndef __LOG_H
#define __LOG_H

#include "zlog.h"

#define LOG_CONFIG_FILE "/etc/csperf/log.conf"
#define CSPERF_LOG_CATEGORY "csperf_cat"

int log_init();

zlog_category_t* log_get_cat();
#endif /* __LOG_H */
