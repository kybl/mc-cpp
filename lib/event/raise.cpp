/*
   Handle any events in application.
   Raise events.

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
#include "lib/event.h"

#include "internal.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

bool
mc_event_raise (const gchar * event_group_name, const gchar * event_name, gpointer event_data)
{
    GTree *event_group;
    GPtrArray *callbacks;
    guint array_index;

    if (mc_event_grouplist == nullptr || event_group_name == nullptr || event_name == nullptr)
        return false;

    event_group = mc_event_get_event_group_by_name (event_group_name, false, nullptr);
    if (event_group == nullptr)
        return false;

    callbacks = mc_event_get_event_by_name (event_group, event_name, false, nullptr);
    if (callbacks == nullptr)
        return false;

    for (array_index = callbacks->len; array_index > 0; array_index--)
    {
        mc_event_callback_t *cb = static_cast<mc_event_callback_t *> (g_ptr_array_index (callbacks, array_index - 1));
        if (!(*cb->callback) (event_group_name, event_name, cb->init_data, event_data))
            break;
    }
    return true;
}

/* --------------------------------------------------------------------------------------------- */