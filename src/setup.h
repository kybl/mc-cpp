/** \file setup.h
 *  \brief Header: setup loading/saving
 */

#ifndef MC__SETUP_H
#define MC__SETUP_H

#include <config.h>

#include "lib/global.h"         /* GError */

#include "filemanager/layout.h" /* panel_view_mode_t */
#include "filemanager/panel.h"  /* WPanel */

/*** typedefs(not structures) and defined constants **********************************************/

/* TAB length for editor and viewer */
#define DEFAULT_TAB_SPACING 8

#define MAX_MACRO_LENGTH 1024

/*** enums ***************************************************************************************/

typedef enum
{
    QSEARCH_CASE_INSENSITIVE = 0,       /* quick search in case insensitive mode */
    QSEARCH_CASE_SENSITIVE = 1, /* quick search in case sensitive mode */
    QSEARCH_PANEL_CASE = 2,     /* quick search get value from panel case_sensitive */
    QSEARCH_NUM
} qsearch_mode_t;

/*** structures declarations (and typedefs of structures)*****************************************/

/* panels ini options; [Panels] section */
typedef struct
{
    bool show_mini_info;    /* If true, show the mini-info on the panel */
    bool kilobyte_si;       /* If true, SI units (1000 based) will be used for larger units
                                 * (kilobyte, megabyte, ...). If false, binary units (1024 based) will be used */
    bool mix_all_files;     /* If false then directories are shown separately from files */
    bool show_backups;      /* If true, show files ending in ~ */
    bool show_dot_files;    /* If true, show files starting with a dot */
    bool fast_reload;       /* If true then use stat() on the cwd to determine directory changes */
    bool fast_reload_msg_shown;     /* Have we shown the fast-reload warning in the past? */
    bool mark_moves_down;   /* If true, marking a files moves the cursor down */
    bool reverse_files_only;        /* If true, only selection of files is inverted */
    bool auto_save_setup;
    bool navigate_with_arrows;      /* If true: l&r arrows are used to chdir if the input line is empty */
    bool scroll_pages;      /* If true, panel is scrolled by half the display when the cursor reaches
                                   the end or the beginning of the panel */
    bool scroll_center;     /* If true, scroll when the cursor hits the middle of the panel */
    bool mouse_move_pages;  /* Scroll page/item using mouse wheel */
    bool filetype_mode;     /* If true then add per file type hilighting */
    bool permission_mode;   /* If true, we use permission hilighting */
    qsearch_mode_t qsearch_mode;        /* Quick search mode */
    bool torben_fj_mode;    /* If true, use some usability hacks by Torben */
    panel_select_flags_t select_flags;  /* Select/unselect file flags */
} panels_options_t;

typedef struct macro_action_t
{
    long action;
    int ch;
} macro_action_t;

typedef struct macros_t
{
    int hotkey;
    GArray *macro;
} macros_t;

struct mc_fhl_struct;

/*** global variables defined in .c file *********************************************************/

/* global parameters */
extern char *global_profile_name;
extern bool confirm_delete;
extern bool confirm_directory_hotlist_delete;
extern bool confirm_execute;
extern bool confirm_exit;
extern bool confirm_overwrite;
extern bool confirm_view_dir;
extern bool safe_delete;
extern bool safe_overwrite;
extern bool clear_before_exec;
extern bool auto_menu;
extern bool drop_menus;
extern bool verbose;
extern bool copymove_persistent_attr;
extern bool classic_progressbar;
extern bool easy_patterns;
extern int option_tab_spacing;
extern bool auto_save_setup;
extern bool only_leading_plus_minus;
extern int cd_symlinks;
extern bool auto_fill_mkdir_name;
extern bool output_starts_shell;
extern bool use_file_to_check_type;
extern bool file_op_compute_totals;
extern bool editor_ask_filename_before_edit;

extern panels_options_t panels_options;

extern panel_view_mode_t startup_left_mode;
extern panel_view_mode_t startup_right_mode;
extern bool boot_current_is_left;
extern bool use_internal_view;
extern bool use_internal_edit;

#ifdef HAVE_CHARSET
extern int default_source_codepage;
extern char *autodetect_codeset;
extern bool is_autodetect_codeset_enabled;
#endif /* !HAVE_CHARSET */

#ifdef HAVE_ASPELL
extern char *spell_language;
#endif

/* Value of "other_dir" key in ini file */
extern char *saved_other_dir;

/* If set, then print to the given file the last directory we were at */
extern char *last_wd_string;

extern int quit;
/* Set to true to suppress printing the last directory */
extern bool print_last_revert;

#ifdef USE_INTERNAL_EDIT
/* index to record_macro_buf[], -1 if not recording a macro */
extern int macro_index;

/* macro stuff */
extern struct macro_action_t record_macro_buf[MAX_MACRO_LENGTH];

extern GArray *macros_list;
#endif /* USE_INTERNAL_EDIT */

extern int saving_setup;

/*** declarations of public functions ************************************************************/

const char *setup_init (void);
void load_setup (void);
bool save_setup (bool save_options, bool save_panel_options);
void done_setup (void);
void setup_save_config_show_error (const char *filename, GError ** mcerror);

void load_key_defs (void);
#ifdef ENABLE_VFS_FTP
char *load_anon_passwd (void);
#endif /* ENABLE_VFS_FTP */

void load_keymap_defs (bool load_from_file);
void free_keymap_defs (void);

void panel_load_setup (WPanel * panel, const char *section);
void panel_save_setup (WPanel * panel, const char *section);

/*** inline functions ****************************************************************************/

#endif /* MC__SETUP_H */
