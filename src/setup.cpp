/*
   Setup loading/saving.

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

/** \file setup.c
 *  \brief Source: setup loading/saving
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/mcconfig.h"       /* num_history_items_recorded */
#include "lib/fileloc.h"
#include "lib/timefmt.h"
#include "lib/util.h"
#include "lib/widget.h"

#ifdef ENABLE_VFS_FTP
#include "src/vfs/ftpfs/ftpfs.h"
#endif
#ifdef ENABLE_VFS_FISH
#include "src/vfs/fish/fish.h"
#endif

#ifdef HAVE_CHARSET
#include "lib/charsets.h"
#endif

#include "filemanager/dir.h"
#include "filemanager/midnight.h"
#include "filemanager/tree.h"   /* xtree_mode */
#include "filemanager/hotlist.h"        /* load/save/done hotlist */
#include "filemanager/panelize.h"       /* load/save/done panelize */
#include "filemanager/layout.h"
#include "filemanager/cmd.h"

#include "args.h"
#include "execute.h"            /* pause_after_run */
#include "clipboard.h"
#include "keybind-defaults.h"   /* keybind_lookup_action */

#ifdef HAVE_CHARSET
#include "selcodepage.h"
#endif

#ifdef USE_INTERNAL_EDIT
#include "src/editor/edit.h"
#endif

#include "src/viewer/mcviewer.h"        /* For the externs */

#include "setup.h"

/*** global variables ****************************************************************************/

char *global_profile_name;      /* mc.lib */

/* Only used at program boot */
bool boot_current_is_left = true;

/* If on, default for "No" in delete operations */
bool safe_delete = false;
/* If on, default for "No" in overwrite files */
bool safe_overwrite = false;

/* Controls screen clearing before an exec */
bool clear_before_exec = true;

/* Asks for confirmation before deleting a file */
bool confirm_delete = true;
/* Asks for confirmation before deleting a hotlist entry */
bool confirm_directory_hotlist_delete = false;
/* Asks for confirmation before overwriting a file */
bool confirm_overwrite = true;
/* Asks for confirmation before executing a program by pressing enter */
bool confirm_execute = false;
/* Asks for confirmation before leaving the program */
bool confirm_exit = false;

/* If true, at startup the user-menu is invoked */
bool auto_menu = false;
/* This flag indicates if the pull down menus by default drop down */
bool drop_menus = false;

/* Asks for confirmation when using F3 to view a directory and there
   are tagged files */
bool confirm_view_dir = false;

/* Ask file name before start the editor */
bool editor_ask_filename_before_edit = false;

panel_view_mode_t startup_left_mode;
panel_view_mode_t startup_right_mode;

bool copymove_persistent_attr = true;

/* Tab size */
int option_tab_spacing = DEFAULT_TAB_SPACING;

/* Ugly hack to allow panel_save_setup to work as a place holder for */
/* default panel values */
int saving_setup;

panels_options_t panels_options = {
    .show_mini_info = true,
    .kilobyte_si = false,
    .mix_all_files = false,
    .show_backups = true,
    .show_dot_files = true,
    .fast_reload = false,
    .fast_reload_msg_shown = false,
    .mark_moves_down = true,
    .reverse_files_only = true,
    .auto_save_setup = false,
    .navigate_with_arrows = false,
    .scroll_pages = true,
    .scroll_center = false,
    .mouse_move_pages = true,
    .filetype_mode = true,
    .permission_mode = false,
    .qsearch_mode = QSEARCH_PANEL_CASE,
    .torben_fj_mode = false,
    .select_flags = SELECT_MATCH_CASE | SELECT_SHELL_PATTERNS
};

bool easy_patterns = true;

/* It true saves the setup when quitting */
bool auto_save_setup = true;

/* If true, then the +, - and \ keys have their special meaning only if the
 * command line is empty, otherwise they behave like regular letters
 */
bool only_leading_plus_minus = true;

/* Automatically fills name with current selected item name on mkdir */
bool auto_fill_mkdir_name = true;

/* If set and you don't have subshell support, then C-o will give you a shell */
bool output_starts_shell = false;

/* If set, we execute the file command to check the file type */
bool use_file_to_check_type = true;

bool verbose = true;

/*
 * Whether the Midnight Commander tries to provide more
 * information about copy/move sizes and bytes transferred
 * at the expense of some speed
 */
bool file_op_compute_totals = true;

/* If true use the internal viewer */
bool use_internal_view = true;
/* If set, use the builtin editor */
bool use_internal_edit = true;

#ifdef HAVE_CHARSET
/* Numbers of (file I/O) and (input/display) codepages. -1 if not selected */
int default_source_codepage = -1;
char *autodetect_codeset = nullptr;
bool is_autodetect_codeset_enabled = false;
#endif /* !HAVE_CHARSET */

#ifdef HAVE_ASPELL
char *spell_language = nullptr;
#endif

/* Value of "other_dir" key in ini file */
char *saved_other_dir = nullptr;

/* If set, then print to the given file the last directory we were at */
char *last_wd_string = nullptr;

/* Set when main loop should be terminated */
int quit = 0;

/* Set to true to suppress printing the last directory */
bool print_last_revert = false;

#ifdef USE_INTERNAL_EDIT
/* index to record_macro_buf[], -1 if not recording a macro */
int macro_index = -1;

/* macro stuff */
struct macro_action_t record_macro_buf[MAX_MACRO_LENGTH];

GArray *macros_list;
#endif /* USE_INTERNAL_EDIT */

/*** file scope macro definitions ****************************************************************/

/* In order to use everywhere the same setup for the locale we use defines */
#define FMTYEAR _("%b %e  %Y")
#define FMTTIME _("%b %e %H:%M")

/*** file scope type declarations ****************************************************************/

/*** file scope variables ************************************************************************/

static char *profile_name = nullptr;       /* ${XDG_CONFIG_HOME}/mc/ini */
static char *panels_profile_name = nullptr;        /* ${XDG_CACHE_HOME}/mc/panels.ini */

/* *INDENT-OFF* */
static const struct
{
    const char *key;
    int  list_format;
} list_formats [] = {
    { "full",  list_full  },
    { "brief", list_brief },
    { "long",  list_long  },
    { "user",  list_user  },
    { nullptr, 0 }
};

