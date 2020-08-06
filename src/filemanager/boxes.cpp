/*
   Some misc dialog boxes for the program.

   Copyright (C) 1994-2020
   Free Software Foundation, Inc.

   Written by:
   Miguel de Icaza, 1994, 1995
   Jakub Jelinek, 1995
   Andrew Borodin <aborodin@vmail.ru>, 2009-2015

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

/** \file boxes.c
 *  \brief Source: Some misc dialog boxes for the program
 */

#include <config.h>

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/color.h"      /* tty_use_colors() */
#include "lib/tty/key.h"        /* XCTRL and ALT macros  */
#include "lib/skin.h"           /* INPUT_COLOR */
#include "lib/mcconfig.h"       /* Load/save user formats */
#include "lib/strutil.h"

#include "lib/vfs/vfs.h"
#ifdef ENABLE_VFS_FTP
#include "src/vfs/ftpfs/ftpfs.h"
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_SMB
#include "src/vfs/smbfs/smbfs.h"
#endif /* ENABLE_VFS_SMB */

#include "lib/util.h"           /* Q_() */
#include "lib/widget.h"

#include "src/setup.h"
#include "src/history.h"        /* MC_HISTORY_ESC_TIMEOUT */
#include "src/execute.h"        /* pause_after_run */
#ifdef ENABLE_BACKGROUND
#include "src/background.h"     /* task_list */
#endif

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#include "src/selcodepage.h"
#endif

#include "command.h"            /* For cmdline */
#include "dir.h"
#include "panel.h"              /* LIST_FORMATS */
#include "tree.h"
#include "layout.h"             /* for get_nth_panel_name proto */
#include "midnight.h"           /* current_panel */

#include "boxes.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#ifdef ENABLE_BACKGROUND
#define B_STOP   (B_USER+1)
#define B_RESUME (B_USER+2)
#define B_KILL   (B_USER+3)
#endif /* ENABLE_BACKGROUND */

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static unsigned long configure_old_esc_mode_id, configure_time_out_id;

/* Index in list_formats[] for "brief" */
static const int panel_list_brief_idx = 1;
/* Index in list_formats[] for "user defined" */
static const int panel_list_user_idx = 3;

static char **status_format;
static unsigned long panel_list_formats_id, panel_user_format_id, panel_brief_cols_id;
static unsigned long mini_user_status_id, mini_user_format_id;

#ifdef HAVE_CHARSET
static int new_display_codepage;
#endif /* HAVE_CHARSET */

#if defined(ENABLE_VFS) && defined(ENABLE_VFS_FTP)
static unsigned long ftpfs_always_use_proxy_id, ftpfs_proxy_host_id;
#endif /* ENABLE_VFS && ENABLE_VFS_FTP */

static GPtrArray *skin_names;
static gchar *current_skin_name;

#ifdef ENABLE_BACKGROUND
static WListbox *bg_list = nullptr;
#endif /* ENABLE_BACKGROUND */

