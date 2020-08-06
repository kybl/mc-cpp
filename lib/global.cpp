/*
   Global structure for some library-related variables

   Copyright (C) 2009-2020
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009.

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

/** \file glibcompat.c
 *  \brief Source: global structure for some library-related variables
 *
 */

#include <config.h>

#include "global.h"
#include "lib/timer.h"

/* *INDENT-OFF* */
#ifdef ENABLE_SUBSHELL
#  ifdef SUBSHELL_OPTIONAL
#    define SUBSHELL_USE false
#  else /* SUBSHELL_OPTIONAL */
#    define SUBSHELL_USE true
#  endif /* SUBSHELL_OPTIONAL */
#else /* !ENABLE_SUBSHELL */
#    define SUBSHELL_USE false
#endif /* !ENABLE_SUBSHELL */
/* *INDENT-ON* */

/*** global variables ****************************************************************************/

/* *INDENT-OFF* */
mc_global_t mc_global = {
    .mc_run_mode = MC_RUN_FULL,
    .run_from_parent_mc = false,
    .timer = nullptr,
    .midnight_shutdown = false,

    .sysconfig_dir = nullptr,
    .share_data_dir = nullptr,

#ifdef HAVE_CHARSET
    .source_codepage = -1,
    .display_codepage = -1,
#else
    .eight_bit_clean = true,
    .full_eight_bits = false,
#endif /* !HAVE_CHARSET */
    .utf8_display = false,

    .message_visible = true,
    .keybar_visible = true,

#ifdef ENABLE_BACKGROUND
    .we_are_background = false,
#endif /* ENABLE_BACKGROUND */

    .widget =
    {
        .confirm_history_cleanup = true,
        .show_all_if_ambiguous = false,
        .is_right = false
    },

    .shell = nullptr,

    .tty =
    {
        .skin = nullptr,
        .shadows = true,
        .setup_color_string = nullptr,
        .term_color_string = nullptr,
        .color_terminal_string = nullptr,
        .command_line_colors = nullptr,
#ifndef LINUX_CONS_SAVER_C
        .console_flag = '\0',
#endif /* !LINUX_CONS_SAVER_C */

        .use_subshell = SUBSHELL_USE,

#ifdef ENABLE_SUBSHELL
        .subshell_pty = 0,
#endif /* !ENABLE_SUBSHELL */

        .xterm_flag = false,
        .disable_x11 = false,
        .slow_terminal = false,
        .disable_colors = false,
        .ugly_line_drawing = false,
        .old_mouse = false,
        .alternate_plus_minus = false
    },

    .vfs =
    {
        .cd_symlinks = true,
        .preallocate_space = false,
    }

};
/* *INDENT-ON* */

#undef SUBSHELL_USE

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */
