/* sb.h - declare functions for EFI Secure Boot support */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_EFI_SB_HEADER
#define GRUB_EFI_SB_HEADER	1

#include <grub/types.h>
#include <grub/dl.h>

/* Functions.  */
int EXPORT_FUNC (grub_efi_secure_boot) (void);

void EXPORT_FUNC (grub_efi_set_shim_lock_active) (int val);
int EXPORT_FUNC (grub_efi_is_shim_lock_active) (void);

#endif /* ! GRUB_EFI_SB_HEADER */