static const struct
{
    const char *opt_name;
    panel_view_mode_t opt_type;
} panel_types [] = {
    { "listing",   view_listing },
    { "quickview", view_quick },
    { "info",      view_info },
    { "tree",      view_tree },
    { nullptr,        view_listing }
};

static const struct
{
    const char *opt_name;
    int *opt_addr;
} layout_int_options [] = {
    { "output_lines", &output_lines },
    { "left_panel_size", &panels_layout.left_panel_size },
    { "top_panel_size", &panels_layout.top_panel_size },
    { nullptr, nullptr }
};

static const struct
{
    const char *opt_name;
    bool *opt_addr;
} layout_bool_options [] = {
    { "message_visible", &mc_global.message_visible },
    { "keybar_visible", &mc_global.keybar_visible },
    { "xterm_title", &xterm_title },
    { "command_prompt", &command_prompt },
    { "menubar_visible", &menubar_visible },
    { "free_space", &free_space },
    { "horizontal_split", &panels_layout.horizontal_split },
    { "vertical_equal", &panels_layout.vertical_equal },
    { "horizontal_equal", &panels_layout.horizontal_equal },
    { nullptr, nullptr }
};

static const struct
{
    const char *opt_name;
    bool *opt_addr;
} bool_options [] = {
    { "verbose", &verbose },
    { "shell_patterns", &easy_patterns },
    { "auto_save_setup", &auto_save_setup },
    { "preallocate_space", &mc_global.vfs.preallocate_space },
    { "auto_menu", &auto_menu },
    { "use_internal_view", &use_internal_view },
    { "use_internal_edit", &use_internal_edit },
    { "clear_before_exec", &clear_before_exec },
    { "confirm_delete", &confirm_delete },
    { "confirm_overwrite", &confirm_overwrite },
    { "confirm_execute", &confirm_execute },
    { "confirm_history_cleanup", &mc_global.widget.confirm_history_cleanup },
    { "confirm_exit", &confirm_exit },
    { "confirm_directory_hotlist_delete", &confirm_directory_hotlist_delete },
    { "confirm_view_dir", &confirm_view_dir },
    { "safe_delete", &safe_delete },
    { "safe_overwrite", &safe_overwrite },
#ifndef HAVE_CHARSET
    { "eight_bit_clean", &mc_global.eight_bit_clean },
    { "full_eight_bits", &mc_global.full_eight_bits },
#endif /* !HAVE_CHARSET */
    { "use_8th_bit_as_meta", &use_8th_bit_as_meta },
    { "mouse_move_pages_viewer", &mcview_mouse_move_pages },
    { "mouse_close_dialog", &mouse_close_dialog},
    { "fast_refresh", &fast_refresh },
    { "drop_menus", &drop_menus },
    { "wrap_mode",  &mcview_global_flags.wrap },
    { "old_esc_mode", &old_esc_mode },
    { "cd_symlinks", &mc_global.vfs.cd_symlinks },
    { "show_all_if_ambiguous", &mc_global.widget.show_all_if_ambiguous },
    { "use_file_to_guess_type", &use_file_to_check_type },
    { "alternate_plus_minus", &mc_global.tty.alternate_plus_minus },
    { "only_leading_plus_minus", &only_leading_plus_minus },
    { "show_output_starts_shell", &output_starts_shell },
    { "xtree_mode", &xtree_mode },
    { "file_op_compute_totals", &file_op_compute_totals },
    { "classic_progressbar", &classic_progressbar },
#ifdef ENABLE_VFS
#ifdef ENABLE_VFS_FTP
    { "use_netrc", &ftpfs_use_netrc },
    { "ftpfs_always_use_proxy", &ftpfs_always_use_proxy },
    { "ftpfs_use_passive_connections", &ftpfs_use_passive_connections },
    { "ftpfs_use_passive_connections_over_proxy", &ftpfs_use_passive_connections_over_proxy },
    { "ftpfs_use_unix_list_options", &ftpfs_use_unix_list_options },
    { "ftpfs_first_cd_then_ls", &ftpfs_first_cd_then_ls },
    { "ignore_ftp_chattr_errors", & ftpfs_ignore_chattr_errors} ,
#endif /* ENABLE_VFS_FTP */
#endif /* ENABLE_VFS */
#ifdef USE_INTERNAL_EDIT
    { "editor_fill_tabs_with_spaces", &option_fill_tabs_with_spaces },
    { "editor_return_does_auto_indent", &option_return_does_auto_indent },
    { "editor_backspace_through_tabs", &option_backspace_through_tabs },
    { "editor_fake_half_tabs", &option_fake_half_tabs },
    { "editor_option_save_position", &option_save_position },
    { "editor_option_auto_para_formatting", &option_auto_para_formatting },
    { "editor_option_typewriter_wrap", &option_typewriter_wrap },
    { "editor_edit_confirm_save", &edit_confirm_save },
    { "editor_syntax_highlighting", &option_syntax_highlighting },
    { "editor_persistent_selections", &option_persistent_selections },
    { "editor_drop_selection_on_copy", &option_drop_selection_on_copy },
    { "editor_cursor_beyond_eol", &option_cursor_beyond_eol },
    { "editor_cursor_after_inserted_block", &option_cursor_after_inserted_block },
    { "editor_visible_tabs", &visible_tabs },
    { "editor_visible_spaces", &visible_tws },
    { "editor_line_state", &option_line_state },
    { "editor_simple_statusbar", &simple_statusbar },
    { "editor_check_new_line", &option_check_nl_at_eof },
    { "editor_show_right_margin", &show_right_margin },
    { "editor_group_undo", &option_group_undo },
    { "editor_state_full_filename", &option_state_full_filename },
#endif /* USE_INTERNAL_EDIT */
    { "editor_ask_filename_before_edit", &editor_ask_filename_before_edit },
    { "nice_rotating_dash", &nice_rotating_dash },
    { "shadows", &mc_global.tty.shadows },
    { "mcview_remember_file_position", &mcview_remember_file_position },
    { "auto_fill_mkdir_name", &auto_fill_mkdir_name },
    { "copymove_persistent_attr", &copymove_persistent_attr },
    { nullptr, nullptr }
};

