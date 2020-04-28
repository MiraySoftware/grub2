/* miray_bootcast.h - check bootcast flag */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2014-2020 Miray Software <oss@miray.de>
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MIRAY_BOOTCAST_H
#define MIRAY_BOOTCAST_H 1

#include <grub/env.h>

static inline int miray_isbootcast(void)
{
  const char *var = grub_env_get("miray_bootcast_menu");
  if (var == 0 || var[0] == '\0' || var[0] == '0')
    return 0;
  else
    return 1;      
}

#endif