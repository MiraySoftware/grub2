/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#ifndef GRUB_RELOCATOR_HEADER
#define GRUB_RELOCATOR_HEADER	1

#include <grub/types.h>
#include <grub/err.h>
#include <grub/memory.h>
#include <grub/cpu/memory.h>

struct grub_relocator;
struct grub_relocator_chunk;
typedef const struct grub_relocator_chunk *grub_relocator_chunk_t;

struct grub_relocator *grub_relocator_new (void);

grub_err_t
grub_relocator_alloc_chunk_addr (struct grub_relocator *rel,
				 grub_relocator_chunk_t *out,
				 grub_phys_addr_t target, grub_size_t size);

grub_err_t
grub_relocator_alloc_chunk_addr_l (struct grub_relocator *rel,
				 grub_relocator_chunk_t *out,
				 grub_phys_addr_t target, grub_size_t size,
				 int avoid_efi_loader_code);

void *
get_virtual_current_address (grub_relocator_chunk_t in);
grub_phys_addr_t
get_physical_target_address (grub_relocator_chunk_t in);

grub_err_t
grub_relocator_alloc_chunk_align (struct grub_relocator *rel, 
				  grub_relocator_chunk_t *out,
				  grub_phys_addr_t min_addr,
				  grub_phys_addr_t max_addr,
				  grub_size_t size, grub_size_t align,
				  int preference,
				  int avoid_efi_boot_services);

/*
 * Wrapper for grub_relocator_alloc_chunk_align() with purpose of
 * protecting against integer underflow.
 *
 * Compare to its callee, max_addr has different meaning here.
 * It covers entire chunk and not just start address of the chunk.
 */
static inline grub_err_t
grub_relocator_alloc_chunk_align_safe (struct grub_relocator *rel,
				       grub_relocator_chunk_t *out,
				       grub_phys_addr_t min_addr,
				       grub_phys_addr_t max_addr,
				       grub_size_t size, grub_size_t align,
				       int preference,
				       int avoid_efi_boot_services)
{
  /* Sanity check and ensure following equation (max_addr - size) is safe. */
  if (max_addr < size || (max_addr - size) < min_addr)
    return GRUB_ERR_OUT_OF_RANGE;

  return grub_relocator_alloc_chunk_align (rel, out, min_addr,
					   max_addr - size,
					   size, align, preference,
					   avoid_efi_boot_services);
}

/* Top 32-bit address minus s bytes and plus 1 byte. */
#define UP_TO_TOP32(s)	((~(s) & 0xffffffff) + 1)

#define GRUB_RELOCATOR_PREFERENCE_NONE 0
#define GRUB_RELOCATOR_PREFERENCE_LOW 1
#define GRUB_RELOCATOR_PREFERENCE_HIGH 2

void
grub_relocator_unload (struct grub_relocator *rel);

#endif
