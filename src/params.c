/***************************************************************************
 *   Copyright (C) 2008 by Jesus Arias Fisteus                             *
 *   jaf@it.uc3m.es                                                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "params.h"

FILE *param_inputf;
FILE *param_outputf;

charset_t *param_charset_in;
charset_t *param_charset_out;
int   param_strict;
int   param_doctype;
int   param_chars_per_line;
int   param_tab_len;
int   param_pre_comments; /* preserve spacing inside comments */
int   param_protect_cdata;
int   param_cgi_html_output;
int   param_compact_block_elms;
int   param_compact_empty_elm_tags;
int   param_empty_tags;
int   param_crlf_eol;

void params_set_defaults()
{
  param_outputf = stdout;
  param_inputf = stdin;

  param_charset_in = NULL;
  param_charset_out = NULL;
  param_strict = 1;
  param_doctype = -1;
  param_chars_per_line = 80;
  param_tab_len = 2;
  param_pre_comments = 0;
  param_protect_cdata = 1;
  param_compact_block_elms = 0;
  param_compact_empty_elm_tags = 0;
  param_cgi_html_output = 0;
  param_empty_tags = 0;
  param_crlf_eol = 0;
}

