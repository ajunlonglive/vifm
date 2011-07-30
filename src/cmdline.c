/* vifm
 * Copyright (C) 2001 Ken Steen.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#define _GNU_SOURCE /* I don't know how portable this is but it is
					   needed in Linux for the ncurses wide char
					   functions
					   */

#include <curses.h>

#include <dirent.h> /* DIR */
#include <limits.h>

#include <wchar.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "../config.h"

#include "bookmarks.h"
#include "cmds.h"
#include "color_scheme.h"
#include "commands.h"
#include "completion.h"
#include "config.h"
#include "filelist.h"
#include "keys.h"
#include "menu.h"
#include "menus.h"
#include "modes.h"
#include "options.h"
#include "permissions_dialog.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "utils.h"
#include "visual.h"

#include "cmdline.h"

#ifndef TEST

struct line_stats
{
	wchar_t *line;            /* the line reading */
	int index;                /* index of the current character */
	int curs_pos;             /* position of the cursor */
	int len;                  /* length of the string */
	int cmd_pos;              /* position in the history */
	wchar_t prompt[NAME_MAX]; /* prompt */
	int prompt_wid;           /* width of prompt */
	int complete_continue;    /* if non-zero, continue the previous completion */
	int history_search;       /* 0 - none, 1 - <c-n>/<c-p>, 2 - <down>/<up> */
	int hist_search_len;      /* length of history search pattern */
	wchar_t *line_buf;        /* content of line before using history */
};

#endif

static int *mode;
static int prev_mode;
static enum CmdLineSubModes sub_mode;
static struct line_stats input_stat;
static int line_width = 1;
static void *sub_mode_ptr;
static char **paths;
static int paths_count;

static void split_path(void);
static int def_handler(wchar_t key);
static void update_cmdline_size(void);
static void update_cmdline_text(void);
static wchar_t * wcsins(wchar_t *src, wchar_t *ins, int pos);
static void prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd);
static void leave_cmdline_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_h(struct key_info, struct keys_info *);
static void cmd_ctrl_i(struct key_info, struct keys_info *);
static void cmd_shift_tab(struct key_info, struct keys_info *);
static void do_completion(void);
static void draw_wild_menu(int op);
static void cmd_ctrl_k(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void cmd_ctrl_n(struct key_info, struct keys_info *);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_down(struct key_info, struct keys_info *);
#endif /* ENABLE_EXTENDED_KEYS */
static void cmd_ctrl_u(struct key_info, struct keys_info *);
static void cmd_ctrl_w(struct key_info, struct keys_info *);
static void cmd_meta_b(struct key_info, struct keys_info *);
static void find_prev_word(void);
static void cmd_meta_d(struct key_info, struct keys_info *);
static void cmd_meta_f(struct key_info, struct keys_info *);
static void find_next_word(void);
static void cmd_left(struct key_info, struct keys_info *);
static void cmd_right(struct key_info, struct keys_info *);
static void cmd_home(struct key_info, struct keys_info *);
static void cmd_end(struct key_info, struct keys_info *);
static void cmd_delete(struct key_info, struct keys_info *);
static void update_cmdline(void);
static void complete_cmd_next(void);
static void complete_search_next(void);
static void cmd_ctrl_p(struct key_info, struct keys_info *);
#ifdef ENABLE_EXTENDED_KEYS
static void cmd_up(struct key_info, struct keys_info *);
#endif /* ENABLE_EXTENDED_KEYS */
static void complete_cmd_prev(void);
static void complete_search_prev(void);
#ifndef TEST
static
#endif
int line_completion(struct line_stats *stat);
static void update_line_stat(struct line_stats *stat, int new_len);
static wchar_t * wcsdel(wchar_t *src, int pos, int len);
void filename_completion(const char *str, int type);
static void stop_completion(void);

static struct keys_add_info builtin_cmds[] = {
	{L"\x03",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* backspace */
	{L"\x08",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{L"\x09",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_i}}},
	{L"\x0b",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_k}}},
	{L"\x0d",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_n}}},
	{L"\x10",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_p}}},
	/* escape */
	{L"\x1b",         {BUILDIN_WAIT_POINT, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* escape escape */
	{L"\x1b\x1b",     {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* ascii Delete */
	{L"\x7f",         {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_BACKSPACE}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_h}}},
	{{KEY_DOWN},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_down}}},
	{{KEY_UP},        {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_up}}},
	{{KEY_LEFT},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	{{KEY_RIGHT},     {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	{{KEY_HOME},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	{{KEY_END},       {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
	{{KEY_DC},        {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
	{{KEY_BTAB},      {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
#endif /* ENABLE_EXTENDED_KEYS */
	{L"\x1b"L"[Z",    {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_shift_tab}}},
	/* ctrl b */
	{L"\x02", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_left}}},
	/* ctrl f */
	{L"\x06", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_right}}},
	/* ctrl a */
	{L"\x01", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_home}}},
	/* ctrl e */
	{L"\x05", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_end}}},
	/* ctrl d */
	{L"\x04", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_delete}}},
	{L"\x15", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_u}}},
	{L"\x17", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_w}}},
	{L"\x1b"L"b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_b}}},
	{L"\x1b"L"d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_d}}},
	{L"\x1b"L"f", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_meta_f}}},
};

