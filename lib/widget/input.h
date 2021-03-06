
/** \file input.h
 *  \brief Header: WInput widget
 */

#ifndef MC__WIDGET_INPUT_H
#define MC__WIDGET_INPUT_H

#include <limits.h>             /* MB_LEN_MAX */

/*** typedefs(not structures) and defined constants **********************************************/

#define INPUT(x) ((WInput *)(x))

/* For history load-save functions */
#define INPUT_LAST_TEXT ((char *) 2)

/*** enums ***************************************************************************************/

typedef enum
{
    WINPUTC_MAIN,               /* color used */
    WINPUTC_MARK,               /* color for marked text */
    WINPUTC_UNCHANGED,          /* color for inactive text (Is first keystroke) */
    WINPUTC_HISTORY,            /* color for history list */
    WINPUTC_COUNT_COLORS        /* count of used colors */
} input_colors_enum_t;

/* completion flags */
typedef int input_complete_t;
#define INPUT_COMPLETE_NONE 0
#define INPUT_COMPLETE_FILENAMES (1 << 0)
#define INPUT_COMPLETE_HOSTNAMES (1 << 1)
#define INPUT_COMPLETE_COMMANDS (1 << 2)
#define INPUT_COMPLETE_VARIABLES (1 << 3)
#define INPUT_COMPLETE_USERNAMES (1 << 4)
#define INPUT_COMPLETE_CD (1 << 5)
#define INPUT_COMPLETE_SHELL_ESC (1 << 6)

/*** structures declarations (and typedefs of structures)*****************************************/

typedef int input_colors_t[WINPUTC_COUNT_COLORS];

typedef struct
{
    Widget widget;
    const int *color;
    int point;                  /* cursor position in the input line in characters */
    int mark;                   /* the mark position in characters; negative value means no marked text */
    int term_first_shown;       /* column of the first shown character */
    size_t current_max_size;    /* maximum length of input line (bytes) */
    bool first;             /* is first keystroke? */
    int disable_update;         /* do we want to skip updates? */
    bool is_password;       /* is this a password input line? */
    bool init_from_history; /* init text will be get from history */
    char *buffer;               /* pointer to editing buffer */
    bool need_push;         /* need to push the current Input on hist? */
    bool strip_password;    /* need to strip password before placing string to history */
    char **completions;         /* possible completions array */
    input_complete_t completion_flags;
    char charbuf[MB_LEN_MAX];   /* buffer for multibytes characters */
    size_t charpoint;           /* point to end of mulibyte sequence in charbuf */
    WLabel *label;              /* label associated with this input line */
    struct input_history_t
    {
        char *name;             /* name of history for loading and saving */
        GList *list;            /* the history */
        GList *current;         /* current history item */
        bool changed;       /* the history has changed */
    } history;
} WInput;

/*** global variables defined in .c file *********************************************************/

extern bool quote;

extern const global_keymap_t *input_map;

/* Color styles for normal and command line input widgets */
extern input_colors_t input_colors;

/*** declarations of public functions ************************************************************/

WInput *input_new (int y, int x, const int *colors,
                   int len, const char *text, const char *histname,
                   input_complete_t completion_flags);
/* callbac is public; needed for command line */
cb_ret_t input_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data);
void input_set_default_colors (void);
cb_ret_t input_handle_char (WInput * in, int key);
void input_assign_text (WInput * in, const char *text);
bool input_is_empty (const WInput * in);
void input_insert (WInput * in, const char *text, bool insert_extra_space);
void input_set_point (WInput * in, int pos);
void input_update (WInput * in, bool clear_first);
void input_enable_update (WInput * in);
void input_disable_update (WInput * in);
void input_clean (WInput * in);

/* input_complete.c */
void input_complete (WInput * in);
void input_complete_free (WInput * in);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_INPUT_H */