static const struct
{
    const char *opt_name;
    int *opt_addr;
} int_options [] = {
    { "pause_after_run", &pause_after_run },
    { "mouse_repeat_rate", &mou_auto_repeat },
    { "double_click_speed", &double_click_speed },
    { "old_esc_mode_timeout", &old_esc_mode_timeout },
    { "max_dirt_limit", &mcview_max_dirt_limit },
    { "num_history_items_recorded", &num_history_items_recorded },
#ifdef ENABLE_VFS
    { "vfs_timeout", &vfs_timeout },
#ifdef ENABLE_VFS_FTP
    { "ftpfs_directory_timeout", &ftpfs_directory_timeout },
    { "ftpfs_retry_seconds", &ftpfs_retry_seconds },
#endif /* ENABLE_VFS_FTP */
#ifdef ENABLE_VFS_FISH
    { "fish_directory_timeout", &fish_directory_timeout },
#endif /* ENABLE_VFS_FISH */
#endif /* ENABLE_VFS */
    /* option_tab_spacing is used in internal viewer */
    { "editor_tab_spacing", &option_tab_spacing },
#ifdef USE_INTERNAL_EDIT
    { "editor_word_wrap_line_length", &option_word_wrap_line_length },
    { "editor_option_save_mode", &option_save_mode },
#endif /* USE_INTERNAL_EDIT */
    { nullptr, nullptr }
};

static const struct
{
    const char *opt_name;
    char **opt_addr;
    const char *opt_defval;
} str_options[] = {
#ifdef USE_INTERNAL_EDIT
    { "editor_backup_extension", &option_backup_ext, "~" },
    { "editor_filesize_threshold", &option_filesize_threshold, "64M" },
    { "editor_stop_format_chars", &option_stop_format_chars, "-+*\\,.;:&>" },
#endif
    { "mcview_eof", &mcview_show_eof, "" },
    {  nullptr, nullptr, nullptr }
};

static const struct
{
    const char *opt_name;
    bool *opt_addr;
} panels_ini_options[] = {
    { "show_mini_info", &panels_options.show_mini_info },
    { "kilobyte_si", &panels_options.kilobyte_si },
    { "mix_all_files", &panels_options.mix_all_files },
    { "show_backups", &panels_options.show_backups },
    { "show_dot_files", &panels_options.show_dot_files },
    { "fast_reload", &panels_options.fast_reload },
    { "fast_reload_msg_shown", &panels_options.fast_reload_msg_shown },
    { "mark_moves_down", &panels_options.mark_moves_down },
    { "reverse_files_only", &panels_options.reverse_files_only },
    { "auto_save_setup_panels", &panels_options.auto_save_setup },
    { "navigate_with_arrows", &panels_options.navigate_with_arrows },
    { "panel_scroll_pages", &panels_options.scroll_pages },
    { "panel_scroll_center", &panels_options.scroll_center },
    { "mouse_move_pages",  &panels_options.mouse_move_pages },
    { "filetype_mode", &panels_options.filetype_mode },
    { "permission_mode", &panels_options.permission_mode },
    { "torben_fj_mode", &panels_options.torben_fj_mode },
    { nullptr, nullptr }
};
/* *INDENT-ON* */

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/**
 * Get name of config file.
 *
 * @param subdir If not nullptr, config is also searched in specified subdir.
 * @param config_file_name If relative, file if searched in standard paths.
 *
 * @return newly allocated string with config name or nullptr if file is not found.
 */

