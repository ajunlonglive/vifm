/* vifm
 * Copyright (C) 2012 xaizek.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "../config.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* qsort() */
#include <string.h> /* strlen() strncasecmp() */
#include <wchar.h> /* wcscpy() wcslen() */

#include "utils/macros.h"

#include "bracket_notation.h"

/* Bracket notation entry description structure. */
typedef struct
{
	const char *notation; /* The notation itself. */
	const wchar_t key[8]; /* The replacement for the notation. */
	size_t len; /* Length of the notation. */
}key_pair_t;

static int notation_sorter(const void *first, const void *second);
static key_pair_t * find_notation(const char str[]);

static key_pair_t key_pairs[] = {
	{ "<esc>",      L"\x1b"              },
	{ "<c-a>",      L"\x01"              },
	{ "<c-b>",      L"\x02"              },
	{ "<c-c>",      L"\x03"              },
	{ "<c-d>",      L"\x04"              },
	{ "<c-e>",      L"\x05"              },
	{ "<c-f>",      L"\x06"              },
	{ "<c-g>",      L"\x07"              },
	{ "<c-h>",      { KEY_BACKSPACE, 0 } },
	{ "<c-i>",      L"\x09"              },
	{ "<c-j>",      L"\x0a"              },
	{ "<c-k>",      L"\x0b"              },
	{ "<c-l>",      L"\x0c"              },
	{ "<c-m>",      L"\x0d"              },
	{ "<c-n>",      L"\x0e"              },
	{ "<c-o>",      L"\x0f"              },
	{ "<c-p>",      L"\x10"              },
	{ "<c-q>",      L"\x11"              },
	{ "<c-r>",      L"\x12"              },
	{ "<c-s>",      L"\x13"              },
	{ "<c-t>",      L"\x14"              },
	{ "<c-u>",      L"\x15"              },
	{ "<c-v>",      L"\x16"              },
	{ "<c-w>",      L"\x17"              },
	{ "<c-x>",      L"\x18"              },
	{ "<c-y>",      L"\x19"              },
	{ "<c-z>",      L"\x1a"              },
	{ "<c-[>",      L"\x1b"              },
	{ "<c-\\>",     L"\x1c"              },
	{ "<c-]>",      L"\x1d"              },
	{ "<c-^>",      L"\x1e"              },
	{ "<c-_>",      L"\x1f"              },
	{ "<s-c-a>",    L"\x01"              },
	{ "<s-c-b>",    L"\x02"              },
	{ "<s-c-c>",    L"\x03"              },
	{ "<s-c-d>",    L"\x04"              },
	{ "<s-c-e>",    L"\x05"              },
	{ "<s-c-f>",    L"\x06"              },
	{ "<s-c-g>",    L"\x07"              },
	{ "<s-c-h>",    L"\x08"              },
	{ "<s-c-i>",    L"\x09"              },
	{ "<s-c-j>",    L"\x0a"              },
	{ "<s-c-k>",    L"\x0b"              },
	{ "<s-c-l>",    L"\x0c"              },
	{ "<s-c-m>",    L"\x0d"              },
	{ "<s-c-n>",    L"\x0e"              },
	{ "<s-c-o>",    L"\x0f"              },
	{ "<s-c-p>",    L"\x10"              },
	{ "<s-c-q>",    L"\x11"              },
	{ "<s-c-r>",    L"\x12"              },
	{ "<s-c-s>",    L"\x13"              },
	{ "<s-c-t>",    L"\x14"              },
	{ "<s-c-u>",    L"\x15"              },
	{ "<s-c-v>",    L"\x16"              },
	{ "<s-c-w>",    L"\x17"              },
	{ "<s-c-x>",    L"\x18"              },
	{ "<s-c-y>",    L"\x19"              },
	{ "<s-c-z>",    L"\x1a"              },
	{ "<s-c-[>",    L"\x1b"              },
	{ "<s-c-\\>",   L"\x1c"              },
	{ "<s-c-]>",    L"\x1d"              },
	{ "<s-c-^>",    L"\x1e"              },
	{ "<s-c-_>",    L"\x1f"              },
	{ "<c-s-a>",    L"\x01"              },
	{ "<c-s-b>",    L"\x02"              },
	{ "<c-s-c>",    L"\x03"              },
	{ "<c-s-d>",    L"\x04"              },
	{ "<c-s-e>",    L"\x05"              },
	{ "<c-s-f>",    L"\x06"              },
	{ "<c-s-g>",    L"\x07"              },
	{ "<c-s-h>",    L"\x08"              },
	{ "<c-s-i>",    L"\x09"              },
	{ "<c-s-j>",    L"\x0a"              },
	{ "<c-s-k>",    L"\x0b"              },
	{ "<c-s-l>",    L"\x0c"              },
	{ "<c-s-m>",    L"\x0d"              },
	{ "<c-s-n>",    L"\x0e"              },
	{ "<c-s-o>",    L"\x0f"              },
	{ "<c-s-p>",    L"\x10"              },
	{ "<c-s-q>",    L"\x11"              },
	{ "<c-s-r>",    L"\x12"              },
	{ "<c-s-s>",    L"\x13"              },
	{ "<c-s-t>",    L"\x14"              },
	{ "<c-s-u>",    L"\x15"              },
	{ "<c-s-v>",    L"\x16"              },
	{ "<c-s-w>",    L"\x17"              },
	{ "<c-s-x>",    L"\x18"              },
	{ "<c-s-y>",    L"\x19"              },
	{ "<c-s-z>",    L"\x1a"              },
	{ "<c-s-[>",    L"\x1b"              },
	{ "<c-s-\\>",   L"\x1c"              },
	{ "<c-s-]>",    L"\x1d"              },
	{ "<c-s-^>",    L"\x1e"              },
	{ "<c-s-_>",    L"\x1f"              },
	{ "<cr>",       L"\r"                },
	{ "<space>",    L" "                 },
	{ "<tab>",      L"\t"                },
#ifdef ENABLE_EXTENDED_KEYS
	{ "<s-tab>",    { KEY_BTAB, 0 }      },
#else
	{ "<s-tab>",    L"\033"L"[Z"         },
#endif
#ifndef __PDCURSES__
	{ "<a-a>",      L"\033"L"a"          },
	{ "<a-b>",      L"\033"L"b"          },
	{ "<a-c>",      L"\033"L"c"          },
	{ "<a-d>",      L"\033"L"d"          },
	{ "<a-e>",      L"\033"L"e"          },
	{ "<a-f>",      L"\033"L"f"          },
	{ "<a-g>",      L"\033"L"g"          },
	{ "<a-h>",      L"\033"L"h"          },
	{ "<a-i>",      L"\033"L"i"          },
	{ "<a-j>",      L"\033"L"j"          },
	{ "<a-k>",      L"\033"L"k"          },
	{ "<a-l>",      L"\033"L"l"          },
	{ "<a-m>",      L"\033"L"m"          },
	{ "<a-n>",      L"\033"L"n"          },
	{ "<a-o>",      L"\033"L"o"          },
	{ "<a-p>",      L"\033"L"p"          },
	{ "<a-q>",      L"\033"L"q"          },
	{ "<a-r>",      L"\033"L"r"          },
	{ "<a-s>",      L"\033"L"s"          },
	{ "<a-t>",      L"\033"L"t"          },
	{ "<a-u>",      L"\033"L"u"          },
	{ "<a-v>",      L"\033"L"v"          },
	{ "<a-w>",      L"\033"L"w"          },
	{ "<a-x>",      L"\033"L"x"          },
	{ "<a-y>",      L"\033"L"y"          },
	{ "<a-z>",      L"\033"L"z"          },
	{ "<m-a>",      L"\033"L"a"          },
	{ "<m-b>",      L"\033"L"b"          },
	{ "<m-c>",      L"\033"L"c"          },
	{ "<m-d>",      L"\033"L"d"          },
	{ "<m-e>",      L"\033"L"e"          },
	{ "<m-f>",      L"\033"L"f"          },
	{ "<m-g>",      L"\033"L"g"          },
	{ "<m-h>",      L"\033"L"h"          },
	{ "<m-i>",      L"\033"L"i"          },
	{ "<m-j>",      L"\033"L"j"          },
	{ "<m-k>",      L"\033"L"k"          },
	{ "<m-l>",      L"\033"L"l"          },
	{ "<m-m>",      L"\033"L"m"          },
	{ "<m-n>",      L"\033"L"n"          },
	{ "<m-o>",      L"\033"L"o"          },
	{ "<m-p>",      L"\033"L"p"          },
	{ "<m-q>",      L"\033"L"q"          },
	{ "<m-r>",      L"\033"L"r"          },
	{ "<m-s>",      L"\033"L"s"          },
	{ "<m-t>",      L"\033"L"t"          },
	{ "<m-u>",      L"\033"L"u"          },
	{ "<m-v>",      L"\033"L"v"          },
	{ "<m-w>",      L"\033"L"w"          },
	{ "<m-x>",      L"\033"L"x"          },
	{ "<m-y>",      L"\033"L"y"          },
	{ "<m-z>",      L"\033"L"z"          },
	{ "<a-c-a>",    L"\033"L"\x01"       },
	{ "<a-c-b>",    L"\033"L"\x02"       },
	{ "<a-c-c>",    L"\033"L"\x03"       },
	{ "<a-c-d>",    L"\033"L"\x04"       },
	{ "<a-c-e>",    L"\033"L"\x05"       },
	{ "<a-c-f>",    L"\033"L"\x06"       },
	{ "<a-c-g>",    L"\033"L"\x07"       },
	{ "<a-c-h>",    L"\033"L"\x08"       },
	{ "<a-c-i>",    L"\033"L"\x09"       },
	{ "<a-c-j>",    L"\033"L"\x0a"       },
	{ "<a-c-k>",    L"\033"L"\x0b"       },
	{ "<a-c-l>",    L"\033"L"\x0c"       },
	{ "<a-c-m>",    L"\033"L"\x0d"       },
	{ "<a-c-n>",    L"\033"L"\x0e"       },
	{ "<a-c-o>",    L"\033"L"\x0f"       },
	{ "<a-c-p>",    L"\033"L"\x10"       },
	{ "<a-c-q>",    L"\033"L"\x11"       },
	{ "<a-c-r>",    L"\033"L"\x12"       },
	{ "<a-c-s>",    L"\033"L"\x13"       },
	{ "<a-c-t>",    L"\033"L"\x14"       },
	{ "<a-c-u>",    L"\033"L"\x15"       },
	{ "<a-c-v>",    L"\033"L"\x16"       },
	{ "<a-c-w>",    L"\033"L"\x17"       },
	{ "<a-c-x>",    L"\033"L"\x18"       },
	{ "<a-c-y>",    L"\033"L"\x19"       },
	{ "<a-c-z>",    L"\033"L"\x1a"       },
	{ "<m-c-a>",    L"\033"L"\x01"       },
	{ "<m-c-b>",    L"\033"L"\x02"       },
	{ "<m-c-c>",    L"\033"L"\x03"       },
	{ "<m-c-d>",    L"\033"L"\x04"       },
	{ "<m-c-e>",    L"\033"L"\x05"       },
	{ "<m-c-f>",    L"\033"L"\x06"       },
	{ "<m-c-g>",    L"\033"L"\x07"       },
	{ "<m-c-h>",    L"\033"L"\x08"       },
	{ "<m-c-i>",    L"\033"L"\x09"       },
	{ "<m-c-j>",    L"\033"L"\x0a"       },
	{ "<m-c-k>",    L"\033"L"\x0b"       },
	{ "<m-c-l>",    L"\033"L"\x0c"       },
	{ "<m-c-m>",    L"\033"L"\x0d"       },
	{ "<m-c-n>",    L"\033"L"\x0e"       },
	{ "<m-c-o>",    L"\033"L"\x0f"       },
	{ "<m-c-p>",    L"\033"L"\x10"       },
	{ "<m-c-q>",    L"\033"L"\x11"       },
	{ "<m-c-r>",    L"\033"L"\x12"       },
	{ "<m-c-s>",    L"\033"L"\x13"       },
	{ "<m-c-t>",    L"\033"L"\x14"       },
	{ "<m-c-u>",    L"\033"L"\x15"       },
	{ "<m-c-v>",    L"\033"L"\x16"       },
	{ "<m-c-w>",    L"\033"L"\x17"       },
	{ "<m-c-x>",    L"\033"L"\x18"       },
	{ "<m-c-y>",    L"\033"L"\x19"       },
	{ "<m-c-z>",    L"\033"L"\x1a"       },
	{ "<c-a-a>",    L"\033"L"\x01"       },
	{ "<c-a-b>",    L"\033"L"\x02"       },
	{ "<c-a-c>",    L"\033"L"\x03"       },
	{ "<c-a-d>",    L"\033"L"\x04"       },
	{ "<c-a-e>",    L"\033"L"\x05"       },
	{ "<c-a-f>",    L"\033"L"\x06"       },
	{ "<c-a-g>",    L"\033"L"\x07"       },
	{ "<c-a-h>",    L"\033"L"\x08"       },
	{ "<c-a-i>",    L"\033"L"\x09"       },
	{ "<c-a-j>",    L"\033"L"\x0a"       },
	{ "<c-a-k>",    L"\033"L"\x0b"       },
	{ "<c-a-l>",    L"\033"L"\x0c"       },
	{ "<c-a-m>",    L"\033"L"\x0d"       },
	{ "<c-a-n>",    L"\033"L"\x0e"       },
	{ "<c-a-o>",    L"\033"L"\x0f"       },
	{ "<c-a-p>",    L"\033"L"\x10"       },
	{ "<c-a-q>",    L"\033"L"\x11"       },
	{ "<c-a-r>",    L"\033"L"\x12"       },
	{ "<c-a-s>",    L"\033"L"\x13"       },
	{ "<c-a-t>",    L"\033"L"\x14"       },
	{ "<c-a-u>",    L"\033"L"\x15"       },
	{ "<c-a-v>",    L"\033"L"\x16"       },
	{ "<c-a-w>",    L"\033"L"\x17"       },
	{ "<c-a-x>",    L"\033"L"\x18"       },
	{ "<c-a-y>",    L"\033"L"\x19"       },
	{ "<c-a-z>",    L"\033"L"\x1a"       },
	{ "<c-m-a>",    L"\033"L"\x01"       },
	{ "<c-m-b>",    L"\033"L"\x02"       },
	{ "<c-m-c>",    L"\033"L"\x03"       },
	{ "<c-m-d>",    L"\033"L"\x04"       },
	{ "<c-m-e>",    L"\033"L"\x05"       },
	{ "<c-m-f>",    L"\033"L"\x06"       },
	{ "<c-m-g>",    L"\033"L"\x07"       },
	{ "<c-m-h>",    L"\033"L"\x08"       },
	{ "<c-m-i>",    L"\033"L"\x09"       },
	{ "<c-m-j>",    L"\033"L"\x0a"       },
	{ "<c-m-k>",    L"\033"L"\x0b"       },
	{ "<c-m-l>",    L"\033"L"\x0c"       },
	{ "<c-m-m>",    L"\033"L"\x0d"       },
	{ "<c-m-n>",    L"\033"L"\x0e"       },
	{ "<c-m-o>",    L"\033"L"\x0f"       },
	{ "<c-m-p>",    L"\033"L"\x10"       },
	{ "<c-m-q>",    L"\033"L"\x11"       },
	{ "<c-m-r>",    L"\033"L"\x12"       },
	{ "<c-m-s>",    L"\033"L"\x13"       },
	{ "<c-m-t>",    L"\033"L"\x14"       },
	{ "<c-m-u>",    L"\033"L"\x15"       },
	{ "<c-m-v>",    L"\033"L"\x16"       },
	{ "<c-m-w>",    L"\033"L"\x17"       },
	{ "<c-m-x>",    L"\033"L"\x18"       },
	{ "<c-m-y>",    L"\033"L"\x19"       },
	{ "<c-m-z>",    L"\033"L"\x1a"       },
#else
	{ "<a-a>",      { ALT_A, 0 }         },
	{ "<a-b>",      { ALT_B, 0 }         },
	{ "<a-c>",      { ALT_C, 0 }         },
	{ "<a-d>",      { ALT_D, 0 }         },
	{ "<a-e>",      { ALT_E, 0 }         },
	{ "<a-f>",      { ALT_F, 0 }         },
	{ "<a-g>",      { ALT_G, 0 }         },
	{ "<a-h>",      { ALT_H, 0 }         },
	{ "<a-i>",      { ALT_I, 0 }         },
	{ "<a-j>",      { ALT_J, 0 }         },
	{ "<a-k>",      { ALT_K, 0 }         },
	{ "<a-l>",      { ALT_L, 0 }         },
	{ "<a-m>",      { ALT_M, 0 }         },
	{ "<a-n>",      { ALT_N, 0 }         },
	{ "<a-o>",      { ALT_O, 0 }         },
	{ "<a-p>",      { ALT_P, 0 }         },
	{ "<a-q>",      { ALT_Q, 0 }         },
	{ "<a-r>",      { ALT_R, 0 }         },
	{ "<a-s>",      { ALT_S, 0 }         },
	{ "<a-t>",      { ALT_T, 0 }         },
	{ "<a-u>",      { ALT_U, 0 }         },
	{ "<a-v>",      { ALT_V, 0 }         },
	{ "<a-w>",      { ALT_W, 0 }         },
	{ "<a-x>",      { ALT_X, 0 }         },
	{ "<a-y>",      { ALT_Y, 0 }         },
	{ "<a-z>",      { ALT_Z, 0 }         },
	{ "<m-a>",      { ALT_A, 0 }         },
	{ "<m-b>",      { ALT_B, 0 }         },
	{ "<m-c>",      { ALT_C, 0 }         },
	{ "<m-d>",      { ALT_D, 0 }         },
	{ "<m-e>",      { ALT_E, 0 }         },
	{ "<m-f>",      { ALT_F, 0 }         },
	{ "<m-g>",      { ALT_G, 0 }         },
	{ "<m-h>",      { ALT_H, 0 }         },
	{ "<m-i>",      { ALT_I, 0 }         },
	{ "<m-j>",      { ALT_J, 0 }         },
	{ "<m-k>",      { ALT_K, 0 }         },
	{ "<m-l>",      { ALT_L, 0 }         },
	{ "<m-m>",      { ALT_M, 0 }         },
	{ "<m-n>",      { ALT_N, 0 }         },
	{ "<m-o>",      { ALT_O, 0 }         },
	{ "<m-p>",      { ALT_P, 0 }         },
	{ "<m-q>",      { ALT_Q, 0 }         },
	{ "<m-r>",      { ALT_R, 0 }         },
	{ "<m-s>",      { ALT_S, 0 }         },
	{ "<m-t>",      { ALT_T, 0 }         },
	{ "<m-u>",      { ALT_U, 0 }         },
	{ "<m-v>",      { ALT_V, 0 }         },
	{ "<m-w>",      { ALT_W, 0 }         },
	{ "<m-x>",      { ALT_X, 0 }         },
	{ "<m-y>",      { ALT_Y, 0 }         },
	{ "<m-z>",      { ALT_Z, 0 }         },
#endif
	{ "<del>",      L"\177"              },
#ifdef ENABLE_EXTENDED_KEYS
	{ "<home>",     { KEY_HOME,      0 } },
	{ "<end>",      { KEY_END,       0 } },
	{ "<left>",     { KEY_LEFT,      0 } },
	{ "<right>",    { KEY_RIGHT,     0 } },
	{ "<up>",       { KEY_UP,        0 } },
	{ "<down>",     { KEY_DOWN,      0 } },
	{ "<bs>",       { KEY_BACKSPACE, 0 } },
	{ "<delete>",   { KEY_DC,        0 } },
	{ "<pageup>",   { KEY_PPAGE,     0 } },
	{ "<pagedown>", { KEY_NPAGE,     0 } },
	{ "<f0>",       { KEY_F(0),      0 } },
	{ "<f1>",       { KEY_F(1),      0 } },
	{ "<f2>",       { KEY_F(2),      0 } },
	{ "<f3>",       { KEY_F(3),      0 } },
	{ "<f4>",       { KEY_F(4),      0 } },
	{ "<f5>",       { KEY_F(5),      0 } },
	{ "<f6>",       { KEY_F(6),      0 } },
	{ "<f7>",       { KEY_F(7),      0 } },
	{ "<f8>",       { KEY_F(8),      0 } },
	{ "<f9>",       { KEY_F(9),      0 } },
	{ "<f10>",      { KEY_F(10),     0 } },
	{ "<f11>",      { KEY_F(11),     0 } },
	{ "<f12>",      { KEY_F(12),     0 } },
	{ "<f13>",      { KEY_F(13),     0 } },
	{ "<f14>",      { KEY_F(14),     0 } },
	{ "<f15>",      { KEY_F(15),     0 } },
	{ "<f16>",      { KEY_F(16),     0 } },
	{ "<f17>",      { KEY_F(17),     0 } },
	{ "<f18>",      { KEY_F(18),     0 } },
	{ "<f19>",      { KEY_F(19),     0 } },
	{ "<f20>",      { KEY_F(20),     0 } },
	{ "<f21>",      { KEY_F(21),     0 } },
	{ "<f22>",      { KEY_F(22),     0 } },
	{ "<f23>",      { KEY_F(23),     0 } },
	{ "<f24>",      { KEY_F(24),     0 } },
	{ "<f25>",      { KEY_F(25),     0 } },
	{ "<f26>",      { KEY_F(26),     0 } },
	{ "<f27>",      { KEY_F(27),     0 } },
	{ "<f28>",      { KEY_F(28),     0 } },
	{ "<f29>",      { KEY_F(29),     0 } },
	{ "<f30>",      { KEY_F(30),     0 } },
	{ "<f31>",      { KEY_F(31),     0 } },
	{ "<f32>",      { KEY_F(32),     0 } },
	{ "<f33>",      { KEY_F(33),     0 } },
	{ "<f34>",      { KEY_F(34),     0 } },
	{ "<f35>",      { KEY_F(35),     0 } },
	{ "<f36>",      { KEY_F(36),     0 } },
	{ "<f37>",      { KEY_F(37),     0 } },
	{ "<f38>",      { KEY_F(38),     0 } },
	{ "<f39>",      { KEY_F(39),     0 } },
	{ "<f40>",      { KEY_F(40),     0 } },
	{ "<f41>",      { KEY_F(41),     0 } },
	{ "<f42>",      { KEY_F(42),     0 } },
	{ "<f43>",      { KEY_F(43),     0 } },
	{ "<f44>",      { KEY_F(44),     0 } },
	{ "<f45>",      { KEY_F(45),     0 } },
	{ "<f46>",      { KEY_F(46),     0 } },
	{ "<f47>",      { KEY_F(47),     0 } },
	{ "<f48>",      { KEY_F(48),     0 } },
	{ "<f49>",      { KEY_F(49),     0 } },
	{ "<f50>",      { KEY_F(50),     0 } },
	{ "<f51>",      { KEY_F(51),     0 } },
	{ "<f52>",      { KEY_F(52),     0 } },
	{ "<f53>",      { KEY_F(53),     0 } },
	{ "<f54>",      { KEY_F(54),     0 } },
	{ "<f55>",      { KEY_F(55),     0 } },
	{ "<f56>",      { KEY_F(56),     0 } },
	{ "<f57>",      { KEY_F(57),     0 } },
	{ "<f58>",      { KEY_F(58),     0 } },
	{ "<f59>",      { KEY_F(59),     0 } },
	{ "<f60>",      { KEY_F(60),     0 } },
	{ "<f61>",      { KEY_F(61),     0 } },
	{ "<f62>",      { KEY_F(62),     0 } },
	{ "<f63>",      { KEY_F(63),     0 } },
	{ "<s-f1>",     { KEY_F(13),     0 } },
	{ "<s-f2>",     { KEY_F(14),     0 } },
	{ "<s-f3>",     { KEY_F(15),     0 } },
	{ "<s-f4>",     { KEY_F(16),     0 } },
	{ "<s-f5>",     { KEY_F(17),     0 } },
	{ "<s-f6>",     { KEY_F(18),     0 } },
	{ "<s-f7>",     { KEY_F(19),     0 } },
	{ "<s-f8>",     { KEY_F(20),     0 } },
	{ "<s-f9>",     { KEY_F(21),     0 } },
	{ "<s-f10>",    { KEY_F(22),     0 } },
	{ "<s-f11>",    { KEY_F(23),     0 } },
	{ "<s-f12>",    { KEY_F(24),     0 } },
	{ "<c-f1>",     { KEY_F(25),     0 } },
	{ "<c-f2>",     { KEY_F(26),     0 } },
	{ "<c-f3>",     { KEY_F(27),     0 } },
	{ "<c-f4>",     { KEY_F(28),     0 } },
	{ "<c-f5>",     { KEY_F(29),     0 } },
	{ "<c-f6>",     { KEY_F(30),     0 } },
	{ "<c-f7>",     { KEY_F(31),     0 } },
	{ "<c-f8>",     { KEY_F(32),     0 } },
	{ "<c-f9>",     { KEY_F(33),     0 } },
	{ "<c-f10>",    { KEY_F(34),     0 } },
	{ "<c-f11>",    { KEY_F(35),     0 } },
	{ "<c-f12>",    { KEY_F(36),     0 } },
	{ "<a-f1>",     { KEY_F(37),     0 } },
	{ "<a-f2>",     { KEY_F(38),     0 } },
	{ "<a-f3>",     { KEY_F(39),     0 } },
	{ "<a-f4>",     { KEY_F(40),     0 } },
	{ "<a-f5>",     { KEY_F(41),     0 } },
	{ "<a-f6>",     { KEY_F(42),     0 } },
	{ "<a-f7>",     { KEY_F(43),     0 } },
	{ "<a-f8>",     { KEY_F(44),     0 } },
	{ "<a-f9>",     { KEY_F(45),     0 } },
	{ "<a-f10>",    { KEY_F(46),     0 } },
	{ "<a-f11>",    { KEY_F(47),     0 } },
	{ "<a-f12>",    { KEY_F(48),     0 } },
	{ "<m-f1>",     { KEY_F(37),     0 } },
	{ "<m-f2>",     { KEY_F(38),     0 } },
	{ "<m-f3>",     { KEY_F(39),     0 } },
	{ "<m-f4>",     { KEY_F(40),     0 } },
	{ "<m-f5>",     { KEY_F(41),     0 } },
	{ "<m-f6>",     { KEY_F(42),     0 } },
	{ "<m-f7>",     { KEY_F(43),     0 } },
	{ "<m-f8>",     { KEY_F(44),     0 } },
	{ "<m-f9>",     { KEY_F(45),     0 } },
	{ "<m-f10>",    { KEY_F(46),     0 } },
	{ "<m-f11>",    { KEY_F(47),     0 } },
	{ "<m-f12>",    { KEY_F(48),     0 } },
#endif
};

