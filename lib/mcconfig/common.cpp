/*
   Configure module for the Midnight Commander

   Copyright (C) 1994-2020
   Free Software Foundation, Inc.

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>              /* extern int errno */

#include "lib/global.h"
#include "lib/vfs/vfs.h"        /* mc_stat */
#include "lib/util.h"

#include "lib/mcconfig.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static bool
mc_config_new_or_override_file (mc_config_t * mc_config, const gchar * ini_path, GError ** mcerror)
{
    gchar *data, *written_data;
    gsize len, total_written;
    bool ret;
    int fd;
    ssize_t cur_written;
    vfs_path_t *ini_vpath;

    mc_return_val_if_error (mcerror, false);

    data = g_key_file_to_data (mc_config->handle, &len, nullptr);
    if (!exist_file (ini_path))
    {
        ret = g_file_set_contents (ini_path, data, len, mcerror);
        g_free (data);
        return ret;
    }

    mc_util_make_backup_if_possible (ini_path, "~");

    ini_vpath = vfs_path_from_str (ini_path);
    fd = mc_open (ini_vpath, O_WRONLY | O_TRUNC, 0);
    vfs_path_free (ini_vpath);

    if (fd == -1)
    {
        mc_propagate_error (mcerror, 0, "%s", unix_error_string (errno));
        g_free (data);
        return false;
    }

    for (written_data = data, total_written = len;
         (cur_written = mc_write (fd, (const void *) written_data, total_written)) > 0;
         written_data += cur_written, total_written -= cur_written)
        ;

    mc_close (fd);
    g_free (data);

    if (cur_written == -1)
    {
        mc_util_restore_from_backup_if_possible (ini_path, "~");
        mc_propagate_error (mcerror, 0, "%s", unix_error_string (errno));
        return false;
    }

    mc_util_unlink_backup_if_possible (ini_path, "~");
    return true;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

mc_config_t *
mc_config_init (const gchar * ini_path, bool read_only)
{
    mc_config_t *mc_config;
    struct stat st;

    mc_config = static_cast<mc_config_t *> (g_try_malloc0 (sizeof (mc_config_t)));
    if (mc_config == nullptr)
        return nullptr;

    mc_config->handle = g_key_file_new ();
    if (mc_config->handle == nullptr)
    {
        g_free (mc_config);
        return nullptr;
    }

    if (ini_path == nullptr)
        return mc_config;

    if (exist_file (ini_path))
    {
        vfs_path_t *vpath;

        vpath = vfs_path_from_str (ini_path);
        if (mc_stat (vpath, &st) == 0 && st.st_size != 0)
        {
            GKeyFileFlags flags = G_KEY_FILE_NONE;

            if (!read_only)
                flags = static_cast<GKeyFileFlags> (((unsigned int) flags) | G_KEY_FILE_KEEP_COMMENTS);

            /* file exists and not empty */
            g_key_file_load_from_file (mc_config->handle, ini_path, flags, nullptr);
        }
        vfs_path_free (vpath);
    }

    mc_config->ini_path = g_strdup (ini_path);
    return mc_config;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_config_deinit (mc_config_t * mc_config)
{
    if (mc_config != nullptr)
    {
        g_free (mc_config->ini_path);
        g_key_file_free (mc_config->handle);
        g_free (mc_config);
    }
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_has_param (const mc_config_t * mc_config, const char *group, const gchar * param)
{
    if (mc_config == nullptr || group == nullptr || param == nullptr)
        return false;

    return g_key_file_has_key (mc_config->handle, group, param, nullptr);
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_has_group (mc_config_t * mc_config, const char *group)
{
    if (mc_config == nullptr || group == nullptr)
        return false;

    return g_key_file_has_group (mc_config->handle, group);
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_del_key (mc_config_t * mc_config, const char *group, const gchar * param)
{
    if (mc_config == nullptr || group == nullptr || param == nullptr)
        return false;

    return g_key_file_remove_key (mc_config->handle, group, param, nullptr);
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_del_group (mc_config_t * mc_config, const char *group)
{
    if (mc_config == nullptr || group == nullptr)
        return false;

    return g_key_file_remove_group (mc_config->handle, group, nullptr);
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_read_file (mc_config_t * mc_config, const gchar * ini_path, bool read_only,
                     bool remove_empty)
{
    mc_config_t *tmp_config;
    gchar **groups, **curr_grp;
    gchar *value;
    bool ok;

    if (mc_config == nullptr)
        return false;

    tmp_config = mc_config_init (ini_path, read_only);
    if (tmp_config == nullptr)
        return false;

    groups = mc_config_get_groups (tmp_config, nullptr);
    ok = (*groups != nullptr);

    for (curr_grp = groups; *curr_grp != nullptr; curr_grp++)
    {
        gchar **keys, **curr_key;

        keys = mc_config_get_keys (tmp_config, *curr_grp, nullptr);

        for (curr_key = keys; *curr_key != nullptr; curr_key++)
        {
            value = g_key_file_get_value (tmp_config->handle, *curr_grp, *curr_key, nullptr);
            if (value != nullptr)
            {
                if (*value == '\0' && remove_empty)
                    g_key_file_remove_key (mc_config->handle, *curr_grp, *curr_key, nullptr);
                else
                    g_key_file_set_value (mc_config->handle, *curr_grp, *curr_key, value);
                g_free (value);
            }
            else if (remove_empty)
                g_key_file_remove_key (mc_config->handle, *curr_grp, *curr_key, nullptr);
        }
        g_strfreev (keys);
    }

    g_strfreev (groups);
    mc_config_deinit (tmp_config);

    return ok;
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_save_file (mc_config_t * mc_config, GError ** mcerror)
{
    mc_return_val_if_error (mcerror, false);

    if (mc_config == nullptr || mc_config->ini_path == nullptr)
        return false;

    return mc_config_new_or_override_file (mc_config, mc_config->ini_path, mcerror);
}

/* --------------------------------------------------------------------------------------------- */

bool
mc_config_save_to_file (mc_config_t * mc_config, const gchar * ini_path, GError ** mcerror)
{
    mc_return_val_if_error (mcerror, false);

    if (mc_config == nullptr)
        return false;

    return mc_config_new_or_override_file (mc_config, ini_path, mcerror);
}

/* --------------------------------------------------------------------------------------------- */