static char *
load_setup_get_full_config_name (const char *subdir, const char *config_file_name)
{
    /*
       TODO: IMHO, in future, this function shall be placed in mcconfig module.
     */
    char *lc_basename, *ret;
    char *file_name;

    if (config_file_name == nullptr)
        return nullptr;

    /* check for .keymap suffix */
    if (g_str_has_suffix (config_file_name, ".keymap"))
        file_name = g_strdup (config_file_name);
    else
        file_name = g_strconcat (config_file_name, ".keymap", (char *) nullptr);

    canonicalize_pathname (file_name);

    if (g_path_is_absolute (file_name))
        return file_name;

    lc_basename = g_path_get_basename (file_name);
    g_free (file_name);

    if (lc_basename == nullptr)
        return nullptr;

    if (subdir != nullptr)
        ret = g_build_filename (mc_config_get_path (), subdir, lc_basename, (char *) nullptr);
    else
        ret = g_build_filename (mc_config_get_path (), lc_basename, (char *) nullptr);

    if (exist_file (ret))
    {
        g_free (lc_basename);
        canonicalize_pathname (ret);
        return ret;
    }
    g_free (ret);

    if (subdir != nullptr)
        ret = g_build_filename (mc_global.sysconfig_dir, subdir, lc_basename, (char *) nullptr);
    else
        ret = g_build_filename (mc_global.sysconfig_dir, lc_basename, (char *) nullptr);

    if (exist_file (ret))
    {
        g_free (lc_basename);
        canonicalize_pathname (ret);
        return ret;
    }
    g_free (ret);

    if (subdir != nullptr)
        ret = g_build_filename (mc_global.share_data_dir, subdir, lc_basename, (char *) nullptr);
    else
        ret = g_build_filename (mc_global.share_data_dir, lc_basename, (char *) nullptr);

    g_free (lc_basename);

    if (exist_file (ret))
    {
        canonicalize_pathname (ret);
        return ret;
    }

    g_free (ret);
    return nullptr;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
setup__is_cfg_group_must_panel_config (const char *grp)
{
    return (!strcasecmp ("Dirs", grp) ||
            !strcasecmp ("Temporal:New Right Panel", grp) ||
            !strcasecmp ("Temporal:New Left Panel", grp) ||
            !strcasecmp ("New Left Panel", grp) || !strcasecmp ("New Right Panel", grp))
        ? grp : nullptr;
}

/* --------------------------------------------------------------------------------------------- */

static void
setup__move_panels_config_into_separate_file (const char *profile)
{
    mc_config_t *tmp_cfg;
    char **groups, **curr_grp;

    if (!exist_file (profile))
        return;

    tmp_cfg = mc_config_init (profile, false);
    if (tmp_cfg == nullptr)
        return;

    groups = mc_config_get_groups (tmp_cfg, nullptr);
    if (*groups == nullptr)
    {
        g_strfreev (groups);
        mc_config_deinit (tmp_cfg);
        return;
    }

    for (curr_grp = groups; *curr_grp != nullptr; curr_grp++)
        if (setup__is_cfg_group_must_panel_config (*curr_grp) == nullptr)
            mc_config_del_group (tmp_cfg, *curr_grp);

    mc_config_save_to_file (tmp_cfg, panels_profile_name, nullptr);
    mc_config_deinit (tmp_cfg);

    tmp_cfg = mc_config_init (profile, false);
    if (tmp_cfg == nullptr)
    {
        g_strfreev (groups);
        return;
    }

    for (curr_grp = groups; *curr_grp != nullptr; curr_grp++)
    {
        const char *need_grp;

        need_grp = setup__is_cfg_group_must_panel_config (*curr_grp);
        if (need_grp != nullptr)
            mc_config_del_group (tmp_cfg, need_grp);
    }
    g_strfreev (groups);

    mc_config_save_file (tmp_cfg, nullptr);
    mc_config_deinit (tmp_cfg);
}

/* --------------------------------------------------------------------------------------------- */
/**
  Create new mc_config object from specified ini-file or
  append data to existing mc_config object from ini-file
*/

static void
load_setup_init_config_from_file (mc_config_t ** config, const char *fname, bool read_only)
{
    /*
       TODO: IMHO, in future, this function shall be placed in mcconfig module.
     */
    if (exist_file (fname))
    {
        if (*config != nullptr)
            mc_config_read_file (*config, fname, read_only, true);
        else
            *config = mc_config_init (fname, read_only);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
load_config (void)
{
    size_t i;
    const char *kt;

    /* Load boolean options */
    for (i = 0; bool_options[i].opt_name != nullptr; i++)
        *bool_options[i].opt_addr =
            mc_config_get_bool (mc_global.main_config, CONFIG_APP_SECTION, bool_options[i].opt_name,
                                *bool_options[i].opt_addr);

    /* Load integer options */
    for (i = 0; int_options[i].opt_name != nullptr; i++)
        *int_options[i].opt_addr =
            mc_config_get_int (mc_global.main_config, CONFIG_APP_SECTION, int_options[i].opt_name,
                               *int_options[i].opt_addr);

    /* Load string options */
    for (i = 0; str_options[i].opt_name != nullptr; i++)
        *str_options[i].opt_addr =
            mc_config_get_string (mc_global.main_config, CONFIG_APP_SECTION,
                                  str_options[i].opt_name, str_options[i].opt_defval);

    /* Overwrite some options */
#ifdef USE_INTERNAL_EDIT
    if (option_word_wrap_line_length <= 0)
        option_word_wrap_line_length = DEFAULT_WRAP_LINE_LENGTH;
#else
    /* Reset forced in case of build without internal editor */
    use_internal_edit = false;
#endif /* USE_INTERNAL_EDIT */

    if (option_tab_spacing <= 0)
        option_tab_spacing = DEFAULT_TAB_SPACING;

    kt = getenv ("KEYBOARD_KEY_TIMEOUT_US");
    if (kt != nullptr && kt[0] != '\0')
        old_esc_mode_timeout = atoi (kt);
}

/* --------------------------------------------------------------------------------------------- */

static panel_view_mode_t
setup__load_panel_state (const char *section)
{
    char *buffer;
    size_t i;
    panel_view_mode_t mode = view_listing;

    /* Load the display mode */
    buffer = mc_config_get_string (mc_global.panels_config, section, "display", "listing");

    for (i = 0; panel_types[i].opt_name != nullptr; i++)
        if (g_ascii_strcasecmp (panel_types[i].opt_name, buffer) == 0)
        {
            mode = panel_types[i].opt_type;
            break;
        }

    g_free (buffer);

    return mode;
}

/* --------------------------------------------------------------------------------------------- */

static void
load_layout (void)
{
    size_t i;

    /* actual options override legacy ones */
    for (i = 0; layout_int_options[i].opt_name != nullptr; i++)
        *layout_int_options[i].opt_addr =
            mc_config_get_int (mc_global.main_config, CONFIG_LAYOUT_SECTION,
                               layout_int_options[i].opt_name, *layout_int_options[i].opt_addr);

    for (i = 0; layout_bool_options[i].opt_name != nullptr; i++)
        *layout_bool_options[i].opt_addr =
            mc_config_get_bool (mc_global.main_config, CONFIG_LAYOUT_SECTION,
                                layout_bool_options[i].opt_name, *layout_bool_options[i].opt_addr);

    startup_left_mode = setup__load_panel_state ("New Left Panel");
    startup_right_mode = setup__load_panel_state ("New Right Panel");

    /* At least one of the panels is a listing panel */
    if (startup_left_mode != view_listing && startup_right_mode != view_listing)
        startup_left_mode = view_listing;

    boot_current_is_left =
        mc_config_get_bool (mc_global.panels_config, "Dirs", "current_is_left", true);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_keys_from_section (const char *terminal, mc_config_t * cfg)
{
    char *section_name;
    gchar **profile_keys, **keys;
    char *valcopy, *value;
    long key_code;

    if (terminal == nullptr)
        return;

    section_name = g_strconcat ("terminal:", terminal, (char *) nullptr);
    keys = mc_config_get_keys (cfg, section_name, nullptr);

    for (profile_keys = keys; *profile_keys != nullptr; profile_keys++)
    {
        /* copy=other causes all keys from [terminal:other] to be loaded. */
        if (g_ascii_strcasecmp (*profile_keys, "copy") == 0)
        {
            valcopy = mc_config_get_string (cfg, section_name, *profile_keys, "");
            load_keys_from_section (valcopy, cfg);
            g_free (valcopy);
            continue;
        }

        key_code = lookup_key (*profile_keys, nullptr);
        if (key_code != 0)
        {
            gchar **values;

            values = mc_config_get_string_list (cfg, section_name, *profile_keys, nullptr);
            if (values != nullptr)
            {
                gchar **curr_values;

                for (curr_values = values; *curr_values != nullptr; curr_values++)
                {
                    valcopy = convert_controls (*curr_values);
                    define_sequence (key_code, valcopy, MCKEY_NOACTION);
                    g_free (valcopy);
                }

                g_strfreev (values);
            }
            else
            {
                value = mc_config_get_string (cfg, section_name, *profile_keys, "");
                valcopy = convert_controls (value);
                define_sequence (key_code, valcopy, MCKEY_NOACTION);
                g_free (valcopy);
                g_free (value);
            }
        }
    }
    g_strfreev (keys);
    g_free (section_name);
}

/* --------------------------------------------------------------------------------------------- */

static void
load_keymap_from_section (const char *section_name, GArray * keymap, mc_config_t * cfg)
{
    gchar **profile_keys, **keys;

    if (section_name == nullptr)
        return;

    keys = mc_config_get_keys (cfg, section_name, nullptr);

    for (profile_keys = keys; *profile_keys != nullptr; profile_keys++)
    {
        gchar **values;

        values = mc_config_get_string_list (cfg, section_name, *profile_keys, nullptr);
        if (values != nullptr)
        {
            long action;

            action = keybind_lookup_action (*profile_keys);
            if (action > 0)
            {
                gchar **curr_values;

                for (curr_values = values; *curr_values != nullptr; curr_values++)
                    keybind_cmd_bind (keymap, *curr_values, action);
            }

            g_strfreev (values);
        }
    }

    g_strfreev (keys);
}

/* --------------------------------------------------------------------------------------------- */

static mc_config_t *
load_setup_get_keymap_profile_config (bool load_from_file)
{
    /*
       TODO: IMHO, in future, this function shall be placed in mcconfig module.
     */
    mc_config_t *keymap_config;
    char *share_keymap, *sysconfig_keymap;
    char *fname, *fname2;

    /* 0) Create default keymap */
    keymap_config = create_default_keymap ();
    if (!load_from_file)
        return keymap_config;

    /* load and merge global keymaps */

    /* 1) /usr/share/mc (mc_global.share_data_dir) */
    share_keymap = g_build_filename (mc_global.share_data_dir, GLOBAL_KEYMAP_FILE, (char *) nullptr);
    load_setup_init_config_from_file (&keymap_config, share_keymap, true);

    /* 2) /etc/mc (mc_global.sysconfig_dir) */
    sysconfig_keymap =
        g_build_filename (mc_global.sysconfig_dir, GLOBAL_KEYMAP_FILE, (char *) nullptr);
    load_setup_init_config_from_file (&keymap_config, sysconfig_keymap, true);

    /* then load and merge one of user-defined keymap */

    /* 3) --keymap=<keymap> */
    fname = load_setup_get_full_config_name (nullptr, mc_args__keymap_file);
    if (fname != nullptr && strcmp (fname, sysconfig_keymap) != 0 && strcmp (fname, share_keymap) != 0)
    {
        load_setup_init_config_from_file (&keymap_config, fname, true);
        goto done;
    }
    g_free (fname);

    /* 4) getenv("MC_KEYMAP") */
    fname = load_setup_get_full_config_name (nullptr, g_getenv ("MC_KEYMAP"));
    if (fname != nullptr && strcmp (fname, sysconfig_keymap) != 0 && strcmp (fname, share_keymap) != 0)
    {
        load_setup_init_config_from_file (&keymap_config, fname, true);
        goto done;
    }

    MC_PTR_FREE (fname);

    /* 5) main config; [Midnight Commander] -> keymap */
    fname2 = mc_config_get_string (mc_global.main_config, CONFIG_APP_SECTION, "keymap", nullptr);
    if (fname2 != nullptr && *fname2 != '\0')
        fname = load_setup_get_full_config_name (nullptr, fname2);
    g_free (fname2);
    if (fname != nullptr && strcmp (fname, sysconfig_keymap) != 0 && strcmp (fname, share_keymap) != 0)
    {
        load_setup_init_config_from_file (&keymap_config, fname, true);
        goto done;
    }
    g_free (fname);

    /* 6) ${XDG_CONFIG_HOME}/mc/mc.keymap */
    fname = mc_config_get_full_path (GLOBAL_KEYMAP_FILE);
    load_setup_init_config_from_file (&keymap_config, fname, true);

  done:
    g_free (fname);
    g_free (sysconfig_keymap);
    g_free (share_keymap);

    return keymap_config;
}

/* --------------------------------------------------------------------------------------------- */

static void
panel_save_type (const char *section, panel_view_mode_t type)
{
    size_t i;

    for (i = 0; panel_types[i].opt_name != nullptr; i++)
        if (panel_types[i].opt_type == type)
        {
            mc_config_set_string (mc_global.panels_config, section, "display",
                                  panel_types[i].opt_name);
            break;
        }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Load panels options from [Panels] section.
 */
static void
panels_load_options (void)
{
    if (mc_config_has_group (mc_global.main_config, CONFIG_PANELS_SECTION))
    {
        size_t i;
        int qmode;

        for (i = 0; panels_ini_options[i].opt_name != nullptr; i++)
            *panels_ini_options[i].opt_addr =
                mc_config_get_bool (mc_global.main_config, CONFIG_PANELS_SECTION,
                                    panels_ini_options[i].opt_name,
                                    *panels_ini_options[i].opt_addr);

        qmode = mc_config_get_int (mc_global.main_config, CONFIG_PANELS_SECTION,
                                   "quick_search_mode", (int) panels_options.qsearch_mode);
        if (qmode < 0)
            panels_options.qsearch_mode = QSEARCH_CASE_INSENSITIVE;
        else if (qmode >= QSEARCH_NUM)
            panels_options.qsearch_mode = QSEARCH_PANEL_CASE;
        else
            panels_options.qsearch_mode = (qsearch_mode_t) qmode;

        panels_options.select_flags =
            mc_config_get_int (mc_global.main_config, CONFIG_PANELS_SECTION, "select_flags",
                               (int) panels_options.select_flags);
    }
}

/* --------------------------------------------------------------------------------------------- */

/**
 * Save panels options in [Panels] section.
 */
static void
panels_save_options (void)
{
    size_t i;

    for (i = 0; panels_ini_options[i].opt_name != nullptr; i++)
        mc_config_set_bool (mc_global.main_config, CONFIG_PANELS_SECTION,
                            panels_ini_options[i].opt_name, *panels_ini_options[i].opt_addr);

    mc_config_set_int (mc_global.main_config, CONFIG_PANELS_SECTION,
                       "quick_search_mode", (int) panels_options.qsearch_mode);
    mc_config_set_int (mc_global.main_config, CONFIG_PANELS_SECTION,
                       "select_flags", (int) panels_options.select_flags);
}

/* --------------------------------------------------------------------------------------------- */

static void
save_config (void)
{
    size_t i;

    /* Save boolean options */
    for (i = 0; bool_options[i].opt_name != nullptr; i++)
        mc_config_set_bool (mc_global.main_config, CONFIG_APP_SECTION, bool_options[i].opt_name,
                            *bool_options[i].opt_addr);

    /* Save integer options */
    for (i = 0; int_options[i].opt_name != nullptr; i++)
        mc_config_set_int (mc_global.main_config, CONFIG_APP_SECTION, int_options[i].opt_name,
                           *int_options[i].opt_addr);

    /* Save string options */
    for (i = 0; str_options[i].opt_name != nullptr; i++)
        mc_config_set_string (mc_global.main_config, CONFIG_APP_SECTION, str_options[i].opt_name,
                              *str_options[i].opt_addr);
}

/* --------------------------------------------------------------------------------------------- */

static void
save_layout (void)
{
    size_t i;

    /* Save integer options */
    for (i = 0; layout_int_options[i].opt_name != nullptr; i++)
        mc_config_set_int (mc_global.main_config, CONFIG_LAYOUT_SECTION,
                           layout_int_options[i].opt_name, *layout_int_options[i].opt_addr);

    /* Save boolean options */
    for (i = 0; layout_bool_options[i].opt_name != nullptr; i++)
        mc_config_set_bool (mc_global.main_config, CONFIG_LAYOUT_SECTION,
                            layout_bool_options[i].opt_name, *layout_bool_options[i].opt_addr);
}

/* --------------------------------------------------------------------------------------------- */

/* save panels.ini */
static void
save_panel_types (void)
{
    panel_view_mode_t type;

    if (mc_global.mc_run_mode != MC_RUN_FULL)
        return;

    type = get_panel_type (0);
    panel_save_type ("New Left Panel", type);
    if (type == view_listing)
        panel_save_setup (left_panel, left_panel->panel_name);
    type = get_panel_type (1);
    panel_save_type ("New Right Panel", type);
    if (type == view_listing)
        panel_save_setup (right_panel, right_panel->panel_name);

    {
        char *dirs;

        dirs = get_panel_dir_for (other_panel);
        mc_config_set_string (mc_global.panels_config, "Dirs", "other_dir", dirs);
        g_free (dirs);
    }

    if (current_panel != nullptr)
        mc_config_set_bool (mc_global.panels_config, "Dirs", "current_is_left",
                            get_current_index () == 0);

    if (mc_global.panels_config->ini_path == nullptr)
        mc_global.panels_config->ini_path = g_strdup (panels_profile_name);

    mc_config_del_group (mc_global.panels_config, "Temporal:New Left Panel");
    mc_config_del_group (mc_global.panels_config, "Temporal:New Right Panel");

    mc_config_save_file (mc_global.panels_config, nullptr);
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

const char *
setup_init (void)
{
    if (profile_name == nullptr)
    {
        char *profile;

        profile = mc_config_get_full_path (MC_CONFIG_FILE);
        if (!exist_file (profile))
        {
            char *inifile;

            inifile = mc_build_filename (mc_global.sysconfig_dir, "mc.ini", (char *) nullptr);
            if (exist_file (inifile))
            {
                g_free (profile);
                profile = inifile;
            }
            else
            {
                g_free (inifile);
                inifile = mc_build_filename (mc_global.share_data_dir, "mc.ini", (char *) nullptr);
                if (!exist_file (inifile))
                    g_free (inifile);
                else
                {
                    g_free (profile);
                    profile = inifile;
                }
            }
        }

        profile_name = profile;
    }

    return profile_name;
}

/* --------------------------------------------------------------------------------------------- */

void
load_setup (void)
{
    const char *profile;

#ifdef HAVE_CHARSET
    const char *cbuffer;

    load_codepages_list ();
#endif /* HAVE_CHARSET */

    profile = setup_init ();

    /* mc.lib is common for all users, but has priority lower than
       ${XDG_CONFIG_HOME}/mc/ini.  FIXME: it's only used for keys and treestore now */
    global_profile_name =
        g_build_filename (mc_global.sysconfig_dir, MC_GLOBAL_CONFIG_FILE, (char *) nullptr);
    if (!exist_file (global_profile_name))
    {
        g_free (global_profile_name);
        global_profile_name =
            g_build_filename (mc_global.share_data_dir, MC_GLOBAL_CONFIG_FILE, (char *) nullptr);
    }

    panels_profile_name = mc_config_get_full_path (MC_PANELS_FILE);

    mc_global.main_config = mc_config_init (profile, false);

    if (!exist_file (panels_profile_name))
        setup__move_panels_config_into_separate_file (profile);

    mc_global.panels_config = mc_config_init (panels_profile_name, false);

    load_config ();
    load_layout ();
    panels_load_options ();
    load_panelize ();

    /* Load time formats */
    user_recent_timeformat =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "timeformat_recent",
                              FMTTIME);
    user_old_timeformat =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "timeformat_old",
                              FMTYEAR);

#ifdef ENABLE_VFS_FTP
    ftpfs_proxy_host =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "ftp_proxy_host", "gate");
    ftpfs_init_passwd ();
#endif /* ENABLE_VFS_FTP */

    /* The default color and the terminal dependent color */
    mc_global.tty.setup_color_string =
        mc_config_get_string (mc_global.main_config, "Colors", "base_color", "");
    mc_global.tty.term_color_string =
        mc_config_get_string (mc_global.main_config, "Colors", getenv ("TERM"), "");
    mc_global.tty.color_terminal_string =
        mc_config_get_string (mc_global.main_config, "Colors", "color_terminals", "");

    /* Load the directory history */
    /*    directory_history_load (); */
    /* Remove the temporal entries */

#ifdef HAVE_CHARSET
    if (codepages->len > 1)
    {
        char *buffer;

        buffer =
            mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "display_codepage",
                                  "");
        if (buffer[0] != '\0')
        {
            mc_global.display_codepage = get_codepage_index (buffer);
            cp_display = get_codepage_id (mc_global.display_codepage);
        }
        g_free (buffer);
        buffer =
            mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "source_codepage",
                                  "");
        if (buffer[0] != '\0')
        {
            default_source_codepage = get_codepage_index (buffer);
            mc_global.source_codepage = default_source_codepage;        /* May be source_codepage doesn't need this */
            cp_source = get_codepage_id (mc_global.source_codepage);
        }
        g_free (buffer);
    }

    autodetect_codeset =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "autodetect_codeset", "");
    if ((autodetect_codeset[0] != '\0') && (strcmp (autodetect_codeset, "off") != 0))
        is_autodetect_codeset_enabled = true;

    g_free (init_translation_table (mc_global.source_codepage, mc_global.display_codepage));
    cbuffer = get_codepage_id (mc_global.display_codepage);
    if (cbuffer != nullptr)
        mc_global.utf8_display = str_isutf8 (cbuffer);
#endif /* HAVE_CHARSET */

#ifdef HAVE_ASPELL
    spell_language =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "spell_language", "en");
