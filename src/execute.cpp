/*
   Execution routines for GNU Midnight Commander

   Copyright (C) 2003-2020
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2013

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

/** \file  execute.c
 *  \brief Source: execution routines
 */

#include <config.h>

#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/tty/win.h"
#include "lib/vfs/vfs.h"
#include "lib/mcconfig.h"
#include "lib/util.h"
#include "lib/strutil.h"        /* str_replace_all_substrings() */
#include "lib/widget.h"

#include "filemanager/midnight.h"
#include "filemanager/layout.h" /* use_dash() */
#include "consaver/cons.saver.h"
#ifdef ENABLE_SUBSHELL
#include "subshell/subshell.h"
#endif
#include "setup.h"              /* clear_before_exec */

#include "execute.h"

/*** global variables ****************************************************************************/

int pause_after_run = pause_on_dumb_terminals;

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/

void do_execute (const char *shell, const char *command, int flags);
void do_executev (const char *shell, int flags, char *const argv[]);
char *execute_get_external_cmd_opts_from_config (const char *command,
                                                 const vfs_path_t * filename_vpath,
                                                 long start_line);

/* --------------------------------------------------------------------------------------------- */

static void
edition_post_exec (void)
{
    tty_enter_ca_mode ();

    /* FIXME: Missing on slang endwin? */
    tty_reset_prog_mode ();
    tty_flush_input ();

    tty_keypad (true);
    tty_raw_mode ();
    channels_up ();
    enable_mouse ();
    enable_bracketed_paste ();
    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();
}

/* --------------------------------------------------------------------------------------------- */

static void
edition_pre_exec (void)
{
    if (clear_before_exec)
        clr_scr ();
    else
    {
        if (!(mc_global.tty.console_flag != '\0' || mc_global.tty.xterm_flag))
            printf ("\n\n");
    }

    channels_down ();
    disable_mouse ();
    disable_bracketed_paste ();

    tty_reset_shell_mode ();
    tty_keypad (false);
    tty_reset_screen ();

    numeric_keypad_mode ();

    /* on xterms: maybe endwin did not leave the terminal on the shell
     * screen page: do it now.
     *
     * Do not move this before endwin: in some systems rmcup includes
     * a call to clear screen, so it will end up clearing the shell screen.
     */
    tty_exit_ca_mode ();
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_SUBSHELL
static void
do_possible_cd (const vfs_path_t * new_dir_vpath)
{
    if (!do_cd (new_dir_vpath, cd_exact))
        message (D_ERROR, _("Warning"), "%s",
                 _("The Commander can't change to the directory that\n"
                   "the subshell claims you are in. Perhaps you have\n"
                   "deleted your working directory, or given yourself\n"
                   "extra access permissions with the \"su\" command?"));
}
#endif /* ENABLE_SUBSHELL */

/* --------------------------------------------------------------------------------------------- */

static void
do_suspend_cmd (void)
{
    pre_exec ();

    if (mc_global.tty.console_flag != '\0' && !mc_global.tty.use_subshell)
        handle_console (CONSOLE_RESTORE);

#ifdef SIGTSTP
    {
        struct sigaction sigtstp_action;

        memset (&sigtstp_action, 0, sizeof (sigtstp_action));
        /* Make sure that the SIGTSTP below will suspend us directly,
           without calling ncurses' SIGTSTP handler; we *don't* want
           ncurses to redraw the screen immediately after the SIGCONT */
        sigaction (SIGTSTP, &startup_handler, &sigtstp_action);

        kill (getpid (), SIGTSTP);

        /* Restore previous SIGTSTP action */
        sigaction (SIGTSTP, &sigtstp_action, nullptr);
    }
#endif /* SIGTSTP */

    if (mc_global.tty.console_flag != '\0' && !mc_global.tty.use_subshell)
        handle_console (CONSOLE_SAVE);

    edition_post_exec ();
}

/* --------------------------------------------------------------------------------------------- */

static bool
execute_prepare_with_vfs_arg (const vfs_path_t * filename_vpath, vfs_path_t ** localcopy_vpath,
                              time_t * mtime)
{
    struct stat st;

    /* Simplest case, this file is local */
    if ((filename_vpath == nullptr && vfs_file_is_local (vfs_get_raw_current_dir ()))
        || vfs_file_is_local (filename_vpath))
        return true;

    /* FIXME: Creation of new files on VFS is not supported */
    if (filename_vpath == nullptr)
        return false;

    *localcopy_vpath = mc_getlocalcopy (filename_vpath);
    if (*localcopy_vpath == nullptr)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot fetch a local copy of %s"),
                 vfs_path_as_str (filename_vpath));
        return false;
    }

    mc_stat (*localcopy_vpath, &st);
    *mtime = st.st_mtime;
    return true;
}

