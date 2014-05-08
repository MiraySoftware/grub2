/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2014 Free Software Foundation, Inc.
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

#include <grub/err.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/cpu/linux.h>
#include <grub/efi/efi.h>
#include <grub/efi/pe32.h>
#include <grub/efi/linux.h>
#include <grub/efi/sb.h>

int
grub_efi_secure_boot (void)
{
#ifdef GRUB_MACHINE_EFI
  grub_efi_guid_t efi_var_guid = GRUB_EFI_GLOBAL_VARIABLE_GUID;
  grub_size_t datasize;
  char *secure_boot = NULL;
  char *setup_mode = NULL;
  grub_efi_boolean_t ret = 0;

  secure_boot = grub_efi_get_variable("SecureBoot", &efi_var_guid, &datasize);
  if (datasize != 1 || !secure_boot)
    {
      grub_dprintf ("secureboot", "No SecureBoot variable\n");
      goto out;
    }
  grub_dprintf ("secureboot", "SecureBoot: %d\n", *secure_boot);

  setup_mode = grub_efi_get_variable("SetupMode", &efi_var_guid, &datasize);
  if (datasize != 1 || !setup_mode)
    {
      grub_dprintf ("secureboot", "No SetupMode variable\n");
      goto out;
    }
  grub_dprintf ("secureboot", "SetupMode: %d\n", *setup_mode);

  if (*secure_boot && !*setup_mode)
    ret = 1;

 out:
  grub_free (secure_boot);
  grub_free (setup_mode);
  return ret;
#else
  return 0;
#endif
}

struct grub_efi_shim_lock
{
  grub_efi_status_t (*verify) (void *buffer, grub_uint32_t size);
};
typedef struct grub_efi_shim_lock grub_efi_shim_lock_t;

#ifdef GRUB_MACHINE_EFI
int
grub_efi_secure_validate(void * buffer, grub_uint32_t size)
{
  grub_efi_guid_t guid = GRUB_EFI_SHIM_LOCK_GUID;
  grub_efi_shim_lock_t *shim_lock;
  grub_efi_status_t status;

  shim_lock = grub_efi_locate_protocol(&guid, NULL);

  if (!shim_lock)
    return 0;

  status = shim_lock->verify(buffer, size);
  if (status == GRUB_EFI_SUCCESS)
    return 1;

  grub_printf("Verification failed: %d\n", (int)status);
  
  return 0;
}
#else
int
grub_efi_secure_validate(void * buffer __attribute__((unused)), grub_uint32_t size __attribute__((unused)))
{
  return 0;
} 
#endif

