/*
   Editor syntax highlighting.

   Copyright (C) 1996-2020
   Free Software Foundation, Inc.

   Written by:
   Paul Sheer, 1998
   Egmont Koblinger <egmont@gmail.com>, 2010
   Slava Zanko <slavazanko@gmail.com>, 2013
   Andrew Borodin <aborodin@vmail.ru>, 2013, 2014

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

/** \file
 *  \brief Source: editor syntax highlighting
 *  \author Paul Sheer
 *  \date 1996, 1997
 *  \author Mikhail Pobolovets
 *  \date 2010
 *
 *  Mispelled words are flushed from the syntax highlighting rules
 *  when they have been around longer than
 *  TRANSIENT_WORD_TIME_OUT seconds. At a cursor rate of 30
 *  chars per second and say 3 chars + a space per word, we can
 *  accumulate 450 words absolute max with a value of 60. This is
 *  below this limit of 1024 words in a context.
 */

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "lib/global.h"
#include "lib/search.h"         /* search engine */
#include "lib/skin.h"
#include "lib/fileloc.h"        /* EDIT_HOME_DIR, EDIT_HOME_SYNTAX_FILE */
#include "lib/strutil.h"        /* utf string functions */
#include "lib/util.h"
#include "lib/widget.h"         /* message() */

#include "edit-impl.h"
#include "editwidget.h"

/*** global variables ****************************************************************************/

bool option_syntax_highlighting = true;
bool option_auto_syntax = true;

/*** file scope macro definitions ****************************************************************/

/* bytes */
#define SYNTAX_MARKER_DENSITY 512

#define RULE_ON_LEFT_BORDER 1
#define RULE_ON_RIGHT_BORDER 2

#define SYNTAX_TOKEN_STAR       '\001'
#define SYNTAX_TOKEN_PLUS       '\002'
#define SYNTAX_TOKEN_BRACKET    '\003'
#define SYNTAX_TOKEN_BRACE      '\004'

#define break_a { result = line; break; }
#define check_a { if (*a == nullptr) { result = line; break; } }
#define check_not_a { if (*a != nullptr) { result = line ;break; } }

#define SYNTAX_KEYWORD(x) ((syntax_keyword_t *) (x))
#define CONTEXT_RULE(x) ((context_rule_t *) (x))

#define ARGS_LEN 1024

/*** file scope type declarations ****************************************************************/

typedef struct
{
    char *keyword;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    bool line_start;
    int color;
} syntax_keyword_t;

typedef struct
{
    char *left;
    unsigned char first_left;
    char *right;
    unsigned char first_right;
    bool line_start_left;
    bool line_start_right;
    bool between_delimiters;
    char *whole_word_chars_left;
    char *whole_word_chars_right;
    char *keyword_first_chars;
    bool spelling;
    /* first word is word[1] */
    GPtrArray *keyword;
} context_rule_t;

typedef struct
{
    off_t offset;
    edit_syntax_rule_t rule;
} syntax_marker_t;

/*** file scope variables ************************************************************************/

static char *error_file_name = nullptr;

/* --------------------------------------------------------------------------------------------- */
/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
syntax_keyword_free (gpointer keyword)
{
    syntax_keyword_t *k = SYNTAX_KEYWORD (keyword);

    g_free (k->keyword);
    g_free (k->whole_word_chars_left);
    g_free (k->whole_word_chars_right);
    g_free (k);
}

/* --------------------------------------------------------------------------------------------- */

static void
context_rule_free (gpointer rule)
{
    context_rule_t *r = CONTEXT_RULE (rule);

    g_free (r->left);
    g_free (r->right);
    g_free (r->whole_word_chars_left);
    g_free (r->whole_word_chars_right);
    g_free (r->keyword_first_chars);

    if (r->keyword != nullptr)
    {
        g_ptr_array_foreach (r->keyword, (GFunc) syntax_keyword_free, nullptr);
        g_ptr_array_free (r->keyword, true);
    }

    g_free (r);
}

/* --------------------------------------------------------------------------------------------- */

static gint
mc_defines_destroy (gpointer key, gpointer value, gpointer data)
{
    (void) data;

    g_free (key);
    g_strfreev ((char **) value);

    return false;
}

/* --------------------------------------------------------------------------------------------- */
/** Completely destroys the defines tree */

static void
destroy_defines (GTree ** defines)
{
    g_tree_foreach (*defines, mc_defines_destroy, nullptr);
    g_tree_destroy (*defines);
    *defines = nullptr;
}

/* --------------------------------------------------------------------------------------------- */

/** Wrapper for case insensitive mode */
inline static int
xx_tolower (const WEdit * edit, int c)
{
    return edit->is_case_insensitive ? tolower (c) : c;
}