#endif /* HAVE_ASPELL */

    clipboard_store_path =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "clipboard_store", "");
    clipboard_paste_path =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "clipboard_paste", "");
}

/* --------------------------------------------------------------------------------------------- */

bool
save_setup (bool save_options, bool save_panel_options)
{
    bool ret = true;

    saving_setup = 1;

    save_hotlist ();

    if (save_panel_options)
        save_panel_types ();

    if (save_options)
    {
        char *tmp_profile;

        save_config ();
        save_layout ();
        panels_save_options ();
        save_panelize ();
        /* directory_history_save (); */

#ifdef ENABLE_VFS_FTP
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "ftpfs_password",
                              ftpfs_anonymous_passwd);
        if (ftpfs_proxy_host)
            mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "ftp_proxy_host",
                                  ftpfs_proxy_host);
#endif /* ENABLE_VFS_FTP */

#ifdef HAVE_CHARSET
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "display_codepage",
                              get_codepage_id (mc_global.display_codepage));
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "source_codepage",
                              get_codepage_id (default_source_codepage));
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "autodetect_codeset",
                              autodetect_codeset);
#endif /* HAVE_CHARSET */

#ifdef HAVE_ASPELL
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "spell_language",
                              spell_language);
#endif /* HAVE_ASPELL */

        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "clipboard_store",
                              clipboard_store_path);
        mc_config_set_string (mc_global.main_config, CONFIG_MISC_SECTION, "clipboard_paste",
                              clipboard_paste_path);

        tmp_profile = mc_config_get_full_path (MC_CONFIG_FILE);
        ret = mc_config_save_to_file (mc_global.main_config, tmp_profile, nullptr);
        g_free (tmp_profile);
    }

    saving_setup = 0;

    return ret;
}

