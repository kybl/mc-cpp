/*
   Handle any events in application.
   Manage events: add, delete, destroy, search

   Copyright (C) 2011-2020
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2011.

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

#include "lib/global.h"
#include "lib/util.h"
#include "lib/event.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
mc_event_group_destroy_value (gpointer data)
{
    GPtrArray *callbacks;

    callbacks = (GPtrArray *) data;
    g_ptr_array_foreach (callbacks, (GFunc) g_free, nullptr);
    g_ptr_array_free (callbacks, true);
}

/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

bool
mc_event_add (const gchar * event_group_name, const gchar * event_name,
              mc_event_callback_func_t event_callback, gpointer event_init_data, GError ** mcerror)
{

    GTree *event_group;
    GPtrArray *callbacks;
    mc_event_callback_t *cb;

    mc_return_val_if_error (mcerror, false);

    if (mc_event_grouplist == nullptr || event_group_name == nullptr || event_name == nullptr
        || event_callback == nullptr)
    {
        mc_propagate_error (mcerror, 0, "%s", _("Check input data! Some of parameters are nullptr!"));
        return false;
    }

    event_group = mc_event_get_event_group_by_name (event_group_name, true, mcerror);
    if (event_group == nullptr)
        return false;

    callbacks = mc_event_get_event_by_name (event_group, event_name, true, mcerror);
    if (callbacks == nullptr)
        return false;

    cb = mc_event_is_callback_in_array (callbacks, event_callback, event_init_data);
    if (cb == nullptr)
    {
        cb = g_new0 (mc_event_callback_t, 1);
        cb->callback = event_callback;
        g_ptr_array_add (callbacks, (gpointer) cb);
    }
    cb->init_data = event_init_data;
    return true;
}

/* --------------------------------------------------------------------------------------------- */

void
mc_event_del (const gchar * event_group_name, const gchar * event_name,
              mc_event_callback_func_t event_callback, gpointer event_init_data)
{
    GTree *event_group;
    GPtrArray *callbacks;
    mc_event_callback_t *cb;

    if (mc_event_grouplist == nullptr || event_group_name == nullptr || event_name == nullptr
        || event_callback == nullptr)
        return;

    event_group = mc_event_get_event_group_by_name (event_group_name, false, nullptr);
    if (event_group == nullptr)
        return;

    callbacks = mc_event_get_event_by_name (event_group, event_name, false, nullptr);
    if (callbacks == nullptr)
        return;

    cb = mc_event_is_callback_in_array (callbacks, event_callback, event_init_data);

    if (cb == nullptr)
        return;

    g_ptr_array_remove (callbacks, (gpointer) cb);
    g_free ((gpointer) cb);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_event_destroy (const gchar * event_group_name, const gchar * event_name)
{
    GTree *event_group;

    if (mc_event_grouplist == nullptr || event_group_name == nullptr || event_name == nullptr)
        return;

    event_group = mc_event_get_event_group_by_name (event_group_name, false, nullptr);
    g_tree_remove (event_group, (gconstpointer) event_name);
}

/* --------------------------------------------------------------------------------------------- */

void
mc_event_group_del (const gchar * event_group_name)
{

    if (mc_event_grouplist != nullptr && event_group_name != nullptr)
        g_tree_remove (mc_event_grouplist, (gconstpointer) event_group_name);
}

/* --------------------------------------------------------------------------------------------- */

GTree *
mc_event_get_event_group_by_name (const gchar * event_group_name, bool create_new,
                                  GError ** mcerror)
{
    GTree *event_group;

    mc_return_val_if_error (mcerror, nullptr);

    event_group = (GTree *) g_tree_lookup (mc_event_grouplist, (gconstpointer) event_group_name);
    if (event_group == nullptr && create_new)
    {
        event_group =
            g_tree_new_full ((GCompareDataFunc) g_ascii_strcasecmp,
                             nullptr,
                             (GDestroyNotify) g_free,
                             (GDestroyNotify) mc_event_group_destroy_value);
        if (event_group == nullptr)
        {
            mc_propagate_error (mcerror, 0, _("Unable to create group '%s' for events!"),
                                event_group_name);
            return nullptr;
        }
        g_tree_insert (mc_event_grouplist, g_strdup (event_group_name), (gpointer) event_group);
    }
    return event_group;
}

/* --------------------------------------------------------------------------------------------- */

GPtrArray *
mc_event_get_event_by_name (GTree * event_group, const gchar * event_name, bool create_new,
                            GError ** mcerror)
{
    GPtrArray *callbacks;

    mc_return_val_if_error (mcerror, nullptr);

    callbacks = (GPtrArray *) g_tree_lookup (event_group, (gconstpointer) event_name);
    if (callbacks == nullptr && create_new)
    {
        callbacks = g_ptr_array_new ();
        if (callbacks == nullptr)
        {
            mc_propagate_error (mcerror, 0, _("Unable to create event '%s'!"), event_name);
            return nullptr;
        }
        g_tree_insert (event_group, g_strdup (event_name), (gpointer) callbacks);
    }
    return callbacks;
}

/* --------------------------------------------------------------------------------------------- */

mc_event_callback_t *
mc_event_is_callback_in_array (GPtrArray * callbacks, mc_event_callback_func_t event_callback,
                               gpointer event_init_data)
{
    guint array_index;

    for (array_index = 0; array_index < callbacks->len; array_index++)
    {
        mc_event_callback_t *cb = static_cast<mc_event_callback_t *> (g_ptr_array_index (callbacks, array_index));
        if (cb->callback == event_callback && cb->init_data == event_init_data)
            return cb;
    }
    return nullptr;
}

/* --------------------------------------------------------------------------------------------- */