/* --------------------------------------------------------------------------------------------- */

static void
execute_cleanup_with_vfs_arg (const vfs_path_t * filename_vpath, vfs_path_t ** localcopy_vpath,
                              time_t * mtime)
{
    if (*localcopy_vpath != nullptr)
    {
        struct stat st;

        /*
         * filename can be an entry on panel, it can be changed by executing
         * the command, so make a copy.  Smarter VFS code would make the code
         * below unnecessary.
         */
        mc_stat (*localcopy_vpath, &st);
        mc_ungetlocalcopy (filename_vpath, *localcopy_vpath, *mtime != st.st_mtime);
        vfs_path_free (*localcopy_vpath);
        *localcopy_vpath = nullptr;
    }
}

/* --------------------------------------------------------------------------------------------- */

static char *
execute_get_opts_from_cfg (const char *command, const char *default_str)
{
    char *str_from_config;

    str_from_config =
        mc_config_get_string_raw (mc_global.main_config, CONFIG_EXT_EDITOR_VIEWER_SECTION, command,
                                  nullptr);

    if (str_from_config == nullptr)
    {
        mc_config_t *cfg;

        cfg = mc_config_init (global_profile_name, true);
        if (cfg == nullptr)
            return g_strdup (default_str);

        str_from_config =
            mc_config_get_string_raw (cfg, CONFIG_EXT_EDITOR_VIEWER_SECTION, command, default_str);

        mc_config_deinit (cfg);
    }

    return str_from_config;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

char *
execute_get_external_cmd_opts_from_config (const char *command, const vfs_path_t * filename_vpath,
                                           long start_line)
{
    char *str_from_config, *return_str;
    char *parameter;

    if (filename_vpath == nullptr)
        return g_strdup ("");

    parameter = g_shell_quote (vfs_path_get_last_path_str (filename_vpath));

    if (start_line <= 0)
        return parameter;

    str_from_config = execute_get_opts_from_cfg (command, "%filename");

    return_str = str_replace_all (str_from_config, "%filename", parameter);
    g_free (parameter);
    g_free (str_from_config);
    str_from_config = return_str;

    parameter = g_strdup_printf ("%ld", start_line);
    return_str = str_replace_all (str_from_config, "%lineno", parameter);
    g_free (parameter);
    g_free (str_from_config);

    return return_str;
}

/* --------------------------------------------------------------------------------------------- */

void
do_executev (const char *shell, int flags, char *const argv[])
{
#ifdef ENABLE_SUBSHELL
    vfs_path_t *new_dir_vpath = nullptr;
#endif /* ENABLE_SUBSHELL */

    vfs_path_t *old_vfs_dir_vpath = nullptr;

    if (!vfs_current_is_local ())
        old_vfs_dir_vpath = vfs_path_clone (vfs_get_raw_current_dir ());

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        save_cwds_stat ();
    pre_exec ();
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_RESTORE);

    if (!mc_global.tty.use_subshell && *argv != nullptr && (flags & EXECUTE_INTERNAL) == 0)
    {
        printf ("%s%s\n", mc_prompt, *argv);
        fflush (stdout);
    }
#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell && (flags & EXECUTE_INTERNAL) == 0)
    {
        do_update_prompt ();

        /* We don't care if it died, higher level takes care of this */
        invoke_subshell (*argv, VISIBLY, old_vfs_dir_vpath != nullptr ? nullptr : &new_dir_vpath);
    }
    else
#endif /* ENABLE_SUBSHELL */
        my_systemv_flags (flags, shell, argv);

    if ((flags & EXECUTE_INTERNAL) == 0)
    {
        if ((pause_after_run == pause_always
             || (pause_after_run == pause_on_dumb_terminals && !mc_global.tty.xterm_flag
                 && mc_global.tty.console_flag == '\0')) && quit == 0
#ifdef ENABLE_SUBSHELL
            && subshell_state != RUNNING_COMMAND
#endif /* ENABLE_SUBSHELL */
            )
        {
            printf ("%s", _("Press any key to continue..."));
            fflush (stdout);
            tty_raw_mode ();
            get_key_code (0);
            printf ("\r\n");
            fflush (stdout);
        }
        if (mc_global.tty.console_flag != '\0' && output_lines != 0 && mc_global.keybar_visible)
        {
            putchar ('\n');
            fflush (stdout);
        }
    }

    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);
    edition_post_exec ();

