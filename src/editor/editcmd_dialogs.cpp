/*
   Editor dialogs for high level editing commands

   Copyright (C) 2009-2020
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2009
   Andrew Borodin, <aborodin@vmail.ru>, 2012, 2013

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
#include "lib/tty/tty.h"
#include "lib/skin.h"           /* INPUT_COLOR */
#include "lib/tty/key.h"
#include "lib/search.h"
#include "lib/strutil.h"
#include "lib/widget.h"
#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "src/history.h"

#include "src/editor/editwidget.h"
#include "src/editor/etags.h"
#include "src/editor/editcmd_dialogs.h"

#ifdef HAVE_ASPELL
#include "src/editor/spell.h"
#endif

/*** global variables ****************************************************************************/

edit_search_options_t edit_search_options = {
    .type = MC_SEARCH_T_NORMAL,
    .case_sens = false,
    .backwards = false,
    .only_in_selection = false,
    .whole_words = false,
    .all_codepages = false
};

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
editcmd_dialog_raw_key_query_cb (Widget * w, Widget * sender, widget_msg_t msg, int parm,
                                 void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_KEY:
        h->ret_value = parm;
        dlg_stop (h);
        return MSG_HANDLED;
    default:
        return dlg_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

bool
editcmd_dialog_search_show (WEdit * edit)
{
    char *search_text;
    int num_of_types = 0;
    gchar **list_of_types;
    int dialog_result;

    list_of_types = mc_search_get_types_strings_array (&num_of_types);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Enter search string:"), input_label_above, INPUT_LAST_TEXT, 
                                 MC_HISTORY_SHARED_SEARCH, &search_text, nullptr, false, false,
                                 INPUT_COMPLETE_NONE),
            QUICK_SEPARATOR (true),
            QUICK_START_COLUMNS,
                QUICK_RADIO (num_of_types, (const char **) list_of_types,
                             (int *) &edit_search_options.type, nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &edit_search_options.case_sens, nullptr),
                QUICK_CHECKBOX (N_("&Backwards"), &edit_search_options.backwards, nullptr),
                QUICK_CHECKBOX (N_("In se&lection"), &edit_search_options.only_in_selection, nullptr),
                QUICK_CHECKBOX (N_("&Whole words"), &edit_search_options.whole_words, nullptr),
#ifdef HAVE_CHARSET
                QUICK_CHECKBOX (N_("&All charsets"), &edit_search_options.all_codepages, nullptr),
#endif
            QUICK_STOP_COLUMNS,
            QUICK_START_BUTTONS (true, true),
                QUICK_BUTTON (N_("&OK"), B_ENTER, nullptr, nullptr),
                QUICK_BUTTON (N_("&Find all"), B_USER, nullptr, nullptr),
                QUICK_BUTTON (N_("&Cancel"), B_CANCEL, nullptr, nullptr),
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 58,
            N_("Search"), "[Input Line Keys]",
            quick_widgets, nullptr, nullptr
        };

        dialog_result = quick_dialog (&qdlg);
    }

    g_strfreev (list_of_types);

    if ((dialog_result == B_CANCEL) || (search_text == nullptr) || (search_text[0] == '\0'))
    {
        g_free (search_text);
        return false;
    }

    if (dialog_result == B_USER)
        search_create_bookmark = true;

#ifdef HAVE_CHARSET
    {
        GString *tmp;

        tmp = str_convert_to_input (search_text);
        g_free (search_text);
        search_text = g_string_free (tmp, false);
    }
#endif

    g_free (edit->last_search_string);
    edit->last_search_string = search_text;
    mc_search_free (edit->search);

#ifdef HAVE_CHARSET
    edit->search = mc_search_new (edit->last_search_string, cp_source);
#else
    edit->search = mc_search_new (edit->last_search_string, nullptr);
#endif
    if (edit->search != nullptr)
    {
        edit->search->search_type = edit_search_options.type;
#ifdef HAVE_CHARSET
        edit->search->is_all_charsets = edit_search_options.all_codepages;
#endif
        edit->search->is_case_sensitive = edit_search_options.case_sens;
        edit->search->whole_words = edit_search_options.whole_words;
        edit->search->search_fn = edit_search_cmd_callback;
        edit->search->update_fn = edit_search_update_callback;
    }

    return (edit->search != nullptr);
}

/* --------------------------------------------------------------------------------------------- */

