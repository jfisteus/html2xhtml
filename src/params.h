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

/*
 * params.h
 * 
 * Execution parameters.
 *
 */

#ifndef PARAMS_H
#define PARAMS_H

#include <stdio.h>
#include "charset.h"

extern FILE *param_inputf;
extern FILE *param_outputf;

extern charset_t *param_charset_in;
extern charset_t *param_charset_out;
extern int   param_strict;
extern int   param_doctype;
extern int   param_chars_per_line;
extern int   param_tab_len;
extern int   param_pre_comments; /* preserve spacing inside comments */
extern int   param_protect_cdata;
extern int   param_cgi_html_output;
extern int   param_compact_block_elms;
extern int   param_compact_empty_elm_tags;
extern int   param_empty_tags;
extern int   param_crlf_eol;

void params_set_defaults(void);

#endif