#ifdef ENABLE_SUBSHELL
    if (new_dir_vpath != nullptr)
    {
        do_possible_cd (new_dir_vpath);
        vfs_path_free (new_dir_vpath);
    }

#endif /* ENABLE_SUBSHELL */

    if (old_vfs_dir_vpath != nullptr)
    {
        mc_chdir (old_vfs_dir_vpath);
        vfs_path_free (old_vfs_dir_vpath);
    }

    if (mc_global.mc_run_mode == MC_RUN_FULL)
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        update_xterm_title_path ();
    }

    do_refresh ();
    use_dash (true);
}

/* --------------------------------------------------------------------------------------------- */

void
do_execute (const char *shell, const char *command, int flags)
{
    GPtrArray *args_array;

    args_array = g_ptr_array_new ();
    g_ptr_array_add (args_array, (char *) command);
    g_ptr_array_add (args_array, nullptr);

    do_executev (shell, flags, (char *const *) args_array->pdata);

    g_ptr_array_free (args_array, true);
}

/* --------------------------------------------------------------------------------------------- */

/** Set up the terminal before executing a program */

void
pre_exec (void)
{
    use_dash (false);
    edition_pre_exec ();
}

/* --------------------------------------------------------------------------------------------- */
/** Hide the terminal after executing a program */
void
post_exec (void)
{
    edition_post_exec ();
    use_dash (true);
    repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */
/* Executes a command */

void
shell_execute (const char *command, int flags)
{
    char *cmd = nullptr;

    if (flags & EXECUTE_HIDE)
    {
        cmd = g_strconcat (" ", command, (char *) nullptr);
        flags ^= EXECUTE_HIDE;
    }

#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell)
    {
        if (subshell_state == INACTIVE)
            do_execute (mc_global.shell->path, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);
        else
            message (D_ERROR, MSG_ERROR, "%s", _("The shell is already running a command"));
    }
    else
#endif /* ENABLE_SUBSHELL */
        do_execute (mc_global.shell->path, cmd ? cmd : command, flags | EXECUTE_AS_SHELL);

    g_free (cmd);
}

/* --------------------------------------------------------------------------------------------- */

void
toggle_subshell (void)
{
    static bool message_flag = true;

#ifdef ENABLE_SUBSHELL
    vfs_path_t *new_dir_vpath = nullptr;
#endif /* ENABLE_SUBSHELL */

    SIG_ATOMIC_VOLATILE_T was_sigwinch = 0;

    if (!(mc_global.tty.xterm_flag || mc_global.tty.console_flag != '\0'
          || mc_global.tty.use_subshell || output_starts_shell))
    {
        if (message_flag)
            message (D_ERROR, MSG_ERROR,
                     _("Not an xterm or Linux console;\nthe subshell cannot be toggled."));
        message_flag = false;
        return;
    }

    channels_down ();
    disable_mouse ();
    disable_bracketed_paste ();
    if (clear_before_exec)
        clr_scr ();
    if (mc_global.tty.alternate_plus_minus)
        numeric_keypad_mode ();
#ifndef HAVE_SLANG
    /* With slang we don't want any of this, since there
     * is no raw_mode supported
     */
    tty_reset_shell_mode ();
#endif /* !HAVE_SLANG */
    tty_noecho ();
    tty_keypad (false);
    tty_reset_screen ();
    tty_exit_ca_mode ();
    tty_raw_mode ();
    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_RESTORE);

#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell)
    {
        vfs_path_t **new_dir_p;

        new_dir_p = vfs_current_is_local ()? &new_dir_vpath : nullptr;
        invoke_subshell (nullptr, VISIBLY, new_dir_p);
    }
    else
#endif /* ENABLE_SUBSHELL */
    {
        if (output_starts_shell)
        {
            fputs (_("Type 'exit' to return to the Midnight Commander"), stderr);
            fputs ("\n\r\n\r", stderr);

            my_system (EXECUTE_INTERNAL, mc_global.shell->path, nullptr);
        }
        else
            get_key_code (0);
    }

    if (mc_global.tty.console_flag != '\0')
        handle_console (CONSOLE_SAVE);

    tty_enter_ca_mode ();

    tty_reset_prog_mode ();
    tty_keypad (true);

    /* Prevent screen flash when user did 'exit' or 'logout' within
       subshell */
    if ((quit & SUBSHELL_EXIT) != 0)
    {
        /* User did 'exit' or 'logout': quit MC */
        if (quiet_quit_cmd ())
            return;

        quit = 0;
#ifdef ENABLE_SUBSHELL
        /* restart subshell */
        if (mc_global.tty.use_subshell)
            init_subshell ();
#endif /* ENABLE_SUBSHELL */
    }

    enable_mouse ();
    enable_bracketed_paste ();
    channels_up ();
    if (mc_global.tty.alternate_plus_minus)
        application_keypad_mode ();

    /* HACK:
     * Save sigwinch flag that will be reset in mc_refresh() called via update_panels().
     * There is some problem with screen redraw in ncurses-based mc in this situation.
     */
    was_sigwinch = tty_got_winch ();
    tty_flush_winch ();

#ifdef ENABLE_SUBSHELL
    if (mc_global.tty.use_subshell)
    {
        if (mc_global.mc_run_mode == MC_RUN_FULL)
        {
            do_load_prompt ();
            if (new_dir_vpath != nullptr)
                do_possible_cd (new_dir_vpath);
        }
        else if (new_dir_vpath != nullptr && mc_chdir (new_dir_vpath) != -1)
            vfs_setup_cwd ();
    }

    vfs_path_free (new_dir_vpath);
#endif /* ENABLE_SUBSHELL */

    if (mc_global.mc_run_mode == MC_RUN_FULL)
    {
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
        update_xterm_title_path ();
    }

    if (was_sigwinch != 0 || tty_got_winch ())
        dialog_change_screen_size ();
    else
        repaint_screen ();
}

