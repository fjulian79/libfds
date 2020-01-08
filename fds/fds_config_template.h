/*
 * libfds, used to store data in the on chip flash of a MCU. It shall NOT be a 
 * full blown file system but more than just a simple EEPROM emulation.
 *
 * Copyright (C) 2020 Julian Friedrich
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>. 
 *
 * You can file issues at https://github.com/fjulian79/libfds
 */

#include <bsp/bsp_flash.h>

#ifndef FDS_CONFIG_H_
#define FDS_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the number of supported parameters.
 */
#define FDS_NUM_RECORDS                 4

/**
 * @brief Defines the number of used flash pages. Currently the section used by
 * lib Fds is set to the very end of the usable flash. So if this parameter is 
 * set to two the last two pages will be used.
 */
#define FDS_NUM_PAGES                   4

/**
 * @brief Defines the maximum number of user data bytes per record in the falsh.
 * Hence that this relates to the flash page size, the number of used pages and 
 * the number of supported records as every record mut start and end on the same 
 * flash page. 
 * 
 * Example: FDS_NUM_PAGES and FDS_NUM_RECORDS set to 2, FDS_MAX_DATABYTES set to 
 * 512. The page size shall be 1024. 2 Records á 512 payload bytes will NOT fit 
 * with this configuration as each of them has 6 Bytes overhead and every page 
 * has a 4 Byte page header. Beside of that one page is requiered to be free at
 * any given time.
 */
#define FDS_MAX_DATABYTES               256

/**
 * @brief If FDS_DEBUG is defined debug log messages will be enabled.
 */
//#define FDS_DEBUG

#ifdef __cplusplus
}
#endif

#endif /* FDS_CONFIG_H_ */