/* --------------------------------------------------------------------------------------------- */

static void
subst_defines (GTree * defines, char **argv, char **argv_end)
{
    for (; *argv != nullptr && argv < argv_end; argv++)
    {
        char **t = static_cast<char **> (g_tree_lookup (defines, *argv));
        if (t != nullptr)
        {
            int argc, count;
            char **p;

            /* Count argv array members */
            argc = g_strv_length (argv + 1);

            /* Count members of definition array */
            count = g_strv_length (t);

            p = argv + count + argc;
            /* Buffer overflow or infinitive loop in define */
            if (p >= argv_end)
                break;

            /* Move rest of argv after definition members */
            while (argc >= 0)
                *p-- = argv[argc-- + 1];

            /* Copy definition members to argv */
            for (p = argv; *t != nullptr; *p++ = *t++)
                ;
        }
    }
}

/* --------------------------------------------------------------------------------------------- */

static off_t
compare_word_to_right (const WEdit * edit, off_t i, const char *text,
                       const char *whole_left, const char *whole_right, bool line_start)
{
    const unsigned char *p, *q;
    int c, d, j;

    if (*text == '\0')
        return -1;

    c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i - 1));
    if ((line_start && c != '\n') || (whole_left != nullptr && strchr (whole_left, c) != nullptr))
        return -1;

    for (p = (const unsigned char *) text, q = p + strlen ((const char *) p); p < q; p++, i++)
    {
        switch (*p)
        {
        case SYNTAX_TOKEN_STAR:
            if (++p > q)
                return -1;
            while (true)
            {
                c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i));
                if (*p == '\0' && whole_right != nullptr && strchr (whole_right, c) == nullptr)
                    break;
                if (c == *p)
                    break;
                if (c == '\n')
                    return -1;
                i++;
            }
            break;
        case SYNTAX_TOKEN_PLUS:
            if (++p > q)
                return -1;
            j = 0;
            while (true)
            {
                c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i));
                if (c == *p)
                {
                    j = i;
                    if (p[0] == text[0] && p[1] == '\0')        /* handle eg '+' and @+@ keywords properly */
                        break;
                }
                if (j != 0 && strchr ((const char *) p + 1, c) != nullptr) /* c exists further down, so it will get matched later */
                    break;
                if (whiteness (c) || (whole_right != nullptr && strchr (whole_right, c) == nullptr))
                {
                    if (*p == '\0')
                    {
                        i--;
                        break;
                    }
                    if (j == 0)
                        return -1;
                    i = j;
                    break;
                }
                i++;
            }
            break;
        case SYNTAX_TOKEN_BRACKET:
            if (++p > q)
                return -1;
            c = -1;
            while (true)
            {
                d = c;
                c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i));
                for (j = 0; p[j] != SYNTAX_TOKEN_BRACKET && p[j] != '\0'; j++)
                    if (c == p[j])
                        goto found_char2;
                break;
              found_char2:
                i++;
            }
            i--;
            while (*p != SYNTAX_TOKEN_BRACKET && p <= q)
                p++;
            if (p > q)
                return -1;
            if (p[1] == d)
                i--;
            break;
        case SYNTAX_TOKEN_BRACE:
            if (++p > q)
                return -1;
            c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i));
            for (; *p != SYNTAX_TOKEN_BRACE && *p != '\0'; p++)
                if (c == *p)
                    goto found_char3;
            return -1;
          found_char3:
            while (*p != SYNTAX_TOKEN_BRACE && p < q)
                p++;
            break;
        default:
            if (*p != xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i)))
                return -1;
        }
    }
    return (whole_right != nullptr &&
            strchr (whole_right,
                    xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i))) != nullptr) ? -1 : i;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
xx_strchr (const WEdit * edit, const unsigned char *s, int char_byte)
{
    while (*s >= '\005' && xx_tolower (edit, *s) != char_byte)
        s++;

    return (const char *) s;
}

/* --------------------------------------------------------------------------------------------- */