void
init_bracket_notation(void)
{
	size_t i;
	for(i = 0; i < ARRAY_LEN(key_pairs); i++)
	{
		key_pairs[i].len = strlen(key_pairs[i].notation);
	}

	qsort(key_pairs, ARRAY_LEN(key_pairs), sizeof(key_pairs[0]), notation_sorter);
}

/* Sorter function to called by qsort. */
static int
notation_sorter(const void *first, const void *second)
{
	const key_pair_t *paira = (const key_pair_t *)first;
	const key_pair_t *pairb = (const key_pair_t *)second;
	const char *stra = paira->notation;
	const char *strb = pairb->notation;
	return strcasecmp(stra, strb);
}

wchar_t *
substitute_specs(const char cmd[])
{
	wchar_t *buf, *p;
	size_t len = strlen(cmd) + 1;

	buf = malloc(len*sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}

	p = buf;
	while(*cmd != '\0')
	{
		key_pair_t *pair;
		pair = find_notation(cmd);
		if(pair == NULL)
		{
			*p++ = (wchar_t)*cmd++;
		}
		else
		{
			wcscpy(p, pair->key);
			p += wcslen(p);
			cmd += pair->len;
		}
	}
	*p = L'\0';
	assert(p + 1 - buf <= len);

	return buf;
}

/* Performs binary search in the list of bracket notations.  Returns NULL if
 * str wasn't found in the list. */
static key_pair_t *
find_notation(const char str[])
{
	int l = 0, u = ARRAY_LEN(key_pairs) - 1;
	while(l <= u)
	{
		int i = (l + u)/2;
		int comp = strncasecmp(str, key_pairs[i].notation, key_pairs[i].len);
		if(comp == 0)
		{
			return &key_pairs[i];
		}
		else if(comp < 0)
		{
			u = i - 1;
		}
		else
		{
			l = i + 1;
		}
	}
	return NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
