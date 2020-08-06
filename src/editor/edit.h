/*
   Editor public API
 */

/** \file edit.h
 *  \brief Header: editor public API
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Andrew Borodin
 *  \date 2009, 2012
 */

#ifndef MC__EDIT_H
#define MC__EDIT_H

#include "lib/global.h"         /* PATH_SEP_STR */
#include "lib/vfs/vfs.h"        /* vfs_path_t */

/*** typedefs(not structures) and defined constants **********************************************/

#define DEFAULT_WRAP_LINE_LENGTH 72

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

/* Editor widget */
struct WEdit;
typedef struct WEdit WEdit;

/*** global variables defined in .c file *********************************************************/

extern int option_word_wrap_line_length;
extern bool option_typewriter_wrap;
extern bool option_auto_para_formatting;
extern bool option_fill_tabs_with_spaces;
extern bool option_return_does_auto_indent;
extern bool option_backspace_through_tabs;
extern bool option_fake_half_tabs;
extern bool option_persistent_selections;
extern bool option_drop_selection_on_copy;
extern bool option_cursor_beyond_eol;
extern bool option_cursor_after_inserted_block;
extern bool option_state_full_filename;
extern bool option_line_state;
extern int option_save_mode;
extern bool option_save_position;
extern bool option_syntax_highlighting;
extern bool option_group_undo;
extern char *option_backup_ext;
extern char *option_filesize_threshold;
extern char *option_stop_format_chars;

extern bool edit_confirm_save;

extern bool visible_tabs;
extern bool visible_tws;

extern bool simple_statusbar;
extern bool option_check_nl_at_eof;
extern bool show_right_margin;

/*** declarations of public functions ************************************************************/

/* used in main() */
void edit_stack_init (void);
void edit_stack_free (void);

bool edit_file (const vfs_path_t * file_vpath, long line);
bool edit_files (const GList * files);

const char *edit_get_file_name (const WEdit * edit);
off_t edit_get_cursor_offset (const WEdit * edit);
long edit_get_curs_col (const WEdit * edit);
const char *edit_get_syntax_type (const WEdit * edit);

/*** inline functions ****************************************************************************/
#endif /* MC__EDIT_H */