static void
apply_rules_going_right (WEdit * edit, off_t i)
{
    context_rule_t *r;
    int c;
    bool contextchanged = false;
    bool found_left = false, found_right = false;
    bool keyword_foundleft = false, keyword_foundright = false;
    bool is_end;
    off_t end = 0;
    edit_syntax_rule_t _rule = edit->rule;

    c = xx_tolower (edit, edit_buffer_get_byte (&edit->buffer, i));
    if (c == 0)
        return;

    is_end = (edit->rule.end == i);

    /* check to turn off a keyword */
    if (_rule.keyword != 0)
    {
        if (edit_buffer_get_byte (&edit->buffer, i - 1) == '\n')
            _rule.keyword = 0;
        if (is_end)
        {
            _rule.keyword = 0;
            keyword_foundleft = true;
        }
    }

    /* check to turn off a context */
    if (_rule.context != 0 && _rule.keyword == 0)
    {
        off_t e;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        if (r->first_right == c && (edit->rule.border & RULE_ON_RIGHT_BORDER) == 0
            && (e =
                compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                       r->whole_word_chars_right, r->line_start_right)) > 0)
        {
            _rule.end = e;
            found_right = true;
            _rule.border = RULE_ON_RIGHT_BORDER;
            if (r->between_delimiters)
                _rule.context = 0;
        }
        else if (is_end && (edit->rule.border & RULE_ON_RIGHT_BORDER) != 0)
        {
            /* always turn off a context at 4 */
            found_left = true;
            _rule.border = 0;
            if (!keyword_foundleft)
                _rule.context = 0;
        }
        else if (is_end && (edit->rule.border & RULE_ON_LEFT_BORDER) != 0)
        {
            /* never turn off a context at 2 */
            found_left = true;
            _rule.border = 0;
        }
    }

    /* check to turn on a keyword */
    if (_rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        p = r->keyword_first_chars;

        if (p != nullptr)
            while (*(p = xx_strchr (edit, (const unsigned char *) p + 1, c)) != '\0')
            {
                syntax_keyword_t *k;
                int count;
                off_t e;

                count = p - r->keyword_first_chars;
                k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));
                e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left,
                                           k->whole_word_chars_right, k->line_start);
                if (e > 0)
                {
                    /* when both context and keyword terminate with a newline,
                       the context overflows to the next line and colorizes it incorrectly */
                    if (e > i + 1 && _rule._context != 0
                        && k->keyword[strlen (k->keyword) - 1] == '\n')
                    {
                        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule._context));
                        if (r->right != nullptr && r->right[0] != '\0'
                            && r->right[strlen (r->right) - 1] == '\n')
                            e--;
                    }

                    end = e;
                    _rule.end = e;
                    _rule.keyword = count;
                    keyword_foundright = true;
                    break;
                }
            }
    }

    /* check to turn on a context */
    if (_rule.context == 0)
    {
        if (!found_left && is_end)
        {
            if ((edit->rule.border & RULE_ON_RIGHT_BORDER) != 0)
            {
                _rule.border = 0;
                _rule.context = 0;
                contextchanged = true;
                _rule.keyword = 0;

            }
            else if ((edit->rule.border & RULE_ON_LEFT_BORDER) != 0)
            {
                r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule._context));
                _rule.border = 0;
                if (r->between_delimiters)
                {
                    _rule.context = _rule._context;
                    contextchanged = true;
                    _rule.keyword = 0;

                    if (r->first_right == c)
                    {
                        off_t e;

                        e = compare_word_to_right (edit, i, r->right, r->whole_word_chars_left,
                                                   r->whole_word_chars_right, r->line_start_right);
                        if (e >= end)
                        {
                            _rule.end = e;
                            found_right = true;
                            _rule.border = RULE_ON_RIGHT_BORDER;
                            _rule.context = 0;
                        }
                    }
                }
            }
        }

        if (!found_right)
        {
            size_t count;

            for (count = 1; count < edit->rules->len; count++)
            {
                r = CONTEXT_RULE (g_ptr_array_index (edit->rules, count));
                if (r->first_left == c)
                {
                    off_t e;

                    e = compare_word_to_right (edit, i, r->left, r->whole_word_chars_left,
                                               r->whole_word_chars_right, r->line_start_left);
                    if (e >= end && (_rule.keyword == 0 || keyword_foundright))
                    {
                        _rule.end = e;
                        found_right = true;
                        _rule.border = RULE_ON_LEFT_BORDER;
                        _rule._context = count;
                        if (!r->between_delimiters && _rule.keyword == 0)
                        {
                            _rule.context = count;
                            contextchanged = true;
                        }
                        break;
                    }
                }
            }
        }
    }

    /* check again to turn on a keyword if the context switched */
    if (contextchanged && _rule.keyword == 0)
    {
        const char *p;

        r = CONTEXT_RULE (g_ptr_array_index (edit->rules, _rule.context));
        p = r->keyword_first_chars;

        while (*(p = xx_strchr (edit, (const unsigned char *) p + 1, c)) != '\0')
        {
            syntax_keyword_t *k;
            int count;
            off_t e;

            count = p - r->keyword_first_chars;
            k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, count));
            e = compare_word_to_right (edit, i, k->keyword, k->whole_word_chars_left,
                                       k->whole_word_chars_right, k->line_start);
            if (e > 0)
            {
                _rule.end = e;
                _rule.keyword = count;
                break;
            }
        }
    }

    edit->rule = _rule;
}

/* --------------------------------------------------------------------------------------------- */