/* --------------------------------------------------------------------------------------------- */

void
done_setup (void)
{
    size_t i;

    g_free (clipboard_store_path);
    g_free (clipboard_paste_path);
    g_free (global_profile_name);
    g_free (mc_global.tty.color_terminal_string);
    g_free (mc_global.tty.term_color_string);
    g_free (mc_global.tty.setup_color_string);
    g_free (profile_name);
    g_free (panels_profile_name);
    mc_config_deinit (mc_global.main_config);
    mc_config_deinit (mc_global.panels_config);

    g_free (user_recent_timeformat);
    g_free (user_old_timeformat);

    for (i = 0; str_options[i].opt_name != nullptr; i++)
        g_free (*str_options[i].opt_addr);

    done_hotlist ();
    done_panelize ();
    /*    directory_history_free (); */

#ifdef HAVE_CHARSET
    g_free (autodetect_codeset);
    free_codepages_list ();
#endif

#ifdef HAVE_ASPELL
    g_free (spell_language);
#endif /* HAVE_ASPELL */
}


/* --------------------------------------------------------------------------------------------- */

void
setup_save_config_show_error (const char *filename, GError ** mcerror)
{
    if (mcerror != nullptr && *mcerror != nullptr)
    {
        message (D_ERROR, MSG_ERROR, _("Cannot save file %s:\n%s"), filename, (*mcerror)->message);
        g_error_free (*mcerror);
        *mcerror = nullptr;
    }
}