void
init_cmdline_mode(int *key_mode)
{
	assert(key_mode != NULL);

	mode = key_mode;
	set_def_handler(CMDLINE_MODE, def_handler);

	assert(add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), CMDLINE_MODE) == 0);

	split_path();
}

static char *
expand_tilda(const char *path)
{
	char *result;

	result = malloc((strlen(cfg.home_dir) + strlen(path) + 1));
	if(result == NULL)
		return NULL;

	sprintf(result, "%s/%s", cfg.home_dir, path + 2);

	return result;
}

static void
split_path(void)
{
	char *path, *p, *q;
	int i;

	path = getenv("PATH");

	paths_count = 1;
	p = path;
	while((p = strchr(p, ':')) != NULL)
	{
		paths_count++;
		p++;
	}

	paths = malloc(paths_count*sizeof(paths[0]));
	if(paths == NULL)
		return;

	i = 0;
	p = path - 1;
	do
	{
		int j;
		char *s;

		p++;
		q = strchr(p, ':');
		if(q == NULL)
		{
			q = p + strlen(p);
		}

		s = malloc((q - p + 1)*sizeof(s[0]));
		if(s == NULL)
		{
			for(j = 0; j < i - 1; j++)
				free(paths[j]);
			paths_count = 0;
			return;
		}
		snprintf(s, q - p + 1, "%s", p);

		p = q;

		if(strncmp(s, "~/", 2) == 0)
		{
			char *t;
			t = expand_tilda(s);
			free(s);
			s = t;
		}

		if(access(s, F_OK) != 0)
		{
			free(s);
			continue;
		}

		paths[i++] = s;

		for(j = 0; j < i - 1; j++)
		{
			if(strcmp(paths[j], s) == 0)
			{
				free(s);
				i--;
				break;
			}
		}
	} while (q[0] != '\0');
	paths_count = i;
}

static int
def_handler(wchar_t key)
{
	void *p;
	wchar_t buf[2] = {key, L'\0'};

	input_stat.history_search = 0;

	if(input_stat.complete_continue
			&& input_stat.line[input_stat.index - 1] == L'/' && key == '/')
	{
		stop_completion();
		return 0;
	}

	stop_completion();

	if(key != L'\r' && !iswprint(key))
		return 0;

	p = realloc(input_stat.line, (input_stat.len + 2) * sizeof(wchar_t));
	if(p == NULL)
	{
		leave_cmdline_mode();
		return 0;
	}

	input_stat.line = (wchar_t *) p;

	if(input_stat.len == 0)
		input_stat.line[0] = L'\0';

	input_stat.index++;
	wcsins(input_stat.line, buf, input_stat.index);
	input_stat.len++;

	input_stat.curs_pos += wcwidth(key);

	update_cmdline_size();
	update_cmdline_text();

	return 0;
}

static void
update_cmdline_size(void)
{
	int d;

	d = (input_stat.prompt_wid + input_stat.len + 1 + line_width - 1)/line_width;
	if(d >= getmaxy(status_bar))
	{
		int y = getmaxy(stdscr);

		mvwin(status_bar, y - d, 0);
		wresize(status_bar, d, line_width);

		if(prev_mode != MENU_MODE)
		{
			mvwin(stat_win, y - d - 1, 0);
			wrefresh(stat_win);
		}
		else
		{
			wresize(menu_win, y - d, getmaxx(stdscr));
			update_menu();
			wrefresh(menu_win);
		}
	}
}