static void
edit_get_rule (WEdit * edit, off_t byte_index)
{
    off_t i;

    if (byte_index > edit->last_get_rule)
    {
        for (i = edit->last_get_rule + 1; i <= byte_index; i++)
        {
            off_t d = SYNTAX_MARKER_DENSITY;

            apply_rules_going_right (edit, i);

            if (edit->syntax_marker != nullptr)
                d += ((syntax_marker_t *) edit->syntax_marker->data)->offset;

            if (i > d)
            {
                syntax_marker_t *s;

                s = g_new (syntax_marker_t, 1);
                s->offset = i;
                s->rule = edit->rule;
                edit->syntax_marker = g_slist_prepend (edit->syntax_marker, s);
            }
        }
    }
    else if (byte_index < edit->last_get_rule)
    {
        while (true)
        {
            syntax_marker_t *s;

            if (edit->syntax_marker == nullptr)
            {
                memset (&edit->rule, 0, sizeof (edit->rule));
                for (i = -1; i <= byte_index; i++)
                    apply_rules_going_right (edit, i);
                break;
            }

            s = (syntax_marker_t *) edit->syntax_marker->data;

            if (byte_index >= s->offset)
            {
                edit->rule = s->rule;
                for (i = s->offset + 1; i <= byte_index; i++)
                    apply_rules_going_right (edit, i);
                break;
            }

            g_free (s);
            edit->syntax_marker = g_slist_delete_link (edit->syntax_marker, edit->syntax_marker);
        }
    }
    edit->last_get_rule = byte_index;
}

/* --------------------------------------------------------------------------------------------- */

static int
translate_rule_to_color (const WEdit * edit, const edit_syntax_rule_t * rule)
{
    syntax_keyword_t *k;
    context_rule_t *r;

    r = CONTEXT_RULE (g_ptr_array_index (edit->rules, rule->context));
    k = SYNTAX_KEYWORD (g_ptr_array_index (r->keyword, rule->keyword));

    return k->color;
}

/* --------------------------------------------------------------------------------------------- */
/**
   Returns 0 on error/eof or a count of the number of bytes read
   including the newline. Result must be free'd.
   In case of an error, *line will not be modified.
 */

static size_t
read_one_line (char **line, FILE * f)
{
    GString *p;
    size_t r = 0;

    /* not reallocate string too often */
    p = g_string_sized_new (64);

    while (true)
    {
        int c;

        c = fgetc (f);
        if (c == EOF)
        {
            if (ferror (f))
            {
                if (errno == EINTR)
                    continue;
                r = 0;
            }
            break;
        }
        r++;

        /* handle all of \r\n, \r, \n correctly. */
        if (c == '\n')
            break;
        if (c == '\r')
        {
            c = fgetc (f);
            if (c == '\n')
                r++;
            else
                ungetc (c, f);
            break;
        }

        g_string_append_c (p, c);
    }
    if (r != 0)
        *line = g_string_free (p, false);
    else
        g_string_free (p, true);

    return r;
}

/* --------------------------------------------------------------------------------------------- */

static char *
convert (char *s)
{
    char *r, *p;

    p = r = s;
    while (*s)
    {
        switch (*s)
        {
        case '\\':
            s++;
            switch (*s)
            {
            case ' ':
                *p = ' ';
                s--;
                break;
            case 'n':
                *p = '\n';
                break;
            case 'r':
                *p = '\r';
                break;
            case 't':
                *p = '\t';
                break;
            case 's':
                *p = ' ';
                break;
            case '*':
                *p = '*';
                break;
            case '\\':
                *p = '\\';
                break;
            case '[':
            case ']':
                *p = SYNTAX_TOKEN_BRACKET;
                break;
            case '{':
            case '}':
                *p = SYNTAX_TOKEN_BRACE;
                break;
            case 0:
                *p = *s;
                return r;
            default:
                *p = *s;
                break;
            }
            break;
        case '*':
            *p = SYNTAX_TOKEN_STAR;
            break;
        case '+':
            *p = SYNTAX_TOKEN_PLUS;
            break;
        default:
            *p = *s;
            break;
        }
        s++;
        p++;
    }
    *p = '\0';
    return r;
}

/* --------------------------------------------------------------------------------------------- */

static int
get_args (char *l, char **args, int args_size)
{
    int argc = 0;

    while (argc < args_size)
    {
        char *p = l;

        while (*p != '\0' && whiteness (*p))
            p++;
        if (*p == '\0')
            break;
        for (l = p + 1; *l != '\0' && !whiteness (*l); l++)
            ;
        if (*l != '\0')
            *l++ = '\0';
        args[argc++] = convert (p);
    }
    args[argc] = (char *) nullptr;
    return argc;
}

/* --------------------------------------------------------------------------------------------- */