static unsigned long shadows_id;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
configure_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_NOTIFY:
        /* message from "Single press" checkbutton */
        if (sender != nullptr && sender->id == configure_old_esc_mode_id)
        {
            const bool not_single = !CHECK (sender)->state;
            Widget *ww;

            /* input line */
            ww = widget_find_by_id (w, configure_time_out_id);
            widget_disable (ww, not_single);

            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
skin_apply (const gchar * skin_override)
{
    GError *mcerror = nullptr;

    mc_skin_deinit ();
    mc_skin_init (skin_override, &mcerror);
    mc_fhl_free (&mc_filehighlight);
    mc_filehighlight = mc_fhl_new (true);
    dlg_set_default_colors ();
    input_set_default_colors ();
    if (mc_global.mc_run_mode == MC_RUN_FULL)
        command_set_default_colors ();
    panel_deinit ();
    panel_init ();
    repaint_screen ();

    mc_error_message (&mcerror, nullptr);
}

/* --------------------------------------------------------------------------------------------- */

static const gchar *
skin_name_to_label (const gchar * name)
{
    if (strcmp (name, "default") == 0)
        return _("< Default >");
    return name;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
skin_dlg_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_RESIZE:
        {
            WDialog *d = DIALOG (w);
            Widget *wd = WIDGET (d->data);
            int y, x;
            WRect r;

            y = wd->y + (wd->lines - w->lines) / 2;
            x = wd->x + wd->cols / 2;
            rect_init (&r, y, x, w->lines, w->cols);

            return dlg_default_callback (w, nullptr, MSG_RESIZE, 0, &r);
        }

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
sel_skin_button (WButton * button, int action)
{
    int result;
    WListbox *skin_list;
    WDialog *skin_dlg;
    const gchar *skin_name;
    unsigned int i;
    unsigned int pos = 1;

    (void) action;

    skin_dlg =
        dlg_create (true, 0, 0, 13, 24, WPOS_KEEP_DEFAULT, true, dialog_colors, skin_dlg_callback,
                    nullptr, "[Appearance]", _("Skins"));
    /* use Appearance dialog for positioning */
    skin_dlg->data = WIDGET (button)->owner;

    /* set dialog location before all */
    send_message (skin_dlg, nullptr, MSG_RESIZE, 0, nullptr);

    skin_list = listbox_new (1, 1, 11, 22, false, nullptr);
    skin_name = "default";
    listbox_add_item (skin_list, LISTBOX_APPEND_AT_END, 0, skin_name_to_label (skin_name),
                      (void *) skin_name, false);

    if (strcmp (skin_name, current_skin_name) == 0)
        listbox_select_entry (skin_list, 0);

    for (i = 0; i < skin_names->len; i++)
    {
        skin_name = static_cast<const gchar *> (g_ptr_array_index (skin_names, i));
        if (strcmp (skin_name, "default") != 0)
        {
            listbox_add_item (skin_list, LISTBOX_APPEND_AT_END, 0, skin_name_to_label (skin_name),
                              (void *) skin_name, false);
            if (strcmp (skin_name, current_skin_name) == 0)
                listbox_select_entry (skin_list, pos);
            pos++;
        }
    }

    /* make list stick to all sides of dialog, effectively make it be resized with dialog */
    group_add_widget_autopos (GROUP (skin_dlg), skin_list, WPOS_KEEP_ALL, nullptr);

    result = dlg_run (skin_dlg);
    if (result == B_ENTER)
    {
        gchar *skin_label;

        listbox_get_current (skin_list, &skin_label, (void **) &skin_name);
        g_free (current_skin_name);
        current_skin_name = g_strdup (skin_name);
        skin_apply (skin_name);

        button_set_text (button, str_fit_to_term (skin_label, 20, J_LEFT_FIT));
    }
    dlg_destroy (skin_dlg);

    return 0;
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
appearance_box_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_INIT:
        if (!tty_use_colors ())
        {
            Widget *shadow;

            shadow = widget_find_by_id (w, shadows_id);
            CHECK (shadow)->state = false;
            widget_disable (shadow, true);
        }
        return MSG_HANDLED;

    case MSG_NOTIFY:
        if (sender != nullptr && sender->id == shadows_id)
        {
            mc_global.tty.shadows = CHECK (sender)->state;
            repaint_screen ();
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
panel_listing_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_NOTIFY:
        if (sender != nullptr && sender->id == panel_list_formats_id)
        {
            WCheck *ch;
            WInput *in1, *in2, *in3;

            in1 = INPUT (widget_find_by_id (w, panel_user_format_id));
            in2 = INPUT (widget_find_by_id (w, panel_brief_cols_id));
            ch = CHECK (widget_find_by_id (w, mini_user_status_id));
            in3 = INPUT (widget_find_by_id (w, mini_user_format_id));

            if (!ch->state)
                input_assign_text (in3, status_format[RADIO (sender)->sel]);
            input_update (in1, false);
            input_update (in2, false);
            input_update (in3, false);
            widget_disable (WIDGET (in1), RADIO (sender)->sel != panel_list_user_idx);
            widget_disable (WIDGET (in2), RADIO (sender)->sel != panel_list_brief_idx);
            return MSG_HANDLED;
        }

        if (sender != nullptr && sender->id == mini_user_status_id)
        {
            WInput *in;

            in = INPUT (widget_find_by_id (w, mini_user_format_id));

            if (CHECK (sender)->state)
            {
                widget_disable (WIDGET (in), false);
                input_assign_text (in, status_format[3]);
            }
            else
            {
                WRadio *r;

                r = RADIO (widget_find_by_id (w, panel_list_formats_id));
                widget_disable (WIDGET (in), true);
                input_assign_text (in, status_format[r->sel]);
            }
            /* input_update (in, false); */
            return MSG_HANDLED;
        }

        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef HAVE_CHARSET
static int
sel_charset_button (WButton * button, int action)
{
    int new_dcp;

    (void) action;

    new_dcp = select_charset (-1, -1, new_display_codepage, true);

    if (new_dcp != SELECT_CHARSET_CANCEL)
    {
        const char *cpname;

        new_display_codepage = new_dcp;
        cpname = (new_display_codepage == SELECT_CHARSET_OTHER_8BIT) ?
            _("Other 8 bit") :
            ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;
        if (cpname != nullptr)
            mc_global.utf8_display = str_isutf8 (cpname);
        else
            cpname = _("7-bit ASCII");  /* FIXME */

        button_set_text (button, cpname);
        widget_draw (WIDGET (WIDGET (button)->owner));
    }

    return 0;
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
tree_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_RESIZE:
        {
            WRect r;
            Widget *bar;

            rect_init (&r, w->y, w->x, LINES - 9, COLS - 20);
            dlg_default_callback (w, nullptr, MSG_RESIZE, 0, &r);

            bar = WIDGET (find_buttonbar (h));
            bar->x = 0;
            bar->y = LINES - 1;
            return MSG_HANDLED;
        }

    case MSG_ACTION:
        return send_message (find_tree (h), nullptr, MSG_ACTION, parm, nullptr);

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */

#if defined(ENABLE_VFS) && defined (ENABLE_VFS_FTP)
static cb_ret_t
confvfs_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    switch (msg)
    {
    case MSG_NOTIFY:
        /* message from "Always use ftp proxy" checkbutton */
        if (sender != nullptr && sender->id == ftpfs_always_use_proxy_id)
        {
            const bool not_use = !CHECK (sender)->state;
            Widget *wi;

            /* input */
            wi = widget_find_by_id (w, ftpfs_proxy_host_id);
            widget_disable (wi, not_use);
            return MSG_HANDLED;
        }
        return MSG_NOT_HANDLED;

    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}
#endif /* ENABLE_VFS && ENABLE_VFS_FTP */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
static void
jobs_fill_listbox (WListbox * list)
{
    static const char *state_str[2] = { "", "" };
    TaskList *tl;

    if (state_str[0][0] == '\0')
    {
        state_str[0] = _("Running");
        state_str[1] = _("Stopped");
    }

    for (tl = task_list; tl != nullptr; tl = tl->next)
    {
        char *s;

        s = g_strconcat (state_str[tl->state], " ", tl->info, (char *) nullptr);
        listbox_add_item (list, LISTBOX_APPEND_AT_END, 0, s, (void *) tl, false);
        g_free (s);
    }
}

/* --------------------------------------------------------------------------------------------- */

static int
task_cb (WButton * button, int action)
{
    TaskList *tl;
    int sig = 0;

    (void) button;

    if (bg_list->list == nullptr)
        return 0;

    /* Get this instance information */
    listbox_get_current (bg_list, nullptr, (void **) &tl);

#ifdef SIGTSTP
    if (action == B_STOP)
    {
        sig = SIGSTOP;
        tl->state = Task_Stopped;
    }
    else if (action == B_RESUME)
    {
        sig = SIGCONT;
        tl->state = Task_Running;
    }
    else
#endif
    if (action == B_KILL)
        sig = SIGKILL;

    if (sig == SIGKILL)
        unregister_task_running (tl->pid, tl->fd);

    kill (tl->pid, sig);
    listbox_remove_list (bg_list);
    jobs_fill_listbox (bg_list);

    /* This can be optimized to just redraw this widget :-) */
    widget_draw (WIDGET (WIDGET (button)->owner));

    return 0;
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

void
configure_box (void)
{
    const char *pause_options[] = {
        N_("&Never"),
        N_("On dum&b terminals"),
        N_("Alwa&ys")
    };

    int pause_options_num;

    pause_options_num = G_N_ELEMENTS (pause_options);

    {
        char time_out[BUF_TINY] = "";
        char *time_out_new;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("File operations")),
                    QUICK_CHECKBOX (N_("&Verbose operation"), &verbose, nullptr),
                    QUICK_CHECKBOX (N_("Compute tota&ls"), &file_op_compute_totals, nullptr),
                    QUICK_CHECKBOX (N_("Classic pro&gressbar"), &classic_progressbar, nullptr),
                    QUICK_CHECKBOX (N_("Mkdi&r autoname"), &auto_fill_mkdir_name, nullptr),
                    QUICK_CHECKBOX (N_("&Preallocate space"), &mc_global.vfs.preallocate_space,
                                    nullptr),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Esc key mode")),
                    QUICK_CHECKBOX (N_("S&ingle press"), &old_esc_mode, &configure_old_esc_mode_id),
                    QUICK_LABELED_INPUT (N_("Timeout:"), input_label_left,
                                         (const char *) time_out, MC_HISTORY_ESC_TIMEOUT,
                                         &time_out_new, &configure_time_out_id, false, false,
                                         INPUT_COMPLETE_NONE),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Pause after run")),
                    QUICK_RADIO (pause_options_num, pause_options, &pause_after_run, nullptr),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Other options")),
                    QUICK_CHECKBOX (N_("Use internal edi&t"), &use_internal_edit, nullptr),
                    QUICK_CHECKBOX (N_("Use internal vie&w"), &use_internal_view, nullptr),
                    QUICK_CHECKBOX (N_("A&sk new file name"),
                                    &editor_ask_filename_before_edit, nullptr),
                    QUICK_CHECKBOX (N_("Auto m&enus"), &auto_menu, nullptr),
                    QUICK_CHECKBOX (N_("&Drop down menus"), &drop_menus, nullptr),
                    QUICK_CHECKBOX (N_("S&hell patterns"), &easy_patterns, nullptr),
                    QUICK_CHECKBOX (N_("Co&mplete: show all"),
                                    &mc_global.widget.show_all_if_ambiguous, nullptr),
                    QUICK_CHECKBOX (N_("Rotating d&ash"), &nice_rotating_dash, nullptr),
                    QUICK_CHECKBOX (N_("Cd follows lin&ks"), &mc_global.vfs.cd_symlinks, nullptr),
                    QUICK_CHECKBOX (N_("Sa&fe delete"), &safe_delete, nullptr),
                    QUICK_CHECKBOX (N_("Safe overwrite"), &safe_overwrite, nullptr),       /* w/o hotkey */
                    QUICK_CHECKBOX (N_("A&uto save setup"), &auto_save_setup, nullptr),
                    QUICK_SEPARATOR (false),
                    QUICK_SEPARATOR (false),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Configure options"), "[Configuration]",
            quick_widgets, configure_callback, nullptr
        };

        g_snprintf (time_out, sizeof (time_out), "%d", old_esc_mode_timeout);

#ifndef USE_INTERNAL_EDIT
        quick_widgets[17].state = WST_DISABLED;
#endif

        if (!old_esc_mode)
            quick_widgets[10].state = quick_widgets[11].state = WST_DISABLED;

#ifndef HAVE_POSIX_FALLOCATE
        mc_global.vfs.preallocate_space = false;
        quick_widgets[7].state = WST_DISABLED;
#endif

        if (quick_dialog (&qdlg) == B_ENTER)
            old_esc_mode_timeout = atoi (time_out_new);

        g_free (time_out_new);
    }
}

/* --------------------------------------------------------------------------------------------- */

void
appearance_box (void)
{
    bool shadows = mc_global.tty.shadows;

    current_skin_name = g_strdup (mc_skin__default.name);
    skin_names = mc_skin_list ();

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_LABEL (N_("Skin:"), nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_BUTTON (str_fit_to_term (skin_name_to_label (current_skin_name), 20, J_LEFT_FIT),
                              B_USER, sel_skin_button, nullptr),
            QUICK_STOP_COLUMNS,
            QUICK_SEPARATOR (true),
            QUICK_CHECKBOX (N_("&Shadows"), &mc_global.tty.shadows, &shadows_id),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 54,
            N_("Appearance"), "[Appearance]",
            quick_widgets, appearance_box_callback, nullptr
        };

        if (quick_dialog (&qdlg) == B_ENTER)
            mc_config_set_string (mc_global.main_config, CONFIG_APP_SECTION, "skin",
                                  current_skin_name);
        else
        {
            skin_apply (nullptr);
            mc_global.tty.shadows = shadows;
        }
    }

    g_free (current_skin_name);
    g_ptr_array_foreach (skin_names, (GFunc) g_free, nullptr);
    g_ptr_array_free (skin_names, true);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_options_box (void)
{
    bool simple_swap;

    simple_swap = mc_config_get_bool (mc_global.main_config, CONFIG_PANELS_SECTION,
                                      "simple_swap", false);
    {
        const char *qsearch_options[] = {
            N_("Case &insensitive"),
            N_("Cas&e sensitive"),
            N_("Use panel sort mo&de")
        };

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_START_GROUPBOX (N_("Main options")),
                    QUICK_CHECKBOX (N_("Show mi&ni-status"), &panels_options.show_mini_info, nullptr),
                    QUICK_CHECKBOX (N_("Use SI si&ze units"), &panels_options.kilobyte_si, nullptr),
                    QUICK_CHECKBOX (N_("Mi&x all files"), &panels_options.mix_all_files, nullptr),
                    QUICK_CHECKBOX (N_("Show &backup files"), &panels_options.show_backups, nullptr),
                    QUICK_CHECKBOX (N_("Show &hidden files"), &panels_options.show_dot_files, nullptr),
                    QUICK_CHECKBOX (N_("&Fast dir reload"), &panels_options.fast_reload, nullptr),
                    QUICK_CHECKBOX (N_("Ma&rk moves down"), &panels_options.mark_moves_down, nullptr),
                    QUICK_CHECKBOX (N_("Re&verse files only"), &panels_options.reverse_files_only,
                                    nullptr),
                    QUICK_CHECKBOX (N_("Simple s&wap"), &simple_swap, nullptr),
                    QUICK_CHECKBOX (N_("A&uto save panels setup"), &panels_options.auto_save_setup,
                                    nullptr),
                    QUICK_SEPARATOR (false),
                    QUICK_SEPARATOR (false),
                    QUICK_SEPARATOR (false),
                QUICK_STOP_GROUPBOX,
            QUICK_NEXT_COLUMN,
                QUICK_START_GROUPBOX (N_("Navigation")),
                    QUICK_CHECKBOX (N_("L&ynx-like motion"), &panels_options.navigate_with_arrows,
                                    nullptr),
                    QUICK_CHECKBOX (N_("Pa&ge scrolling"), &panels_options.scroll_pages, nullptr),
                    QUICK_CHECKBOX (N_("Center &scrolling"), &panels_options.scroll_center, nullptr),
                    QUICK_CHECKBOX (N_("&Mouse page scrolling"), &panels_options.mouse_move_pages,
                                    nullptr),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("File highlight")),
                    QUICK_CHECKBOX (N_("File &types"), &panels_options.filetype_mode, nullptr),
                    QUICK_CHECKBOX (N_("&Permissions"), &panels_options.permission_mode, nullptr),
                QUICK_STOP_GROUPBOX,
                QUICK_START_GROUPBOX (N_("Quick search")),
                    QUICK_RADIO (QSEARCH_NUM, qsearch_options, (int *) &panels_options.qsearch_mode,
                                 nullptr),
                QUICK_STOP_GROUPBOX,
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 60,
            N_("Panel options"), "[Panel options]",
            quick_widgets, nullptr, nullptr
        };

        if (quick_dialog (&qdlg) != B_ENTER)
            return;
    }

    mc_config_set_bool (mc_global.main_config, CONFIG_PANELS_SECTION, "simple_swap", simple_swap);

    if (!panels_options.fast_reload_msg_shown && panels_options.fast_reload)
    {
        message (D_NORMAL, _("Information"),
                 _("Using the fast reload option may not reflect the exact\n"
                   "directory contents. In this case you'll need to do a\n"
                   "manual reload of the directory. See the man page for\n" "the details."));
        panels_options.fast_reload_msg_shown = true;
    }

    update_panels (UP_RELOAD, UP_KEEPSEL);
}

/* --------------------------------------------------------------------------------------------- */

/* return list type */
int
panel_listing_box (WPanel * panel, int num, char **userp, char **minip, bool * use_msformat,
                   int *brief_cols)
{
    int result = -1;
    const char *p = nullptr;

    if (panel == nullptr)
    {
        p = get_nth_panel_name (num);
        panel = panel_empty_new (p);
    }

    {
        bool mini_user_status;
        char panel_brief_cols_in[BUF_TINY];
        char *panel_brief_cols_out = nullptr;
        char *panel_user_format = nullptr;
        char *mini_user_format = nullptr;

        /* Controls whether the array strings have been translated */
        const char *list_formats[LIST_FORMATS] = {
            N_("&Full file list"),
            N_("&Brief file list:"),
            N_("&Long file list"),
            N_("&User defined:")
        };

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_RADIO (LIST_FORMATS, list_formats, &result, &panel_list_formats_id),
            QUICK_NEXT_COLUMN,
                QUICK_SEPARATOR (false),
                QUICK_LABELED_INPUT (_ ("columns"), input_label_right, panel_brief_cols_in,
                                     "panel-brief-cols-input", &panel_brief_cols_out,
                                     &panel_brief_cols_id, false, false, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_INPUT (panel->user_format, "user-fmt-input", &panel_user_format,
                         &panel_user_format_id, false, false, INPUT_COMPLETE_NONE),
            QUICK_SEPARATOR (true),
            QUICK_CHECKBOX (N_("User &mini status"), &mini_user_status, &mini_user_status_id),
            QUICK_INPUT (panel->user_status_format[panel->list_format], "mini_input",
                         &mini_user_format, &mini_user_format_id, false, false, INPUT_COMPLETE_NONE),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 48,
            N_("Listing format"), "[Listing Format...]",
            quick_widgets, panel_listing_callback, nullptr
        };

        mini_user_status = panel->user_mini_status;
        result = panel->list_format;
        status_format = panel->user_status_format;

        g_snprintf (panel_brief_cols_in, sizeof (panel_brief_cols_in), "%d", panel->brief_cols);

        if ((int) panel->list_format != panel_list_brief_idx)
            quick_widgets[4].state = WST_DISABLED;

        if ((int) panel->list_format != panel_list_user_idx)
            quick_widgets[6].state = WST_DISABLED;

        if (!mini_user_status)
            quick_widgets[9].state = WST_DISABLED;

        if (quick_dialog (&qdlg) == B_CANCEL)
            result = -1;
        else
        {
            int cols;
            char *error = nullptr;

            *userp = panel_user_format;
            *minip = mini_user_format;
            *use_msformat = mini_user_status;

            cols = strtol (panel_brief_cols_out, &error, 10);
            if (*error == '\0')
                *brief_cols = cols;
            else
                *brief_cols = panel->brief_cols;

            g_free (panel_brief_cols_out);
        }
    }

    if (p != nullptr)
    {
        int i;

        g_free (panel->user_format);
        for (i = 0; i < LIST_FORMATS; i++)
            g_free (panel->user_status_format[i]);
        g_free (panel);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

const panel_field_t *
sort_box (dir_sort_options_t * op, const panel_field_t * sort_field)
{
    char **sort_orders_names;
    gsize i;
    gsize sort_names_num = 0;
    int sort_idx = 0;
    const panel_field_t *result = nullptr;

    sort_orders_names = panel_get_sortable_fields (&sort_names_num);

    for (i = 0; i < sort_names_num; i++)
        if (strcmp (sort_orders_names[i], _(sort_field->title_hotkey)) == 0)
        {
            sort_idx = i;
            break;
        }

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_RADIO (static_cast<int> (sort_names_num), (const char **) sort_orders_names, &sort_idx, nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Executable &first"), &op->exec_first, nullptr),
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &op->case_sensitive, nullptr),
                QUICK_CHECKBOX (N_("&Reverse"), &op->reverse, nullptr),
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 40,
            N_("Sort order"), "[Sort Order...]",
            quick_widgets, nullptr, nullptr
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
            result = panel_get_field_by_title_hotkey (sort_orders_names[sort_idx]);

        if (result == nullptr)
            result = sort_field;
    }

    g_strfreev (sort_orders_names);

    return result;
}

/* --------------------------------------------------------------------------------------------- */

void
confirm_box (void)
{
    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        /* TRANSLATORS: no need to translate 'Confirmation', it's just a context prefix */
        QUICK_CHECKBOX (Q_("Confirmation|&Delete"), &confirm_delete, nullptr),
        QUICK_CHECKBOX (Q_("Confirmation|O&verwrite"), &confirm_overwrite, nullptr),
        QUICK_CHECKBOX (Q_("Confirmation|&Execute"), &confirm_execute, nullptr),
        QUICK_CHECKBOX (Q_("Confirmation|E&xit"), &confirm_exit, nullptr),
        QUICK_CHECKBOX (Q_("Confirmation|Di&rectory hotlist delete"),
                        &confirm_directory_hotlist_delete, nullptr),
        QUICK_CHECKBOX (Q_("Confirmation|&History cleanup"),
                        &mc_global.widget.confirm_history_cleanup, nullptr),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 46,
        N_("Confirmation"), "[Confirmation]",
        quick_widgets, nullptr, nullptr
    };

    (void) quick_dialog (&qdlg);
}

/* --------------------------------------------------------------------------------------------- */

#ifndef HAVE_CHARSET
void
display_bits_box (void)
{
    bool new_meta;
    int current_mode;

    const char *display_bits_str[] = {
        N_("&UTF-8 output"),
        N_("&Full 8 bits output"),
        N_("&ISO 8859-1"),
        N_("7 &bits")
    };

    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_RADIO (4, display_bits_str, &current_mode, nullptr),
        QUICK_SEPARATOR (true),
        QUICK_CHECKBOX (N_("F&ull 8 bits input"), &new_meta, nullptr),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 46,
        _("Display bits"), "[Display bits]",
        quick_widgets, nullptr, nullptr
    };

    if (mc_global.full_eight_bits)
        current_mode = 0;
    else if (mc_global.eight_bit_clean)
        current_mode = 1;
    else
        current_mode = 2;

    new_meta = !use_8th_bit_as_meta;

    if (quick_dialog (&qdlg) != B_CANCEL)
    {
        mc_global.eight_bit_clean = current_mode < 3;
        mc_global.full_eight_bits = current_mode < 2;
#ifndef HAVE_SLANG
        tty_display_8bit (mc_global.eight_bit_clean);
#else
        tty_display_8bit (mc_global.full_eight_bits);
#endif
        use_8th_bit_as_meta = !new_meta;
    }
}

/* --------------------------------------------------------------------------------------------- */
#else /* HAVE_CHARSET */

void
display_bits_box (void)
{
    const char *cpname;

    new_display_codepage = mc_global.display_codepage;

    cpname = (new_display_codepage < 0) ? _("Other 8 bit")
        : ((codepage_desc *) g_ptr_array_index (codepages, new_display_codepage))->name;

    {
        bool new_meta;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_START_COLUMNS,
                QUICK_LABEL (N_("Input / display codepage:"), nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_BUTTON (cpname, B_USER, sel_charset_button, nullptr),
            QUICK_STOP_COLUMNS,
            QUICK_SEPARATOR (true),
                QUICK_CHECKBOX (N_("F&ull 8 bits input"), &new_meta, nullptr),
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 46,
            N_("Display bits"), "[Display bits]",
            quick_widgets, nullptr, nullptr
        };

        new_meta = !use_8th_bit_as_meta;
        application_keypad_mode ();

        if (quick_dialog (&qdlg) == B_ENTER)
        {
            char *errmsg;

            mc_global.display_codepage = new_display_codepage;

            errmsg = init_translation_table (mc_global.source_codepage, mc_global.display_codepage);
            if (errmsg != nullptr)
            {
                message (D_ERROR, MSG_ERROR, "%s", errmsg);
                g_free (errmsg);
            }

#ifdef HAVE_SLANG
            tty_display_8bit (mc_global.display_codepage != 0 && mc_global.display_codepage != 1);
#else
            tty_display_8bit (mc_global.display_codepage != 0);
#endif
            use_8th_bit_as_meta = !new_meta;

            repaint_screen ();
        }
    }
}
#endif /* HAVE_CHARSET */

/* --------------------------------------------------------------------------------------------- */
/** Show tree in a box, not on a panel */

char *
tree_box (const char *current_dir)
{
    WTree *mytree;
    WDialog *dlg;
    WGroup *g;
    Widget *wd;
    char *val = nullptr;
    WButtonBar *bar;

    (void) current_dir;

    /* Create the components */
    dlg = dlg_create (true, 0, 0, LINES - 9, COLS - 20, WPOS_CENTER, false, dialog_colors,
                      tree_callback, nullptr, "[Directory Tree]", _("Directory tree"));
    g = GROUP (dlg);
    wd = WIDGET (dlg);

    mytree = tree_new (2, 2, wd->lines - 6, wd->cols - 5, false);
    group_add_widget_autopos (g, mytree, WPOS_KEEP_ALL, nullptr);
    group_add_widget_autopos (g, hline_new (wd->lines - 4, 1, -1), WPOS_KEEP_BOTTOM, nullptr);
    bar = buttonbar_new (true);
    group_add_widget (g, bar);
    /* restore ButtonBar coordinates after add_widget() */
    WIDGET (bar)->x = 0;
    WIDGET (bar)->y = LINES - 1;

    if (dlg_run (dlg) == B_ENTER)
    {
        const vfs_path_t *selected_name;

        selected_name = tree_selected_name (mytree);
        val = g_strdup (vfs_path_as_str (selected_name));
    }

    dlg_destroy (dlg);
    return val;
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS
void
configure_vfs_box (void)
{
    char buffer2[BUF_TINY];
#ifdef ENABLE_VFS_FTP
    char buffer3[BUF_TINY];

    g_snprintf (buffer3, sizeof (buffer3), "%i", ftpfs_directory_timeout);
#endif

    g_snprintf (buffer2, sizeof (buffer2), "%i", vfs_timeout);

    {
        char *ret_timeout;
#ifdef ENABLE_VFS_FTP
        char *ret_passwd;
        char *ret_ftp_proxy;
        char *ret_directory_timeout;
#endif /* ENABLE_VFS_FTP */

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Timeout for freeing VFSs (sec):"), input_label_left,
                                 buffer2, "input-timo-vfs", &ret_timeout, nullptr, false, false,
                                 INPUT_COMPLETE_NONE),
#ifdef ENABLE_VFS_FTP
            QUICK_SEPARATOR (true),
            QUICK_LABELED_INPUT (N_("FTP anonymous password:"), input_label_left,
                                 ftpfs_anonymous_passwd, "input-passwd", &ret_passwd, nullptr,
                                 false, false, INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_("FTP directory cache timeout (sec):"), input_label_left,
                                 buffer3, "input-timeout", &ret_directory_timeout, nullptr,
                                 false, false, INPUT_COMPLETE_NONE),
            QUICK_CHECKBOX (N_("&Always use ftp proxy:"), &ftpfs_always_use_proxy,
                            &ftpfs_always_use_proxy_id),
            QUICK_INPUT (ftpfs_proxy_host, "input-ftp-proxy", &ret_ftp_proxy,
                         &ftpfs_proxy_host_id, false, false, INPUT_COMPLETE_HOSTNAMES),
            QUICK_CHECKBOX (N_("&Use ~/.netrc"), &ftpfs_use_netrc, nullptr),
            QUICK_CHECKBOX (N_("Use &passive mode"), &ftpfs_use_passive_connections, nullptr),
            QUICK_CHECKBOX (N_("Use passive mode over pro&xy"),
                            &ftpfs_use_passive_connections_over_proxy, nullptr),
#endif /* ENABLE_VFS_FTP */
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 56,
            N_("Virtual File System Setting"), "[Virtual FS]",
            quick_widgets,
#ifdef ENABLE_VFS_FTP
            confvfs_callback,
#else
            nullptr,
#endif
            nullptr,
        };

#ifdef ENABLE_VFS_FTP
        if (!ftpfs_always_use_proxy)
            quick_widgets[5].state = WST_DISABLED;
#endif

        if (quick_dialog (&qdlg) != B_CANCEL)
        {
            /* cppcheck-suppress uninitvar */
            vfs_timeout = atoi (ret_timeout);
            g_free (ret_timeout);

            if (vfs_timeout < 0 || vfs_timeout > 10000)
                vfs_timeout = 10;
#ifdef ENABLE_VFS_FTP
            g_free (ftpfs_anonymous_passwd);
            /* cppcheck-suppress uninitvar */
            ftpfs_anonymous_passwd = ret_passwd;
            g_free (ftpfs_proxy_host);
            /* cppcheck-suppress uninitvar */
            ftpfs_proxy_host = ret_ftp_proxy;
            /* cppcheck-suppress uninitvar */
            ftpfs_directory_timeout = atoi (ret_directory_timeout);
            g_free (ret_directory_timeout);
#endif
        }
    }
}