static void
update_cmdline_text(void)
{
	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	if(input_stat.line != NULL)
		mvwaddwstr(status_bar, input_stat.prompt_wid/line_width,
				input_stat.prompt_wid%line_width, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
	wrefresh(status_bar);
}

/* Insert a string into another string
 * For example, wcsins(L"foor", L"ba", 4) returns L"foobar"
 * If pos is larger then wcslen(src), the character will
 * be added at the end of the src */
static wchar_t *
wcsins(wchar_t *src, wchar_t *ins, int pos)
{
	int i;
	wchar_t *p;

	pos--;

	for (p = src + pos; *p; p++)
		;

	for (i = 0; ins[i]; i++)
		;

	for (; p >= src + pos; p--)
		*(p + i) = *p;
	p++;

	for (; *ins; ins++, p++)
		*p = *ins;

	return src;
}

void
enter_cmdline_mode(enum CmdLineSubModes cl_sub_mode, const wchar_t *cmd,
		void *ptr)
{
	const wchar_t *prompt;

	sub_mode_ptr = ptr;
	sub_mode = cl_sub_mode;

	if(sub_mode == CMD_SUBMODE || sub_mode == MENU_CMD_SUBMODE)
		prompt = L":";
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_FORWARD_SUBMODE)
		prompt = L"/";
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
		prompt = L"?";
	else
		prompt = L"E";

	prepare_cmdline_mode(prompt, cmd);
}

void
enter_prompt_mode(const wchar_t *prompt, const char *cmd, prompt_cb cb)
{
	wchar_t *buf;

	sub_mode_ptr = cb;
	sub_mode = PROMPT_SUBMODE;

	buf = to_wide(cmd);
	if(buf == NULL)
		return;

	prepare_cmdline_mode(prompt, buf);
	free(buf);
}

void
redraw_cmdline(void)
{
	if(prev_mode == MENU_MODE)
	{
		menu_redraw();
	}
	else
	{
		redraw_window();
		if(prev_mode == SORT_MODE)
			redraw_sort_dialog();
		else if(prev_mode == PERMISSIONS_MODE)
			redraw_permissions_dialog();
	}

	line_width = getmaxx(stdscr);
	curs_set(TRUE);
	update_cmdline_size();
	update_cmdline_text();

	if(cfg.wild_menu)
		draw_wild_menu(-1);
}

static void
prepare_cmdline_mode(const wchar_t *prompt, const wchar_t *cmd)
{
	line_width = getmaxx(stdscr);
	prev_mode = *mode;
	*mode = CMDLINE_MODE;

	input_stat.line = NULL;
	input_stat.index = wcslen(cmd);
	input_stat.curs_pos = 0;
	input_stat.len = input_stat.index;
	input_stat.cmd_pos = -1;
	input_stat.complete_continue = 0;
	input_stat.history_search = 0;
	input_stat.line_buf = NULL;

	wcsncpy(input_stat.prompt, prompt, ARRAY_LEN(input_stat.prompt));
	input_stat.prompt_wid = input_stat.curs_pos = wcslen(input_stat.prompt);

	if(input_stat.len != 0)
	{
		input_stat.line = malloc(sizeof(wchar_t)*(input_stat.len + 1));
		if(input_stat.line == NULL)
		{
			input_stat.index = 0;
			input_stat.len = 0;
		}
		else
		{
			wcscpy(input_stat.line, cmd);
			input_stat.curs_pos += input_stat.len;
		}
	}

	curs_set(1);

	update_cmdline_size();
	update_cmdline_text();

	curr_stats.save_msg = 1;
}

static void
leave_cmdline_mode(void)
{
	if(getmaxy(status_bar) > 1)
	{
		curr_stats.need_redraw = 2;
		wresize(status_bar, 1, getmaxx(stdscr) - 19);
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		if(prev_mode == MENU_MODE)
		{
			wresize(menu_win, getmaxy(stdscr) - 1, getmaxx(stdscr));
			update_menu();
		}
	}
	else
	{
		wresize(status_bar, 1, getmaxx(stdscr) - 19);
	}

	curs_set(0);
	curr_stats.save_msg = 0;
	free(input_stat.line);
	free(input_stat.line_buf);
	clean_status_bar();

	if(*mode == CMDLINE_MODE)
		*mode = prev_mode;

	update_pos_window(curr_view);
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	leave_cmdline_mode();

	if(prev_mode == VISUAL_MODE)
	{
		leave_visual_mode(curr_stats.save_msg);
		moveto_list_pos(curr_view, check_mark_directory(curr_view, '<'));
	}
}