void
editcmd_dialog_replace_show (WEdit * edit, const char *search_default, const char *replace_default,
                             /*@out@ */ char **search_text, /*@out@ */ char **replace_text)
{
    int num_of_types = 0;
    gchar **list_of_types;

    if ((search_default == nullptr) || (*search_default == '\0'))
        search_default = INPUT_LAST_TEXT;

    list_of_types = mc_search_get_types_strings_array (&num_of_types);

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABELED_INPUT (N_("Enter search string:"), input_label_above, search_default,
                                 MC_HISTORY_SHARED_SEARCH, search_text, nullptr, false, false,
                                 INPUT_COMPLETE_NONE),
            QUICK_LABELED_INPUT (N_("Enter replacement string:"), input_label_above, replace_default,
                                 "replace", replace_text, nullptr, false, false, INPUT_COMPLETE_NONE),
            QUICK_SEPARATOR (true),
            QUICK_START_COLUMNS,
                QUICK_RADIO (num_of_types, (const char **) list_of_types,
                             (int *) &edit_search_options.type, nullptr),
            QUICK_NEXT_COLUMN,
                QUICK_CHECKBOX (N_("Cas&e sensitive"), &edit_search_options.case_sens, nullptr),
                QUICK_CHECKBOX (N_("&Backwards"), &edit_search_options.backwards, nullptr),
                QUICK_CHECKBOX (N_("In se&lection"), &edit_search_options.only_in_selection, nullptr),
                QUICK_CHECKBOX (N_("&Whole words"), &edit_search_options.whole_words, nullptr),
#ifdef HAVE_CHARSET
                QUICK_CHECKBOX (N_("&All charsets"), &edit_search_options.all_codepages, nullptr),
#endif
            QUICK_STOP_COLUMNS,
            QUICK_BUTTONS_OK_CANCEL,
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            -1, -1, 58,
            N_("Replace"), "[Input Line Keys]",
            quick_widgets, nullptr, nullptr
        };

        if (quick_dialog (&qdlg) != B_CANCEL)
            edit->replace_mode = 0;
        else
        {
            *replace_text = nullptr;
            *search_text = nullptr;
        }
    }

    g_strfreev (list_of_types);
}

/* --------------------------------------------------------------------------------------------- */

int
editcmd_dialog_replace_prompt_show (WEdit * edit, char *from_text, char *to_text, int xpos,
                                    int ypos)
{
    Widget *w = WIDGET (edit);

    /* dialog size */
    int dlg_height = 10;
    int dlg_width;

    char tmp[BUF_MEDIUM];
    char *repl_from, *repl_to;
    int retval;

    if (xpos == -1)
        xpos = w->x + option_line_state_width + 1;
    if (ypos == -1)
        ypos = w->y + w->lines / 2;
    /* Sometimes menu can hide replaced text. I don't like it */
    if ((edit->curs_row >= ypos - 1) && (edit->curs_row <= ypos + dlg_height - 1))
        ypos -= dlg_height;

    dlg_width = WIDGET (w->owner)->cols - xpos - 1;

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", from_text);
    repl_from = g_strdup (str_trunc (tmp, dlg_width - 7));

    g_snprintf (tmp, sizeof (tmp), "\"%s\"", to_text);
    repl_to = g_strdup (str_trunc (tmp, dlg_width - 7));

    {
        quick_widget_t quick_widgets[] = {
            /* *INDENT-OFF* */
            QUICK_LABEL (repl_from, nullptr),
            QUICK_LABEL (N_("Replace with:"), nullptr),
            QUICK_LABEL (repl_to, nullptr),
            QUICK_START_BUTTONS (true, true),
                QUICK_BUTTON (N_("&Replace"), B_ENTER, nullptr, nullptr),
                QUICK_BUTTON (N_("A&ll"), B_REPLACE_ALL, nullptr, nullptr),
                QUICK_BUTTON (N_("&Skip"), B_SKIP_REPLACE, nullptr, nullptr),
                QUICK_BUTTON (N_("&Cancel"), B_CANCEL, nullptr, nullptr),
            QUICK_END
            /* *INDENT-ON* */
        };

        quick_dialog_t qdlg = {
            ypos, xpos, -1,
            N_("Confirm replace"), nullptr,
            quick_widgets, nullptr, nullptr
        };

        retval = quick_dialog (&qdlg);
    }

    g_free (repl_from);
    g_free (repl_to);

    return retval;
}