#endif /* ENABLE_VFS */

/* --------------------------------------------------------------------------------------------- */

char *
cd_box (void)
{
    const Widget *w = CONST_WIDGET (current_panel);
    char *my_str;

    quick_widget_t quick_widgets[] = {
        QUICK_LABELED_INPUT (N_("cd"), input_label_left, "", "input", &my_str, nullptr, false, true,
                             INPUT_COMPLETE_FILENAMES | INPUT_COMPLETE_CD),
        QUICK_END
    };

    quick_dialog_t qdlg = {
        w->y + w->lines - 6, w->x, w->cols,
        N_("Quick cd"), "[Quick cd]",
        quick_widgets, nullptr, nullptr
    };

    return (quick_dialog (&qdlg) != B_CANCEL) ? my_str : nullptr;
}

/* --------------------------------------------------------------------------------------------- */

void
symlink_box (const vfs_path_t * existing_vpath, const vfs_path_t * new_vpath,
             char **ret_existing, char **ret_new)
{
    quick_widget_t quick_widgets[] = {
        /* *INDENT-OFF* */
        QUICK_LABELED_INPUT (N_("Existing filename (filename symlink will point to):"),
                             input_label_above, vfs_path_as_str (existing_vpath), "input-2",
                             ret_existing, nullptr, false, false, INPUT_COMPLETE_FILENAMES),
        QUICK_SEPARATOR (false),
        QUICK_LABELED_INPUT (N_("Symbolic link filename:"), input_label_above,
                             vfs_path_as_str (new_vpath), "input-1",
                             ret_new, nullptr, false, false, INPUT_COMPLETE_FILENAMES),
        QUICK_BUTTONS_OK_CANCEL,
        QUICK_END
        /* *INDENT-ON* */
    };

    quick_dialog_t qdlg = {
        -1, -1, 64,
        N_("Symbolic link"), "[File Menu]",
        quick_widgets, nullptr, nullptr
    };

    if (quick_dialog (&qdlg) == B_CANCEL)
    {
        *ret_new = nullptr;
        *ret_existing = nullptr;
    }
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_BACKGROUND
void
jobs_box (void)
{
    struct
    {
        const char *name;
        button_flags_t flags;
        int value;
        int len;
        bcback_fn callback;
    }
    job_but[] =
    {
        /* *INDENT-OFF* */
        { N_("&Stop"), NORMAL_BUTTON, B_STOP, 0, task_cb },
        { N_("&Resume"), NORMAL_BUTTON, B_RESUME, 0, task_cb },
        { N_("&Kill"), NORMAL_BUTTON, B_KILL, 0, task_cb },
        { N_("&OK"), DEFPUSH_BUTTON, B_CANCEL, 0, nullptr }
        /* *INDENT-ON* */
    };

    size_t i;
    const size_t n_but = G_N_ELEMENTS (job_but);

    WDialog *jobs_dlg;
    WGroup *g;
    int cols = 60;
    int lines = 15;
    int x = 0;

    for (i = 0; i < n_but; i++)
    {
#ifdef ENABLE_NLS
        job_but[i].name = _(job_but[i].name);
#endif /* ENABLE_NLS */

        job_but[i].len = str_term_width1 (job_but[i].name) + 3;
        if (job_but[i].flags == DEFPUSH_BUTTON)
            job_but[i].len += 2;
        x += job_but[i].len;
    }

    x += (int) n_but - 1;
    cols = MAX (cols, x + 6);

    jobs_dlg = dlg_create (true, 0, 0, lines, cols, WPOS_CENTER, false, dialog_colors, nullptr, nullptr,
                           "[Background jobs]", _("Background jobs"));
    g = GROUP (jobs_dlg);

    bg_list = listbox_new (2, 2, lines - 6, cols - 6, false, nullptr);
    jobs_fill_listbox (bg_list);
    group_add_widget (g, bg_list);

    group_add_widget (g, hline_new (lines - 4, -1, -1));

    x = (cols - x) / 2;
    for (i = 0; i < n_but; i++)
    {
        group_add_widget (g, static_cast<WButton*> (button_new (lines - 3, x, job_but[i].value, job_but[i].flags,
                                         job_but[i].name, job_but[i].callback)));
        x += job_but[i].len + 1;
    }

    (void) dlg_run (jobs_dlg);
    dlg_destroy (jobs_dlg);
}
#endif /* ENABLE_BACKGROUND */

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_SMB
struct smb_authinfo *
vfs_smb_get_authinfo (const char *host, const char *share, const char *domain, const char *user)
{
    char *label;
    struct smb_authinfo *return_value = nullptr;

    if (domain == nullptr)
        domain = "";
    if (user == nullptr)
        user = "";

    label = g_strdup_printf (_("Password for \\\\%s\\%s"), host, share);

    {
        char *ret_domain, *ret_user, *ret_password;

        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABEL (label, nullptr),
            QUICK_SEPARATOR (true),
            QUICK_START_COLUMNS,
                QUICK_LABEL (N_("Domain:"), nullptr),
                QUICK_SEPARATOR (false),
                QUICK_LABEL (N_("Username:"), nullptr),
                QUICK_SEPARATOR (false),
                QUICK_LABEL (N_("Password:"), nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_INPUT (domain, "auth_domain", &ret_domain, nullptr, false, false, INPUT_COMPLETE_HOSTNAMES),
                QUICK_SEPARATOR (false),
                QUICK_INPUT (user, "auth_name", &ret_user, nullptr, false, false, INPUT_COMPLETE_USERNAMES),
                QUICK_SEPARATOR (false),
                QUICK_INPUT ("", "auth_password", &ret_password, nullptr, true, false, INPUT_COMPLETE_NONE),
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 40,
            N_("SMB authentication"), "[Smb Authinfo]",
            quick_widgets, nullptr, nullptr
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
        {
            return_value = vfs_smb_authinfo_new (host, share, ret_domain, ret_user, ret_password);

            g_free (ret_domain);
            g_free (ret_user);
            g_free (ret_password);
        }
    }

    g_free (label);

    return return_value;
}
#endif /* ENABLE_VFS_SMB */

/* --------------------------------------------------------------------------------------------- */