static void
cmd_ctrl_h(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index == 0 && input_stat.len == 0 && sub_mode != PROMPT_SUBMODE)
	{
		cmd_ctrl_c(key_info, keys_info);
		return;
	}
	if(input_stat.index == 0)
		return;

	if(input_stat.index == input_stat.len)
	{ /* If the cursor is at the end of the line, maybe filling
		 * spaces by ourselves would goes faster then repaint
		 * the whole window entirely :-) */

		int w, i;

		input_stat.index--;
		input_stat.len--;

		i = input_stat.curs_pos;
		w = wcwidth(input_stat.line[input_stat.index]);
		while(i - input_stat.curs_pos < w)
		{
			mvwaddch(status_bar, input_stat.curs_pos/line_width,
					input_stat.curs_pos%line_width, ' ');
			input_stat.curs_pos--;
		}
		mvwaddch(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width, ' ');

		input_stat.line[input_stat.index] = L'\0';
	}
	else
	{
		input_stat.index--;
		input_stat.len--;

		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wcsdel(input_stat.line, input_stat.index + 1, 1);

		werase(status_bar);
		mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
		mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
	}

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_ctrl_i(struct key_info key_info, struct keys_info *keys_info)
{
	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	set_completion_order(0);
	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);
}

static void
cmd_shift_tab(struct key_info key_info, struct keys_info *keys_info)
{
	if(!input_stat.complete_continue)
		draw_wild_menu(1);
	set_completion_order(1);
	do_completion();
	if(cfg.wild_menu)
		draw_wild_menu(0);
}

static void
do_completion(void)
{
	if(sub_mode != CMD_SUBMODE)
		return;

	if(input_stat.line == NULL)
	{
		input_stat.line = my_wcsdup(L"");
		if(input_stat.line == NULL)
			return;
	}

	line_completion(&input_stat);

	update_cmdline_size();
	update_cmdline_text();
}

/*
 * op == 0 - draw
 * op < 0 - redraw
 * op > 0 - reset
 */
static void
draw_wild_menu(int op)
{
	static int last_pos;

	const char ** list = get_completion_list();
	int pos = get_completion_pos();
	int count = get_completion_count() - 1;
	int i;
	int len = getmaxx(stdscr);
	
	if(count < 2)
		return;

	if(op > 0)
	{
		last_pos = 0;
		return;
	}

	if(pos == 0 || pos == count)
		last_pos = 0;
	if(last_pos == 0 && pos == count - 1)
		last_pos = count;
	if(pos < last_pos)
	{
		int l = len;
		while(last_pos > 0 && l > 2)
		{
			last_pos--;
			l -= strlen(list[last_pos]);
			if(last_pos != 0)
				l -= 2;
		}
		if(l < 2)
			last_pos++;
	}

	wclear(stat_win);
	wmove(stat_win, 0, 0);

	wattron(stat_win, A_BOLD);
	for(i = last_pos; i < count && len > 0; i++)
	{
		len -= strlen(list[i]);
		if(i != 0)
			len -= 2;

		if(i == last_pos && last_pos > 0)
		{
			wprintw(stat_win, "< ");
		}
		else if(i > last_pos)
		{
			if(len < 2)
			{
				wprintw(stat_win, " >");
				break;
			}
			wprintw(stat_win, "  ");
		}

		if(i == pos)
			wattron(stat_win, COLOR_PAIR(MENU_COLOR + cfg.color_scheme) |
					A_UNDERLINE | A_REVERSE);
		wprintw(stat_win, "%s", list[i]);
		if(i == pos)
		{
			wattroff(stat_win, COLOR_PAIR(MENU_COLOR + cfg.color_scheme) |
					A_UNDERLINE | A_REVERSE);
			pos = -pos;
		}
	}
	wattroff(stat_win, A_BOLD);
	if(pos > 0 && pos != count)
	{
		last_pos = pos;
		draw_wild_menu(op);
		return;
	}
	if(op == 0 && len < 2 && i - 1 == pos)
		last_pos = i;
	wrefresh(stat_win);
}

static void
cmd_ctrl_k(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index == input_stat.len)
		return;

	wcsdel(input_stat.line, input_stat.index + 1,
			input_stat.len - input_stat.index);
	input_stat.len = input_stat.index;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char* p;
	int save_hist = !keys_info->mapped;

	stop_completion();
	werase(status_bar);
	wnoutrefresh(status_bar);

	if((!input_stat.line || !input_stat.line[0]) && (sub_mode == CMD_SUBMODE ||
			sub_mode == MENU_CMD_SUBMODE))
	{
		leave_cmdline_mode();
		return;
	}

	p = input_stat.line ? to_multibyte(input_stat.line) : NULL;

	leave_cmdline_mode();

	if(sub_mode == CMD_SUBMODE)
	{
		char* s = p;
		while(*s == ' ' || *s == ':')
			s++;
		curr_stats.save_msg = exec_commands(s, curr_view, GET_COMMAND, save_hist);
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_command(p, curr_view, GET_FSEARCH_PATTERN);
	}
	else if(sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.save_msg = exec_command(p, curr_view, GET_BSEARCH_PATTERN);
	}
	else if(sub_mode == MENU_CMD_SUBMODE)
	{
		execute_menu_command(p, sub_mode_ptr);
	}
	else if(sub_mode == MENU_SEARCH_FORWARD_SUBMODE
			|| sub_mode == MENU_SEARCH_BACKWARD_SUBMODE)
	{
		curr_stats.need_redraw = 1;
		search_menu_list(p, sub_mode_ptr);
	}
	else if(sub_mode == PROMPT_SUBMODE)
	{
		prompt_cb cb;

		cb = (prompt_cb)sub_mode_ptr;
		cb(p);
	}

	if(prev_mode == VISUAL_MODE)
		leave_visual_mode(curr_stats.save_msg);

	free(p);
}