/* --------------------------------------------------------------------------------------------- */
/* gets a raw key from the keyboard. Passing cancel = 1 draws
   a cancel button thus allowing c-c etc.  Alternatively, cancel = 0
   will return the next key pressed.  ctrl-a (=B_CANCEL), ctrl-g, ctrl-c,
   and Esc are cannot returned */

int
editcmd_dialog_raw_key_query (const char *heading, const char *query, bool cancel)
{
    int w, wq;
    int y = 2;
    WDialog *raw_dlg;
    WGroup *g;

    w = str_term_width1 (heading) + 6;
    wq = str_term_width1 (query);
    w = MAX (w, wq + 3 * 2 + 1 + 2);

    raw_dlg =
        dlg_create (true, 0, 0, cancel ? 7 : 5, w, WPOS_CENTER | WPOS_TRYUP, false, dialog_colors,
                    editcmd_dialog_raw_key_query_cb, nullptr, nullptr, heading);
    g = GROUP (raw_dlg);
    widget_want_tab (WIDGET (raw_dlg), true);

    group_add_widget (g, label_new (y, 3, query));
    group_add_widget (g,
                      input_new (y++, 3 + wq + 1, input_colors, w - (6 + wq + 1), "", 0,
                                 INPUT_COMPLETE_NONE));
    if (cancel)
    {
        group_add_widget (g, hline_new (y++, -1, -1));
        /* Button w/o hotkey to allow use any key as raw or macro one */
        group_add_widget_autopos (g, button_new (y, 1, B_CANCEL, NORMAL_BUTTON, _("Cancel"), nullptr),
                                  WPOS_KEEP_TOP | WPOS_CENTER_HORZ, nullptr);
    }

    w = dlg_run (raw_dlg);
    dlg_destroy (raw_dlg);

    return (cancel && (w == ESC_CHAR || w == B_CANCEL)) ? 0 : w;
}

/* --------------------------------------------------------------------------------------------- */
/* let the user select its preferred completion */

char *
editcmd_dialog_completion_show (const WEdit * edit, int max_len, GString ** compl_, int num_compl)
{
    const Widget *we = CONST_WIDGET (edit);
    int start_x, start_y, offset, i;
    char *curr = nullptr;
    WDialog *compl_dlg;
    WListbox *compl_list;
    int compl_dlg_h;            /* completion dialog height */
    int compl_dlg_w;            /* completion dialog width */

    /* calculate the dialog metrics */
    compl_dlg_h = num_compl + 2;
    compl_dlg_w = max_len + 4;
    start_x = we->x + edit->curs_col + edit->start_col + EDIT_TEXT_HORIZONTAL_OFFSET +
        (edit->fullscreen ? 0 : 1) + option_line_state_width;
    start_y = we->y + edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + (edit->fullscreen ? 0 : 1) + 1;

    if (start_x < 0)
        start_x = 0;
    if (start_x < we->x + 1)
        start_x = we->x + 1 + option_line_state_width;
    if (compl_dlg_w > COLS)
        compl_dlg_w = COLS;
    if (compl_dlg_h > LINES - 2)
        compl_dlg_h = LINES - 2;

    offset = start_x + compl_dlg_w - COLS;
    if (offset > 0)
        start_x -= offset;
    offset = start_y + compl_dlg_h - LINES;
    if (offset > 0)
        start_y -= offset;

    /* create the dialog */
    compl_dlg =
        dlg_create (true, start_y, start_x, compl_dlg_h, compl_dlg_w, WPOS_KEEP_DEFAULT, true,
                    dialog_colors, nullptr, nullptr, "[Completion]", nullptr);

    /* create the listbox */
    compl_list = listbox_new (1, 1, compl_dlg_h - 2, compl_dlg_w - 2, false, nullptr);

    /* add the dialog */
    group_add_widget (GROUP (compl_dlg), compl_list);

    /* fill the listbox with the completions */
    for (i = num_compl - 1; i >= 0; i--)        /* reverse order */
        listbox_add_item (compl_list, LISTBOX_APPEND_AT_END, 0, (char *) compl_[i]->str, nullptr,
                          false);

    /* pop up the dialog and apply the chosen completion */
    if (dlg_run (compl_dlg) == B_ENTER)
    {
        listbox_get_current (compl_list, &curr, nullptr);
        curr = g_strdup (curr);
    }

    /* destroy dialog before return */
    dlg_destroy (compl_dlg);

    return curr;
}

