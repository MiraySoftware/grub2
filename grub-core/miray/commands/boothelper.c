/*
 *  Extension for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2019 Miray Software <oss@miray.de>
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

#include "miray_cmd_boothelper.h"

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/env.h>
#include <grub/time.h>
#include <grub/mm.h>
//#include <grub/i386/pc/kernel.h>
#include <grub/device.h>
#include <grub/extcmd.h>
#include <grub/acpi.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/video.h>

GRUB_MOD_LICENSE ("GPLv3+");


static const struct grub_arg_option options_bootdev[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

#define BOOTDEV_BUFFER_SIZE 60

static grub_err_t
miray_cmd_bootdev (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "bootdev";
   
   char buffer[BOOTDEV_BUFFER_SIZE + 1];

   if (state[0].set)
      env_key = state[0].arg;

   grub_err_t ret = miray_machine_bootdev(buffer, BOOTDEV_BUFFER_SIZE);
   if (ret != GRUB_ERR_NONE)
      return ret;

   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;   
}

static const struct grub_arg_option options_tolower[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static grub_err_t
miray_cmd_asciitolower (grub_extcmd_context_t ctxt, int argc, char **args)
{
   struct grub_arg_list *state = ctxt->state;
   char * out;
   char * p;

   if (argc < 1)
      return -1;

   out = grub_strdup(args[0]);
   if (out == 0)
      return -1;

   for (p = out; *p != '\0'; ++p)
   {
      // This only works for ascii characters. But thats enough for us
      if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
   }
   
   if (state[0].set)
   {
      grub_env_set(state[0].arg, out);
   }
   else
   {
      grub_printf("%s", out);
   }

   grub_free(out);

   return GRUB_ERR_NONE;   
}


static const struct grub_arg_option options_acpi2[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

#define ACPI2_BUFFER_SIZE 40

static grub_err_t
miray_cmd_acpi2 (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "acpi2_start";
   
   char buffer[ACPI2_BUFFER_SIZE + 1];

   struct grub_acpi_rsdp_v20 *a = 0; 

   if (state[0].set)
      env_key = state[0].arg;

   a = grub_machine_acpi_get_rsdpv2 ();

   if (a == 0)
      return grub_error(GRUB_ERR_TEST_FAILURE, "ACPI 2 not supported");

   grub_snprintf(buffer, ACPI2_BUFFER_SIZE, "%p", (void *)a);
   
   grub_env_set(env_key, buffer);
   
   return GRUB_ERR_NONE;   
}


static const struct grub_arg_option options_smbios[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

#define SMBIOS_BUFFER_SIZE 40

static grub_err_t
miray_cmd_smbios (grub_extcmd_context_t ctxt,
                   int argc __attribute__ ((unused)),
                   char **args __attribute__ ((unused)))
{
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "smbios_start";
   
   char buffer[SMBIOS_BUFFER_SIZE + 1];

   void * ptr = 0;

   if (state[0].set)
      env_key = state[0].arg;

   ptr = miray_machine_smbios_ptr();

   if (ptr == 0)
      return grub_error(GRUB_ERR_TEST_FAILURE, "SMBIOS not supported");

   grub_snprintf(buffer, SMBIOS_BUFFER_SIZE, "%p", (void *)ptr);
   
   grub_env_set(env_key, buffer);

   return GRUB_ERR_NONE;   
}


static const struct grub_arg_option options_load_cliconf[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
};

static int str_iendswith(const char * str, const char * pat)
{
   grub_size_t str_len = grub_strlen(str);
   grub_size_t pat_len = grub_strlen(pat);
   grub_size_t i = 0;

   if (str_len < pat_len) return -1;
   for (i = 0; i < pat_len; i++)
   {
      if (grub_toupper(str[str_len - pat_len + i]) != grub_toupper(pat[i])) return -1;
   }

   return 0;
}

static grub_size_t read_config_string(const char * name, const char * path, char * buffer, grub_size_t bufferSize)
{
   grub_size_t offset = 0;
   grub_file_t file = 0;
   grub_size_t read = 0;
   grub_size_t i = 0;

   char key_buffer[80];

   grub_snprintf(key_buffer, 80, "cli.%s", name);
   {
      grub_size_t k = 0;
      grub_size_t key_len = grub_strlen(key_buffer) - 4;
      for (k = 0; k < key_len; k++)
      {
         key_buffer[k] = grub_tolower(key_buffer[k]);
      }
      key_buffer[key_len] = '\0';
   }

   {
      grub_size_t key_len = grub_strlen(key_buffer);
      if (bufferSize - 3 < key_len)
      {
         return 0;
      }

      grub_memcpy(buffer, key_buffer, key_len);
      offset += key_len;
      buffer[offset++] = '=';
      buffer[offset++] = '\'';
   }

   file = grub_file_open(path, GRUB_FILE_TYPE_LOADENV);
   if (file == 0)
   {
      return 0;
   }  

   if (grub_file_size(file) > bufferSize - offset - 2)
   {
      grub_file_close(file);
      return 0;
   }

   read = grub_file_read(file, &buffer[offset], grub_file_size(file));
   if (read < grub_file_size(file))
   {
      grub_file_close(file);
      return 0;
   }
   else if (read > grub_file_size(file))
   {
      read = grub_file_size(file);
   }

   for (i = 0; i < read; i++)
   {
      int cont = 1;
      switch(buffer[offset])
      {
         case '\'':
            buffer[offset] = '"';
            break;
         case '\n':
         case '\t':
            buffer[offset] = ' ';
            break;

         case '\0':
            cont = 0;
            break;
      }

      // Abort on 
      if (!cont)
         break;
         
      offset++;
   }

   grub_file_close(file);
   file = 0;

   buffer[offset++] = '\'';
   buffer[offset] = '\0';

   return offset;
}


struct cliconf_iter_hook_data {
   grub_size_t offset;
   char        *device_name;
   char        *dir_name;
   grub_size_t buffer_size;
   char *      buffer;
};
static int cliconf_iter_hook (const char *filename, const struct grub_dirhook_info *info __attribute__ ((unused)), void * data)
{
   struct cliconf_iter_hook_data * it_data = (struct cliconf_iter_hook_data *) data;
   
   if (str_iendswith(filename, ".CLI") == 0)
   {
      char path_buffer[200];
      grub_size_t coffset = it_data->offset > 0 ? it_data->offset + 1 : 0;

      grub_snprintf(path_buffer, 200, "(%s)%s/%s", it_data->device_name, it_data->dir_name, filename);
      
      grub_size_t read_ret = read_config_string(filename, path_buffer, &(it_data->buffer[coffset]), it_data->buffer_size - coffset);
      if (read_ret > 0)
      {
         if (it_data->offset > 0)
         {
             it_data->buffer[it_data->offset++] = ' ';
         }
         it_data->offset += read_ret;
      }
   }

   while (grub_error_pop()) {} 

   return GRUB_ERR_NONE;
}

static grub_err_t
miray_cmd_load_cliconf (grub_extcmd_context_t ctxt,
                     int argc __attribute__ ((unused)),
                     char **args __attribute__ ((unused)) )
{
   static const grub_size_t buffer_size = 8 * 1024;

   grub_err_t ret = GRUB_ERR_NONE;
   struct grub_arg_list *state = ctxt->state;
   const char * env_key = "symobi_cli";
   const char * path = 0;
   grub_device_t device = 0;
   grub_fs_t fs = 0;
   struct cliconf_iter_hook_data it_data = {
      .offset        = 0,
      .device_name   = 0,
      .dir_name      = 0,
      .buffer_size   = buffer_size,
      .buffer        = 0,
   };

   if (argc < 1)
   {
      ret = grub_error(GRUB_ERR_TEST_FAILURE, "Invalid directory");
      goto func_out;
   }

   path = args[0];
   if (path == 0)
   {
      ret = grub_error(GRUB_ERR_TEST_FAILURE, "Invalid directory");
      goto func_out;
   }
   
   if (state[0].set)
      env_key = state[0].arg;

   it_data.buffer = grub_malloc(buffer_size);   
   if (it_data.buffer == 0)
   {
      ret = grub_error(GRUB_ERR_TEST_FAILURE, "buffer allocation error");
      goto func_out;
   }
   it_data.buffer[0] = '\0';

   it_data.device_name = grub_file_get_device_name(path);
   device = grub_device_open (it_data.device_name);

   if (device == 0)
   {
      if (device == 0)
      {
         ret = grub_error(GRUB_ERR_TEST_FAILURE, "unknown device");
         goto func_out;
      }
   }

   fs = grub_fs_probe(device);
   if (fs == 0)
   {
      ret = grub_error(GRUB_ERR_TEST_FAILURE, "unknown fs");
      goto func_out;
   }

   it_data.dir_name = (path[0] == '(') ? grub_strchr (path, ')') : NULL;
   if (it_data.dir_name)
      it_data.dir_name++;
   else
      it_data.dir_name = (char *) path;

   if (fs->fs_dir == 0)
   {
      ret = grub_error(GRUB_ERR_TEST_FAILURE, "No fs iterator");
      goto func_out;
   }

   fs->fs_dir(device, it_data.dir_name, cliconf_iter_hook, &it_data);

   grub_env_set(env_key, it_data.buffer);

func_out:
   if (it_data.buffer != 0) grub_free(it_data.buffer);
   //if (fs != 0) grub_fs_close(fs);
   if (it_data.device_name != 0) grub_free(it_data.device_name);
   if (device != 0) grub_device_close(device);

   return ret;  
   
}

static grub_err_t
miray_cmd_check_fbaddr(grub_extcmd_context_t ctxt __attribute__ ((unused)),
                     int argc __attribute__ ((unused)),
                     char **args __attribute__ ((unused)) )
{
   #if GRUB_CPU_SIZEOF_VOID_P == 8
   if ((grub_uint64_t) grub_get_fb() > 0xFFFFFFFFu)
   {
      return grub_error(GRUB_ERR_TEST_FAILURE, N_("false"));
   }
   #endif

   return GRUB_ERR_NONE;
}


static grub_extcmd_t cmd_bootdev, cmd_tolower, cmd_acpi2, cmd_smbios, cmd_load_cliconf, cmd_check_fbaddr;

// Legacy command names
static grub_extcmd_t l_cmd_dev;

GRUB_MOD_INIT(boothelper)
{
   cmd_bootdev = grub_register_extcmd ("miray_bootdev", miray_cmd_bootdev,
				     0, 0, N_("Set bootdev variable"), options_bootdev);

   cmd_tolower = grub_register_extcmd ("miray_tolower", miray_cmd_asciitolower,
				     0, 0, N_("Change ascii characters to lower"), options_tolower);

   cmd_acpi2 = grub_register_extcmd ("miray_acpi2", miray_cmd_acpi2,
				   0, 0, N_("Set acpi2 addres to variable"), options_acpi2);

   cmd_smbios = grub_register_extcmd ("miray_smbios", miray_cmd_smbios,
				    0, 0, N_("Set smbios addres to variable"), options_smbios);


   cmd_load_cliconf = grub_register_extcmd ("miray_load_cliconf", miray_cmd_load_cliconf,
				    0, 0, N_("Set cli configuration to variable"), options_load_cliconf);

   cmd_check_fbaddr = grub_register_extcmd ("miray_check_fbaddr", miray_cmd_check_fbaddr,
				    0, 0, N_("Check if framebuffer is addressable with 32 bit"), 0);


   // Register legacy command names
   l_cmd_dev = grub_register_extcmd ("bootdev", miray_cmd_bootdev,
				   0, 0, N_("Set bootdev variable"), options_bootdev );
}

GRUB_MOD_FINI(boothelper)
{
   grub_unregister_extcmd(cmd_bootdev);
   grub_unregister_extcmd(cmd_tolower);
   grub_unregister_extcmd(cmd_acpi2);
   grub_unregister_extcmd(cmd_smbios);
   grub_unregister_extcmd(cmd_load_cliconf);
   grub_unregister_extcmd(cmd_check_fbaddr);

   // Unregister legacy command names
   grub_unregister_extcmd (l_cmd_dev);
}

// GRUB_MOD_DEP(drivemap);
// GRUB_MOD_DEP(boot);