/* --------------------------------------------------------------------------------------------- */

/* event callback */
bool
execute_suspend (const gchar * event_group_name, const gchar * event_name,
                 gpointer init_data, gpointer data)
{
    (void) event_group_name;
    (void) event_name;
    (void) init_data;
    (void) data;

    if (mc_global.mc_run_mode == MC_RUN_FULL)
        save_cwds_stat ();
    do_suspend_cmd ();
    if (mc_global.mc_run_mode == MC_RUN_FULL)
        update_panels (UP_OPTIMIZE, UP_KEEPSEL);
    do_refresh ();

    return true;
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Execute command on a filename that can be on VFS.
 * Errors are reported to the user.
 */

void
execute_with_vfs_arg (const char *command, const vfs_path_t * filename_vpath)
{
    vfs_path_t *localcopy_vpath = nullptr;
    const vfs_path_t *do_execute_vpath;
    time_t mtime;

    if (!execute_prepare_with_vfs_arg (filename_vpath, &localcopy_vpath, &mtime))
        return;

    do_execute_vpath = (localcopy_vpath == nullptr) ? filename_vpath : localcopy_vpath;

    do_execute (command, vfs_path_get_last_path_str (do_execute_vpath), EXECUTE_INTERNAL);

    execute_cleanup_with_vfs_arg (filename_vpath, &localcopy_vpath, &mtime);
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Execute external editor or viewer.
 *
 * @param command editor/viewer to run
 * @param filename_vpath path for edit/view
 * @param start_line cursor will be placed at the 'start_line' position after opening file
 *        if start_line is 0 or negative, no start line will be passed to editor/viewer
 */

void
execute_external_editor_or_viewer (const char *command, const vfs_path_t * filename_vpath,
                                   long start_line)
{
    vfs_path_t *localcopy_vpath = nullptr;
    const vfs_path_t *do_execute_vpath;
    char *extern_cmd_options;
    time_t mtime = 0;

    if (!execute_prepare_with_vfs_arg (filename_vpath, &localcopy_vpath, &mtime))
        return;

    do_execute_vpath = (localcopy_vpath == nullptr) ? filename_vpath : localcopy_vpath;

    extern_cmd_options =
        execute_get_external_cmd_opts_from_config (command, do_execute_vpath, start_line);

    if (extern_cmd_options != nullptr)
    {
        char **argv_cmd_options;
        int argv_count;

        if (g_shell_parse_argv (extern_cmd_options, &argv_count, &argv_cmd_options, nullptr))
        {
            do_executev (command, EXECUTE_INTERNAL, argv_cmd_options);
            g_strfreev (argv_cmd_options);
        }
        else
            do_executev (command, EXECUTE_INTERNAL, nullptr);

        g_free (extern_cmd_options);
    }

    execute_cleanup_with_vfs_arg (filename_vpath, &localcopy_vpath, &mtime);
}

/* --------------------------------------------------------------------------------------------- */