static void
cmd_ctrl_n(struct key_info key_info, struct keys_info *keys_info)
{
	stop_completion();

	if(input_stat.history_search == 0)
	{
		free(input_stat.line_buf);
		input_stat.line_buf = my_wcsdup(input_stat.line != NULL ?
				input_stat.line : L"");
	}

	input_stat.history_search = 1;

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_next();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_next();
	}
}

#ifdef ENABLE_EXTENDED_KEYS
static void
cmd_down(struct key_info key_info, struct keys_info *keys_info)
{
	stop_completion();

	if(input_stat.history_search == 0)
	{
		free(input_stat.line_buf);
		input_stat.line_buf = my_wcsdup(input_stat.line != NULL ?
				input_stat.line : L"");
	}

	if(input_stat.history_search != 2)
	{
		input_stat.history_search = 2;
		input_stat.hist_search_len = input_stat.len;
	}

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_next();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_next();
	}
}
#endif /* ENABLE_EXTENDED_KEYS */

static void
cmd_ctrl_u(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index == 0)
		return;

	input_stat.len -= input_stat.index;

	input_stat.curs_pos = input_stat.prompt_wid;
	wcsdel(input_stat.line, 1, input_stat.index);

	input_stat.index = 0;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_ctrl_w(struct key_info key_info, struct keys_info *keys_info)
{
	int old;

	input_stat.history_search = 0;
	stop_completion();

	old = input_stat.index;
	find_prev_word();

	if(input_stat.index != old)
	{
		wcsdel(input_stat.line, input_stat.index + 1, old - input_stat.index);
		input_stat.len -= old - input_stat.index;
	}

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_meta_b(struct key_info key_info, struct keys_info *keys_info)
{
	find_prev_word();
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
find_prev_word(void)
{
	while(input_stat.index > 0 && isspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
	while(input_stat.index > 0 && !isspace(input_stat.line[input_stat.index - 1]))
	{
		input_stat.index--;
		input_stat.curs_pos--;
	}
}

static void
cmd_meta_d(struct key_info key_info, struct keys_info *keys_info)
{
	int old_i, old_c;

	old_i = input_stat.index;
	old_c = input_stat.curs_pos;
	find_next_word();

	if(input_stat.index == old_i)
	{
		return;
	}

	wcsdel(input_stat.line, old_i + 1, input_stat.index - old_i);
	input_stat.len -= input_stat.index - old_i;
	input_stat.index = old_i;
	input_stat.curs_pos = old_c;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	waddwstr(status_bar, input_stat.line);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_meta_f(struct key_info key_info, struct keys_info *keys_info)
{
	find_next_word();
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
find_next_word(void)
{
	while(input_stat.index < input_stat.len
			&& isspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
	while(input_stat.index < input_stat.len
			&& !isspace(input_stat.line[input_stat.index]))
	{
		input_stat.index++;
		input_stat.curs_pos++;
	}
}

static void
cmd_left(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index > 0)
	{
		input_stat.index--;
		input_stat.curs_pos -= wcwidth(input_stat.line[input_stat.index]);
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
cmd_right(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index < input_stat.len)
	{
		input_stat.curs_pos += wcwidth(input_stat.line[input_stat.index]);
		input_stat.index++;
		wmove(status_bar, input_stat.curs_pos/line_width,
				input_stat.curs_pos%line_width);
	}
}

static void
cmd_home(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = 0;
	input_stat.curs_pos = wcslen(input_stat.prompt);
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_end(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.index = input_stat.len;
	input_stat.curs_pos = input_stat.prompt_wid + input_stat.len;
	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

static void
cmd_delete(struct key_info key_info, struct keys_info *keys_info)
{
	input_stat.history_search = 0;
	stop_completion();

	if(input_stat.index == input_stat.len)
		return;

	wcsdel(input_stat.line, input_stat.index+1, 1);
	input_stat.len--;

	werase(status_bar);
	mvwaddwstr(status_bar, 0, 0, input_stat.prompt);
	mvwaddwstr(status_bar, 0, input_stat.prompt_wid, input_stat.line);

	wmove(status_bar, input_stat.curs_pos/line_width,
			input_stat.curs_pos%line_width);
}

/* Returns 0 on success */
static int
replace_input_line(const char *new)
{
	size_t len;
	wchar_t *p;

	len = mbstowcs(NULL, new, 0);
	p = realloc(input_stat.line, (len + 1)*sizeof(wchar_t));
	if(p == NULL)
		return -1;

	input_stat.line = p;
	input_stat.len = len;
	mbstowcs(input_stat.line, new, len + 1);
	return 0;
}

static void
complete_cmd_prev(void)
{
	if(cfg.cmd_history_num < 0)
		return;

	if(input_stat.history_search != 2)
	{
		if(input_stat.cmd_pos == cfg.cmd_history_num)
			return;
		input_stat.cmd_pos++;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(++pos <= cfg.cmd_history_num)
		{
			wchar_t *buf;

			buf = to_wide(cfg.cmd_history[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos > cfg.cmd_history_num)
			return;
		input_stat.cmd_pos = pos;
	}

	replace_input_line(cfg.cmd_history[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos >= cfg.cmd_history_len - 1)
		input_stat.cmd_pos = cfg.cmd_history_len - 1;
}

static void
complete_search_prev(void)
{
	if(cfg.search_history_num < 0)
		return;

	if(input_stat.history_search != 2)
	{
		if(input_stat.cmd_pos == cfg.search_history_num)
			return;
		input_stat.cmd_pos++;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(++pos <= cfg.search_history_num)
		{
			wchar_t *buf;

			buf = to_wide(cfg.search_history[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos > cfg.search_history_num)
			return;
		input_stat.cmd_pos = pos;
	}

	replace_input_line(cfg.search_history[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > cfg.search_history_len - 1)
		input_stat.cmd_pos = cfg.search_history_len - 1;
}

static void
cmd_ctrl_p(struct key_info key_info, struct keys_info *keys_info)
{
	stop_completion();

	if(input_stat.history_search == 0)
	{
		free(input_stat.line_buf);
		input_stat.line_buf = my_wcsdup(input_stat.line != NULL ?
				input_stat.line : L"");
	}

	input_stat.history_search = 1;

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_prev();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_prev();
	}
}

#ifdef ENABLE_EXTENDED_KEYS
static void
cmd_up(struct key_info key_info, struct keys_info *keys_info)
{
	stop_completion();

	if(input_stat.history_search == 0)
	{
		free(input_stat.line_buf);
		input_stat.line_buf = my_wcsdup(input_stat.line != NULL ?
				input_stat.line : L"");
	}

	if(input_stat.history_search != 2)
	{
		input_stat.history_search = 2;
		input_stat.hist_search_len = input_stat.len;
	}

	if(sub_mode == CMD_SUBMODE)
	{
		complete_cmd_prev();
	}
	else if(sub_mode == SEARCH_FORWARD_SUBMODE
			|| sub_mode == SEARCH_BACKWARD_SUBMODE)
	{
		complete_search_prev();
	}
}
#endif /* ENABLE_EXTENDED_KEYS */

static void
update_cmdline(void)
{
	int d;
	input_stat.curs_pos = input_stat.prompt_wid +
			wcswidth(input_stat.line, input_stat.len);
	input_stat.index = input_stat.len;

	d = (input_stat.prompt_wid + input_stat.len + 1 + line_width - 1)/line_width;
	if(d >= getmaxy(status_bar))
		update_cmdline_size();

	update_cmdline_text();
}

static void
complete_cmd_next(void)
{
	if(cfg.cmd_history_num < 0)
		return;

	if(input_stat.history_search != 2)
	{
		if(input_stat.cmd_pos <= 0)
		{
			input_stat.cmd_pos = -1;
			free(input_stat.line);
			input_stat.line = my_wcsdup(input_stat.line_buf);
			input_stat.len = wcslen(input_stat.line);
			update_cmdline();
			return;
		}
		input_stat.cmd_pos--;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(--pos >= 0)
		{
			wchar_t *buf;

			buf = to_wide(cfg.cmd_history[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos < 0)
		{
			input_stat.cmd_pos = -1;
			free(input_stat.line);
			input_stat.line = my_wcsdup(input_stat.line_buf);
			input_stat.len = wcslen(input_stat.line);
			update_cmdline();
			return;
		}
		input_stat.cmd_pos = pos;
	}

	replace_input_line(cfg.cmd_history[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > cfg.cmd_history_len - 1)
		input_stat.cmd_pos = cfg.cmd_history_len - 1;
}

static void
complete_search_next(void)
{
	if(cfg.search_history_num < 0)
		return;

	if(input_stat.history_search != 2)
	{
		if(input_stat.cmd_pos <= 0)
		{
			input_stat.cmd_pos = -1;
			free(input_stat.line);
			input_stat.line = my_wcsdup(input_stat.line_buf);
			input_stat.len = wcslen(input_stat.line);
			update_cmdline();
			return;
		}
		input_stat.cmd_pos--;
	}
	else
	{
		int pos = input_stat.cmd_pos;
		int len = input_stat.hist_search_len;
		while(--pos >= 0)
		{
			wchar_t *buf;

			buf = to_wide(cfg.search_history[pos]);
			if(wcsncmp(input_stat.line, buf, len) == 0)
			{
				free(buf);
				break;
			}
			free(buf);
		}
		if(pos < 0)
		{
			input_stat.cmd_pos = -1;
			free(input_stat.line);
			input_stat.line = my_wcsdup(input_stat.line_buf);
			input_stat.len = wcslen(input_stat.line);
			update_cmdline();
			return;
		}
		input_stat.cmd_pos = pos;
	}

	replace_input_line(cfg.search_history[input_stat.cmd_pos]);

	update_cmdline();

	if(input_stat.cmd_pos > cfg.search_history_len - 1)
		input_stat.cmd_pos = cfg.search_history_len - 1;
}

/*
 * p - begin of part that being completed
 * completed - new part of command line
 */
static int
line_part_complete(struct line_stats *stat, const char *line_mb, const char *p,
		const char *completed)
{
	void *t;
	wchar_t *line_ending;
	int new_len;

	new_len = (p - line_mb) + mbstowcs(NULL, completed, 0)
			+ (stat->len - stat->index) + 1;

	line_ending = my_wcsdup(stat->line + stat->index);
	if(line_ending == NULL)
		return -1;

	if((t = realloc(stat->line, new_len * sizeof(wchar_t))) == NULL)
		return -1;
	stat->line = (wchar_t *) t;

	swprintf(stat->line + (p - line_mb), new_len, L"%s%ls", completed,
			line_ending);
	free(line_ending);

	update_line_stat(stat, new_len);
	update_cmdline_size();
	update_cmdline_text();
	return 0;
}

/* Returns non-zero on error */
#ifndef TEST
static
#endif
int
line_completion(struct line_stats *stat)
{
	static int offset;
	static char *line_mb, *line_mb_cmd;

	char *completion;
	int result;

	if(!stat->complete_continue)
	{
		int i;
		void *p;
		wchar_t t;

		/* only complete the part before the cursor
		 * so just copy that part to line_mb */
		t = stat->line[stat->index];
		stat->line[stat->index] = L'\0';

		i = wcstombs(NULL, stat->line, 0) + 1;

		if((p = realloc(line_mb, i * sizeof(char))) == NULL)
		{
			free(line_mb);
			line_mb = NULL;
			return -1;
		}

		line_mb = (char *) p;
		wcstombs(line_mb, stat->line, i);
		line_mb_cmd = find_last_command(line_mb);

		stat->line[stat->index] = t;

		reset_completion();
		offset = complete_cmd(line_mb_cmd);
	}

	if(get_completion_count() == 0)
		return 0;

	completion = next_completion();
	result = line_part_complete(stat, line_mb, line_mb_cmd + offset, completion);
	free(completion);

	if(get_completion_count() > 2)
		stat->complete_continue = 1;

	return result;
}

static void
update_line_stat(struct line_stats *stat, int new_len)
{
	stat->index += (new_len - 1) - stat->len;
	stat->curs_pos = stat->prompt_wid + wcswidth(stat->line, stat->index);
	stat->len = new_len - 1;
}

/* Delete a character in a string
 * For example, wcsdelch(L"fooXXbar", 4, 2) returns L"foobar" */
static wchar_t *
wcsdel(wchar_t *src, int pos, int len)
{
	int p;

	pos--;

	for(p = pos; pos - p < len; pos++)
		if(! src[pos])
		{
			src[p] = L'\0';
			return src;
		}

	pos--;

	while(src[++pos] != L'\0')
		src[pos-len] = src[pos];
	src[pos-len] = src[pos];

	return src;
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 */
void
exec_completion(const char *str)
{
	int i;

	for(i = 0; i < paths_count; i++)
	{
		if(chdir(paths[i]) != 0)
			continue;
		filename_completion(str, FNC_EXECONLY);
	}
	add_completion(str);
	chdir(curr_view->curr_dir);
}

static int
is_entry_dir(const struct dirent *d)
{
	if(d->d_type == DT_UNKNOWN)
	{
		struct stat st;
		if(stat(d->d_name, &st) != 0)
			return 0;
		return S_ISDIR(st.st_mode);
	}

	if(d->d_type != DT_DIR && d->d_type != DT_LNK)
		return 0;
	if(d->d_type == DT_LNK && !check_link_is_dir(d->d_name))
		return 0;
	return 1;
}

static int
is_entry_exec(const struct dirent *d)
{
	if(d->d_type == DT_DIR)
		return 0;
	if(d->d_type == DT_LNK && check_link_is_dir(d->d_name))
		return 0;
	if(access(d->d_name, X_OK) != 0)
		return 0;
	return 1;
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 *
 * type: FNC_*
 */
void
filename_completion(const char *str, int type)
{
	/* TODO refactor filename_completion(...) function */
	const char *string;

	DIR *dir;
	struct dirent *d;
	char * dirname;
	char * filename;
	char * temp;
	int filename_len;
	int isdir;

	if(strcmp(str, "~") == 0)
	{
		add_completion(cfg.home_dir);
		return;
	}

	string = str;

	temp = strrchr(str, '/');

	if(strncmp(string, "~/", 2) == 0)
	{
		dirname = expand_tilda(string);
		filename = strdup(dirname);
	}
	else
	{
		if(strlen(string) > 0)
		{
			dirname = strdup(string);
		}
		else
		{
			dirname = malloc(strlen(string) + 2);
			strcpy(dirname, string);
		}
		filename = strdup(string);
	}

	temp = strrchr(dirname, '/');
	if(temp)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else
	{
		dirname[0] = '.';
		dirname[1] = '\0';
	}

	dir = opendir(dirname);

	if(dir == NULL || chdir(dirname) != 0)
	{
		add_completion(filename);
		free(filename);
		free(dirname);
		return;
	}

	filename_len = strlen(filename);
	while((d = readdir(dir)) != NULL)
	{
		char *escaped;

		if(filename[0] == '\0' && d->d_name[0] == '.')
			continue;
		if(strncmp(d->d_name, filename, filename_len) != 0)
			continue;

		if(type == FNC_DIRONLY && !is_entry_dir(d))
			continue;
		else if(type == FNC_EXECONLY && !is_entry_exec(d))
			continue;
		else if(type == FNC_DIREXEC && !is_entry_dir(d) && !is_entry_exec(d))
			continue;

		isdir = 0;
		if(is_dir(d->d_name))
		{
			isdir = 1;
		}
		else if(strcmp(dirname, "."))
		{
			char * tempfile = (char *)NULL;
			int len = strlen(dirname) + strlen(d->d_name) + 1;
			tempfile = (char *)malloc((len) * sizeof(char));
			if(!tempfile)
			{
				closedir(dir);
				chdir(curr_view->curr_dir);
				add_completion(filename);
				free(filename);
				free(dirname);
				return;
			}
			snprintf(tempfile, len, "%s%s", dirname, d->d_name);
			if(is_dir(tempfile))
				isdir = 1;
			else
				temp = strdup(d->d_name);

			free(tempfile);
		}
		else
			temp = strdup(d->d_name);

		if(isdir)
		{
			char * tempfile = (char *)NULL;
			tempfile = (char *) malloc((strlen(d->d_name) + 2) * sizeof(char));
			if(!tempfile)
			{
				closedir(dir);
				chdir(curr_view->curr_dir);
				add_completion(filename);
				free(filename);
				free(dirname);
				return;
			}
			snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
			temp = strdup(tempfile);

			free(tempfile);
		}
		escaped = escape_filename(temp, 0, 1);
		add_completion(escaped);
		free(escaped);
		free(temp);
	}

	chdir(curr_view->curr_dir);

	completion_group_end();
	if(type != FNC_EXECONLY)
	{
		if(get_completion_count() == 0)
			add_completion(filename);
		else
		{
			temp = escape_filename(filename, 0, 1);
			add_completion(temp);
			free(temp);
		}
	}

	free(filename);
	free(dirname);
	closedir(dir);
}

static void
stop_completion(void)
{
	if(!input_stat.complete_continue)
		return;

	input_stat.complete_continue = 0;
	reset_completion();
	if(cfg.wild_menu)
		update_stat_window(curr_view);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