/* --------------------------------------------------------------------------------------------- */

void
load_key_defs (void)
{
    /*
     * Load keys from mc.lib before ${XDG_CONFIG_HOME}/mc/ini, so that the user
     * definitions override global settings.
     */
    mc_config_t *mc_global_config;

    mc_global_config = mc_config_init (global_profile_name, false);
    if (mc_global_config != nullptr)
    {
        load_keys_from_section ("general", mc_global_config);
        load_keys_from_section (getenv ("TERM"), mc_global_config);
        mc_config_deinit (mc_global_config);
    }

    load_keys_from_section ("general", mc_global.main_config);
    load_keys_from_section (getenv ("TERM"), mc_global.main_config);
}

/* --------------------------------------------------------------------------------------------- */

#ifdef ENABLE_VFS_FTP
char *
load_anon_passwd (void)
{
    char *buffer;

    buffer =
        mc_config_get_string (mc_global.main_config, CONFIG_MISC_SECTION, "ftpfs_password", "");

    if ((buffer != nullptr) && (buffer[0] != '\0'))
        return buffer;

    g_free (buffer);
    return nullptr;
}
#endif /* ENABLE_VFS_FTP */

/* --------------------------------------------------------------------------------------------- */

void
load_keymap_defs (bool load_from_file)
{
    /*
     * Load keymap from GLOBAL_KEYMAP_FILE before ${XDG_CONFIG_HOME}/mc/mc.keymap, so that the user
     * definitions override global settings.
     */
    mc_config_t *mc_global_keymap;

    mc_global_keymap = load_setup_get_keymap_profile_config (load_from_file);

    if (mc_global_keymap != nullptr)
    {
#define LOAD_KEYMAP(s,km) \
    km##_keymap = g_array_new (true, false, sizeof (global_keymap_t)); \
    load_keymap_from_section (KEYMAP_SECTION_##s, km##_keymap, mc_global_keymap)

        LOAD_KEYMAP (MAIN, main);
        LOAD_KEYMAP (MAIN_EXT, main_x);
        LOAD_KEYMAP (PANEL, panel);
        LOAD_KEYMAP (DIALOG, dialog);
        LOAD_KEYMAP (MENU, menu);
        LOAD_KEYMAP (INPUT, input);
        LOAD_KEYMAP (LISTBOX, listbox);
        LOAD_KEYMAP (RADIO, radio);
        LOAD_KEYMAP (TREE, tree);
        LOAD_KEYMAP (HELP, help);
#ifdef ENABLE_EXT2FS_ATTR
        LOAD_KEYMAP (CHATTR, chattr);
#endif
#ifdef USE_INTERNAL_EDIT
        LOAD_KEYMAP (EDITOR, editor);
        LOAD_KEYMAP (EDITOR_EXT, editor_x);
#endif
        LOAD_KEYMAP (VIEWER, viewer);
        LOAD_KEYMAP (VIEWER_HEX, viewer_hex);
#ifdef USE_DIFF_VIEW
        LOAD_KEYMAP (DIFFVIEWER, diff);
#endif

#undef LOAD_KEYMAP
        mc_config_deinit (mc_global_keymap);
    }

#define SET_MAP(m) \
    m##_map = (global_keymap_t *) m##_keymap->data

    SET_MAP (main);
    SET_MAP (main_x);
    SET_MAP (panel);
    SET_MAP (dialog);
    SET_MAP (menu);
    SET_MAP (input);
    SET_MAP (listbox);
    SET_MAP (radio);
    SET_MAP (tree);
    SET_MAP (help);
#ifdef ENABLE_EXT2FS_ATTR
    SET_MAP (chattr);
#endif
#ifdef USE_INTERNAL_EDIT
    SET_MAP (editor);
    SET_MAP (editor_x);
#endif
    SET_MAP (viewer);
    SET_MAP (viewer_hex);
#ifdef USE_DIFF_VIEW
    SET_MAP (diff);
#endif

#undef SET_MAP
}

/* --------------------------------------------------------------------------------------------- */

void
free_keymap_defs (void)
{
#define FREE_KEYMAP(km) \
    if (km##_keymap != nullptr) \
        g_array_free (km##_keymap, true)

    FREE_KEYMAP (main);
    FREE_KEYMAP (main_x);
    FREE_KEYMAP (panel);
    FREE_KEYMAP (dialog);
    FREE_KEYMAP (menu);
    FREE_KEYMAP (input);
    FREE_KEYMAP (listbox);
    FREE_KEYMAP (radio);
    FREE_KEYMAP (tree);
    FREE_KEYMAP (help);
#ifdef ENABLE_EXT2FS_ATTR
    FREE_KEYMAP (chattr);
#endif
#ifdef USE_INTERNAL_EDIT
    FREE_KEYMAP (editor);
    FREE_KEYMAP (editor_x);
#endif
    FREE_KEYMAP (viewer);
    FREE_KEYMAP (viewer_hex);
#ifdef USE_DIFF_VIEW
    FREE_KEYMAP (diff);
#endif

#undef FREE_KEYMAP
}

/* --------------------------------------------------------------------------------------------- */

void
panel_load_setup (WPanel * panel, const char *section)
{
    size_t i;
    char *buffer, buffer2[BUF_TINY];

    panel->sort_info.reverse =
        mc_config_get_bool (mc_global.panels_config, section, "reverse", false);
    panel->sort_info.case_sensitive =
        mc_config_get_bool (mc_global.panels_config, section, "case_sensitive",
                            OS_SORT_CASE_SENSITIVE_DEFAULT);
    panel->sort_info.exec_first =
        mc_config_get_bool (mc_global.panels_config, section, "exec_first", false);

    /* Load sort order */
    buffer = mc_config_get_string (mc_global.panels_config, section, "sort_order", "name");
    panel->sort_field = panel_get_field_by_id (buffer);
    if (panel->sort_field == nullptr)
        panel->sort_field = panel_get_field_by_id ("name");

    g_free (buffer);

    /* Load the listing format */
    buffer = mc_config_get_string (mc_global.panels_config, section, "list_format", nullptr);
    if (buffer == nullptr)
    {
        /* fallback to old option */
        buffer = mc_config_get_string (mc_global.panels_config, section, "list_mode", "full");
    }
    panel->list_format = list_full;
    for (i = 0; list_formats[i].key != nullptr; i++)
        if (g_ascii_strcasecmp (list_formats[i].key, buffer) == 0)
        {
            panel->list_format = static_cast<list_format_t> (list_formats[i].list_format);
            break;
        }
    g_free (buffer);

    panel->brief_cols = mc_config_get_int (mc_global.panels_config, section, "brief_cols", 2);

    /* User formats */
    g_free (panel->user_format);
    panel->user_format =
        mc_config_get_string (mc_global.panels_config, section, "user_format", DEFAULT_USER_FORMAT);

    for (i = 0; i < LIST_FORMATS; i++)
    {
        g_free (panel->user_status_format[i]);
        g_snprintf (buffer2, sizeof (buffer2), "user_status%lld", (long long) i);
        panel->user_status_format[i] =
            mc_config_get_string (mc_global.panels_config, section, buffer2, DEFAULT_USER_FORMAT);
    }

    panel->user_mini_status =
        mc_config_get_bool (mc_global.panels_config, section, "user_mini_status", false);
}

/* --------------------------------------------------------------------------------------------- */

void
panel_save_setup (WPanel * panel, const char *section)
{
    char buffer[BUF_TINY];
    size_t i;

    mc_config_set_bool (mc_global.panels_config, section, "reverse", panel->sort_info.reverse);
    mc_config_set_bool (mc_global.panels_config, section, "case_sensitive",
                        panel->sort_info.case_sensitive);
    mc_config_set_bool (mc_global.panels_config, section, "exec_first",
                        panel->sort_info.exec_first);

    mc_config_set_string (mc_global.panels_config, section, "sort_order", panel->sort_field->id);

    for (i = 0; list_formats[i].key != nullptr; i++)
        if (list_formats[i].list_format == (int) panel->list_format)
        {
            mc_config_set_string (mc_global.panels_config, section, "list_format",
                                  list_formats[i].key);
            break;
        }

    mc_config_set_int (mc_global.panels_config, section, "brief_cols", panel->brief_cols);

    mc_config_set_string (mc_global.panels_config, section, "user_format", panel->user_format);

    for (i = 0; i < LIST_FORMATS; i++)
    {
        g_snprintf (buffer, sizeof (buffer), "user_status%lld", (long long) i);
        mc_config_set_string (mc_global.panels_config, section, buffer,
                              panel->user_status_format[i]);
    }

    mc_config_set_bool (mc_global.panels_config, section, "user_mini_status",
                        panel->user_mini_status);
}

/* --------------------------------------------------------------------------------------------- */