static int
this_try_alloc_color_pair (const char *fg, const char *bg, const char *attrs)
{
    char f[80], b[80], a[80], *p;

    if (bg != nullptr && *bg == '\0')
        bg = nullptr;
    if (fg != nullptr && *fg == '\0')
        fg = nullptr;
    if (attrs != nullptr && *attrs == '\0')
        attrs = nullptr;

    if ((fg == nullptr) && (bg == nullptr))
        return EDITOR_NORMAL_COLOR;

    if (fg != nullptr)
    {
        g_strlcpy (f, fg, sizeof (f));
        p = strchr (f, '/');
        if (p != nullptr)
            *p = '\0';
        fg = f;
    }
    if (bg != nullptr)
    {
        g_strlcpy (b, bg, sizeof (b));
        p = strchr (b, '/');
        if (p != nullptr)
            *p = '\0';
        bg = b;
    }
    if ((fg == nullptr) || (bg == nullptr))
    {
        /* get colors from skin */
        char *editnormal;

        editnormal = mc_skin_get ("editor", "_default_", "default;default");

        if (fg == nullptr)
        {
            g_strlcpy (f, editnormal, sizeof (f));
            p = strchr (f, ';');
            if (p != nullptr)
                *p = '\0';
            if (f[0] == '\0')
                g_strlcpy (f, "default", sizeof (f));
            fg = f;
        }
        if (bg == nullptr)
        {
            p = strchr (editnormal, ';');
            if ((p != nullptr) && (*(++p) != '\0'))
                g_strlcpy (b, p, sizeof (b));
            else
                g_strlcpy (b, "default", sizeof (b));
            bg = b;
        }

        g_free (editnormal);
    }

    if (attrs != nullptr)
    {
        g_strlcpy (a, attrs, sizeof (a));
        p = strchr (a, '/');
        if (p != nullptr)
            *p = '\0';
        /* get_args() mangles the + signs, unmangle 'em */
        p = a;
        while ((p = strchr (p, SYNTAX_TOKEN_PLUS)) != nullptr)
            *p++ = '+';
        attrs = a;
    }
    return tty_try_alloc_color_pair (fg, bg, attrs);
}

/* --------------------------------------------------------------------------------------------- */

static FILE *
open_include_file (const char *filename)
{
    FILE *f;

    g_free (error_file_name);
    error_file_name = g_strdup (filename);
    if (g_path_is_absolute (filename))
        return fopen (filename, "r");

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_config_get_data_path (), EDIT_HOME_DIR, filename, (char *) nullptr);
    f = fopen (error_file_name, "r");
    if (f != nullptr)
        return f;

    g_free (error_file_name);
    error_file_name = g_build_filename (mc_global.sysconfig_dir, "syntax", filename, (char *) nullptr);
    f = fopen (error_file_name, "r");
    if (f != nullptr)
        return f;

    g_free (error_file_name);
    error_file_name =
        g_build_filename (mc_global.share_data_dir, "syntax", filename, (char *) nullptr);

    return fopen (error_file_name, "r");
}

/* --------------------------------------------------------------------------------------------- */

inline static void
xx_lowerize_line (WEdit * edit, char *line, size_t len)
{
    if (edit->is_case_insensitive)
    {
        size_t i;

        for (i = 0; i < len; ++i)
            line[i] = tolower (line[i]);
    }
}

/* --------------------------------------------------------------------------------------------- */
/** returns line number on error */