/* --------------------------------------------------------------------------------------------- */
/* let the user select where function definition */

void
editcmd_dialog_select_definition_show (WEdit * edit, char *match_expr, int max_len, int word_len,
                                       etags_hash_t * def_hash, int num_lines)
{
    int start_x, start_y, offset, i;
    char *curr = nullptr;
    WDialog *def_dlg;
    WListbox *def_list;
    int def_dlg_h;              /* dialog height */
    int def_dlg_w;              /* dialog width */

    (void) word_len;
    /* calculate the dialog metrics */
    def_dlg_h = num_lines + 2;
    def_dlg_w = max_len + 4;
    start_x = edit->curs_col + edit->start_col - (def_dlg_w / 2) +
        EDIT_TEXT_HORIZONTAL_OFFSET + (edit->fullscreen ? 0 : 1) + option_line_state_width;
    start_y = edit->curs_row + EDIT_TEXT_VERTICAL_OFFSET + (edit->fullscreen ? 0 : 1) + 1;

    if (start_x < 0)
        start_x = 0;
    if (def_dlg_w > COLS)
        def_dlg_w = COLS;
    if (def_dlg_h > LINES - 2)
        def_dlg_h = LINES - 2;

    offset = start_x + def_dlg_w - COLS;
    if (offset > 0)
        start_x -= offset;
    offset = start_y + def_dlg_h - LINES;
    if (offset > 0)
        start_y -= (offset + 1);

    def_dlg = dlg_create (true, start_y, start_x, def_dlg_h, def_dlg_w, WPOS_KEEP_DEFAULT, true,
                          dialog_colors, nullptr, nullptr, "[Definitions]", match_expr);
    def_list = listbox_new (1, 1, def_dlg_h - 2, def_dlg_w - 2, false, nullptr);
    group_add_widget (GROUP (def_dlg), def_list);

    /* fill the listbox with the completions */
    for (i = 0; i < num_lines; i++)
    {
        char *label_def;

        label_def =
            g_strdup_printf ("%s -> %s:%ld", def_hash[i].short_define, def_hash[i].filename,
                             def_hash[i].line);
        listbox_add_item (def_list, LISTBOX_APPEND_AT_END, 0, label_def, &def_hash[i], false);
        g_free (label_def);
    }

    /* pop up the dialog and apply the chosen completion */
    if (dlg_run (def_dlg) == B_ENTER)
    {
        etags_hash_t *curr_def = nullptr;
        bool do_moveto = false;

        listbox_get_current (def_list, &curr, (void **) &curr_def);

        if (!edit->modified)
            do_moveto = true;
        else if (!edit_query_dialog2
                 (_("Warning"),
                  _("Current text was modified without a file save.\n"
                    "Continue discards these changes."), _("C&ontinue"), _("&Cancel")))
        {
            edit->force |= REDRAW_COMPLETELY;
            do_moveto = true;
        }

        if (curr != nullptr && do_moveto && edit_stack_iterator + 1 < MAX_HISTORY_MOVETO)
        {
            vfs_path_free (edit_history_moveto[edit_stack_iterator].filename_vpath);

            if (edit->dir_vpath != nullptr)
                edit_history_moveto[edit_stack_iterator].filename_vpath =
                    vfs_path_append_vpath_new (edit->dir_vpath, edit->filename_vpath, nullptr);
            else
                edit_history_moveto[edit_stack_iterator].filename_vpath =
                    vfs_path_clone (edit->filename_vpath);

            edit_history_moveto[edit_stack_iterator].line = edit->start_line + edit->curs_row + 1;
            edit_stack_iterator++;
            vfs_path_free (edit_history_moveto[edit_stack_iterator].filename_vpath);
            edit_history_moveto[edit_stack_iterator].filename_vpath =
                vfs_path_from_str ((char *) curr_def->fullpath);
            edit_history_moveto[edit_stack_iterator].line = curr_def->line;
            edit_reload_line (edit, edit_history_moveto[edit_stack_iterator].filename_vpath,
                              edit_history_moveto[edit_stack_iterator].line);
        }
    }

    /* clear definition hash */
    for (i = 0; i < MAX_DEFINITIONS; i++)
        g_free (def_hash[i].filename);

    /* destroy dialog before return */
    dlg_destroy (def_dlg);
}

/* --------------------------------------------------------------------------------------------- */