static int
edit_read_syntax_rules (WEdit * edit, FILE * f, char **args, int args_size)
{
    FILE *g = nullptr;
    char *fg, *bg, *attrs;
    char last_fg[32] = "", last_bg[32] = "", last_attrs[64] = "";
    char whole_right[512];
    char whole_left[512];
    char *l = nullptr;
    int save_line = 0, line = 0;
    context_rule_t *c = nullptr;
    bool no_words = true;
    int result = 0;

    args[0] = nullptr;
    edit->is_case_insensitive = false;

    strcpy (whole_left, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");
    strcpy (whole_right, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_01234567890");

    edit->rules = g_ptr_array_new ();

    if (edit->defines == nullptr)
        edit->defines = g_tree_new ((GCompareFunc) strcmp);

    while (true)
    {
        char **a;
        size_t len;
        int argc;

        line++;
        l = nullptr;

        len = read_one_line (&l, f);
        if (len != 0)
            xx_lowerize_line (edit, l, len);
        else
        {
            if (g == nullptr)
                break;

            fclose (f);
            f = g;
            g = nullptr;
            line = save_line + 1;
            MC_PTR_FREE (error_file_name);
            MC_PTR_FREE (l);
            len = read_one_line (&l, f);
            if (len == 0)
                break;
            xx_lowerize_line (edit, l, len);
        }

        argc = get_args (l, args, args_size);
        a = args + 1;
        if (args[0] == nullptr)
        {
            /* do nothing */
        }
        else if (strcmp (args[0], "include") == 0)
        {
            if (g != nullptr || argc != 2)
            {
                result = line;
                break;
            }
            g = f;
            f = open_include_file (args[1]);
            if (f == nullptr)
            {
                MC_PTR_FREE (error_file_name);
                result = line;
                break;
            }
            save_line = line;
            line = 0;
        }
        else if (strcmp (args[0], "caseinsensitive") == 0)
        {
            edit->is_case_insensitive = true;
        }
        else if (strcmp (args[0], "wholechars") == 0)
        {
            check_a;
            if (strcmp (*a, "left") == 0)
            {
                a++;
                g_strlcpy (whole_left, *a, sizeof (whole_left));
            }
            else if (strcmp (*a, "right") == 0)
            {
                a++;
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            else
            {
                g_strlcpy (whole_left, *a, sizeof (whole_left));
                g_strlcpy (whole_right, *a, sizeof (whole_right));
            }
            a++;
            check_not_a;
        }
        else if (strcmp (args[0], "context") == 0)
        {
            syntax_keyword_t *k;

            check_a;
            if (edit->rules->len == 0)
            {
                /* first context is the default */
                if (strcmp (*a, "default") != 0)
                    break_a;

                a++;
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (edit->rules, c);
                c->left = g_strdup (" ");
                c->right = g_strdup (" ");
            }
            else
            {
                /* Start new context.  */
                c = g_new0 (context_rule_t, 1);
                g_ptr_array_add (edit->rules, c);
                if (strcmp (*a, "exclusive") == 0)
                {
                    a++;
                    c->between_delimiters = true;
                }
                check_a;
                if (strcmp (*a, "whole") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                else if (strcmp (*a, "wholeleft") == 0)
                {
                    a++;
                    c->whole_word_chars_left = g_strdup (whole_left);
                }
                else if (strcmp (*a, "wholeright") == 0)
                {
                    a++;
                    c->whole_word_chars_right = g_strdup (whole_right);
                }
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_left = true;
                }
                check_a;
                c->left = g_strdup (*a++);
                check_a;
                if (strcmp (*a, "linestart") == 0)
                {
                    a++;
                    c->line_start_right = true;
                }
                check_a;
                c->right = g_strdup (*a++);
                c->first_left = *c->left;
                c->first_right = *c->right;
            }
            c->keyword = g_ptr_array_new ();
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (c->keyword, k);
            no_words = false;
            subst_defines (edit->defines, a, &args[ARGS_LEN]);
            fg = *a;
            if (*a != nullptr)
                a++;
            bg = *a;
            if (*a != nullptr)
                a++;
            attrs = *a;
            if (*a != nullptr)
                a++;
            g_strlcpy (last_fg, fg != nullptr ? fg : "", sizeof (last_fg));
            g_strlcpy (last_bg, bg != nullptr ? bg : "", sizeof (last_bg));
            g_strlcpy (last_attrs, attrs != nullptr ? attrs : "", sizeof (last_attrs));
            k->color = this_try_alloc_color_pair (fg, bg, attrs);
            k->keyword = g_strdup (" ");
            check_not_a;
        }
        else if (strcmp (args[0], "spellcheck") == 0)
        {
            if (c == nullptr)
            {
                result = line;
                break;
            }
            c->spelling = true;
        }
        else if (strcmp (args[0], "keyword") == 0)
        {
            context_rule_t *last_rule;
            syntax_keyword_t *k;

            if (no_words)
                break_a;
            check_a;
            last_rule = CONTEXT_RULE (g_ptr_array_index (edit->rules, edit->rules->len - 1));
            k = g_new0 (syntax_keyword_t, 1);
            g_ptr_array_add (last_rule->keyword, k);
            if (strcmp (*a, "whole") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            else if (strcmp (*a, "wholeleft") == 0)
            {
                a++;
                k->whole_word_chars_left = g_strdup (whole_left);
            }
            else if (strcmp (*a, "wholeright") == 0)
            {
                a++;
                k->whole_word_chars_right = g_strdup (whole_right);
            }
            check_a;
            if (strcmp (*a, "linestart") == 0)
            {
                a++;
                k->line_start = true;
            }
            check_a;
            if (strcmp (*a, "whole") == 0)
                break_a;

            k->keyword = g_strdup (*a++);
            subst_defines (edit->defines, a, &args[ARGS_LEN]);
            fg = *a;
            if (*a != nullptr)
                a++;
            bg = *a;
            if (*a != nullptr)
                a++;
            attrs = *a;
            if (*a != nullptr)
                a++;
            if (fg == nullptr)
                fg = last_fg;
            if (bg == nullptr)
                bg = last_bg;
            if (attrs == nullptr)
                attrs = last_attrs;
            k->color = this_try_alloc_color_pair (fg, bg, attrs);
            check_not_a;
        }
        else if (*(args[0]) == '#')
        {
            /* do nothing for comment */
        }
        else if (strcmp (args[0], "file") == 0)
        {
            break;
        }
        else if (strcmp (args[0], "define") == 0)
        {
            char *key = *a++;

            if (argc < 3)
                break_a;
            char **argv = static_cast<char **> (g_tree_lookup (edit->defines, key));
            if (argv != nullptr)
                mc_defines_destroy (nullptr, argv, nullptr);
            else
                key = g_strdup (key);

            argv = g_new (char *, argc - 1);
            g_tree_insert (edit->defines, key, argv);
            while (*a != nullptr)
                *argv++ = g_strdup (*a++);
            *argv = nullptr;
        }
        else
        {
            /* anything else is an error */
            break_a;
        }
        MC_PTR_FREE (l);
    }
    MC_PTR_FREE (l);

    if (edit->rules->len == 0)
    {
        g_ptr_array_free (edit->rules, true);
        edit->rules = nullptr;
    }

    if (result == 0)
    {
        size_t i;
        GString *first_chars;

        if (edit->rules == nullptr)
            return line;

        first_chars = g_string_sized_new (32);

        /* collect first character of keywords */
        for (i = 0; i < edit->rules->len; i++)
        {
            size_t j;

            g_string_set_size (first_chars, 0);
            c = CONTEXT_RULE (g_ptr_array_index (edit->rules, i));

            g_string_append_c (first_chars, (char) 1);
            for (j = 1; j < c->keyword->len; j++)
            {
                syntax_keyword_t *k;

                k = SYNTAX_KEYWORD (g_ptr_array_index (c->keyword, j));
                g_string_append_c (first_chars, k->keyword[0]);
            }

            c->keyword_first_chars = g_strndup (first_chars->str, first_chars->len);
        }

        g_string_free (first_chars, true);
    }

    return result;
}

/* --------------------------------------------------------------------------------------------- */

/* returns -1 on file error, line number on error in file syntax */
static int
edit_read_syntax_file (WEdit * edit, GPtrArray * pnames, const char *syntax_file,
                       const char *editor_file, const char *first_line, const char *type)
{
    FILE *f, *g = nullptr;
    char *args[ARGS_LEN], *l = nullptr;
    long line = 0;
    int result = 0;
    char *lib_file;
    bool found = false;

    f = fopen (syntax_file, "r");
    if (f == nullptr)
    {
        lib_file = g_build_filename (mc_global.share_data_dir, "syntax", "Syntax", (char *) nullptr);
        f = fopen (lib_file, "r");
        g_free (lib_file);
        if (f == nullptr)
            return -1;
    }

    args[0] = nullptr;
    while (true)
    {
        line++;
        MC_PTR_FREE (l);
        if (read_one_line (&l, f) == 0)
            break;
        (void) get_args (l, args, ARGS_LEN - 1);        /* Final nullptr */
        if (args[0] == nullptr)
            continue;

        /* Looking for 'include ...' lines before first 'file ...' ones */
        if (!found && strcmp (args[0], "include") == 0)
        {
            if (g != nullptr)
                continue;

            if (args[1] == nullptr || (g = open_include_file (args[1])) == nullptr)
            {
                result = line;
                break;
            }
            goto found_type;
        }

        /* looking for 'file ...' lines only */
        if (strcmp (args[0], "file") != 0)
            continue;

        found = true;

        /* must have two args or report error */
        if (args[1] == nullptr || args[2] == nullptr)
        {
            result = line;
            break;
        }

        if (pnames != nullptr)
        {
            /* 1: just collecting a list of names of rule sets */
            g_ptr_array_add (pnames, g_strdup (args[2]));
        }
        else if (type != nullptr)
        {
            /* 2: rule set was explicitly specified by the caller */
            if (strcmp (type, args[2]) == 0)
                goto found_type;
        }
        else if (editor_file != nullptr && edit != nullptr)
        {
            /* 3: auto-detect rule set from regular expressions */
            bool q;

            q = mc_search (args[1], DEFAULT_CHARSET, editor_file, MC_SEARCH_T_REGEX);
            /* does filename match arg 1 ? */
            if (!q && args[3] != nullptr)
            {
                /* does first line match arg 3 ? */
                q = mc_search (args[3], DEFAULT_CHARSET, first_line, MC_SEARCH_T_REGEX);
            }
            if (q)
            {
                int line_error;
                char *syntax_type;

              found_type:
                syntax_type = args[2];
                line_error = edit_read_syntax_rules (edit, g ? g : f, args, ARGS_LEN - 1);
                if (line_error != 0)
                {
                    if (error_file_name == nullptr)        /* an included file */
                        result = line + line_error;
                    else
                        result = line_error;
                }
                else
                {
                    g_free (edit->syntax_type);
                    edit->syntax_type = g_strdup (syntax_type);
                    /* if there are no rules then turn off syntax highlighting for speed */
                    if (g == nullptr && edit->rules->len == 1)
                    {
                        context_rule_t *r0;

                        r0 = CONTEXT_RULE (g_ptr_array_index (edit->rules, 0));
                        if (r0->keyword->len == 1 && !r0->spelling)
                        {
                            edit_free_syntax_rules (edit);
                            break;
                        }
                    }
                }

                if (g == nullptr)
                    break;

                fclose (g);
                g = nullptr;
            }
        }
    }
    g_free (l);
    fclose (f);
    return result;
}

/* --------------------------------------------------------------------------------------------- */

static const char *
get_first_editor_line (WEdit * edit)
{
    static char s[256];

    s[0] = '\0';

    if (edit != nullptr)
    {
        size_t i;

        for (i = 0; i < sizeof (s) - 1; i++)
        {
            s[i] = edit_buffer_get_byte (&edit->buffer, i);
            if (s[i] == '\n')
            {
                s[i] = '\0';
                break;
            }
        }

        s[sizeof (s) - 1] = '\0';
    }

    return s;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

int
edit_get_syntax_color (WEdit * edit, off_t byte_index)
{
    if (!tty_use_colors ())
        return 0;

    if (edit->rules != nullptr && byte_index < edit->buffer.size && option_syntax_highlighting)
    {
        edit_get_rule (edit, byte_index);
        return translate_rule_to_color (edit, &edit->rule);
    }

    return EDITOR_NORMAL_COLOR;
}

/* --------------------------------------------------------------------------------------------- */

void
edit_free_syntax_rules (WEdit * edit)
{
    if (edit == nullptr)
        return;

    if (edit->defines != nullptr)
        destroy_defines (&edit->defines);

    if (edit->rules == nullptr)
        return;

    edit_get_rule (edit, -1);
    MC_PTR_FREE (edit->syntax_type);

    g_ptr_array_foreach (edit->rules, (GFunc) context_rule_free, nullptr);
    g_ptr_array_free (edit->rules, true);
    edit->rules = nullptr;
    g_clear_slist (&edit->syntax_marker, g_free);
    tty_color_free_all_tmp ();
}

/* --------------------------------------------------------------------------------------------- */
/**
 * Load rules into edit struct.  Either edit or *pnames must be nullptr.  If
 * edit is nullptr, a list of types will be stored into names.  If type is
 * nullptr, then the type will be selected according to the filename.
 * type must be edit->syntax_type or nullptr
 */
void
edit_load_syntax (WEdit * edit, GPtrArray * pnames, const char *type)
{
    int r;
    char *f = nullptr;

    if (option_auto_syntax)
        type = nullptr;

    if (edit != nullptr)
    {
        char *saved_type;

        saved_type = g_strdup (type);   /* save edit->syntax_type */
        edit_free_syntax_rules (edit);
        edit->syntax_type = saved_type; /* restore edit->syntax_type */
    }

    if (!tty_use_colors ())
        return;

    if (!option_syntax_highlighting && (pnames == nullptr || pnames->len == 0))
        return;

    if (edit != nullptr && edit->filename_vpath == nullptr)
        return;

    f = mc_config_get_full_path (EDIT_HOME_SYNTAX_FILE);
    if (edit != nullptr)
        r = edit_read_syntax_file (edit, pnames, f, vfs_path_as_str (edit->filename_vpath),
                                   get_first_editor_line (edit),
                                   option_auto_syntax ? nullptr : edit->syntax_type);
    else
        r = edit_read_syntax_file (nullptr, pnames, f, nullptr, "", nullptr);
    if (r == -1)
    {
        edit_free_syntax_rules (edit);
        message (D_ERROR, _("Load syntax file"),
                 _("Cannot open file %s\n%s"), f, unix_error_string (errno));
    }
    else if (r != 0)
    {
        edit_free_syntax_rules (edit);
        message (D_ERROR, _("Load syntax file"),
                 _("Error in file %s on line %d"), error_file_name != nullptr ? error_file_name : f,
                 r);
        MC_PTR_FREE (error_file_name);
    }

    g_free (f);
}

/* --------------------------------------------------------------------------------------------- */

const char *
edit_get_syntax_type (const WEdit * edit)
{
    return edit->syntax_type;
}

/* --------------------------------------------------------------------------------------------- */
