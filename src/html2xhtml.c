/***************************************************************************
 *   Copyright (C) 2007 by Jesus Arias Fisteus                             *
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
 * html2xhtml.c
 *
 * 'main' module for the command line version of html2xhtml.
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

#include "procesador.h"
#include "dtd_names.h"
#include "dtd_util.h"
#include "dtd.h"
#include "mensajes.h"
#include "tree.h"
#include "xchar.h"
#include "params.h"
#include "charset.h"

#ifdef WITH_CGI
#include "cgi.h"
#endif

/* parser Yacc / lex */
int yyparse(void);
void parser_set_input(FILE *input);  /* in html.l */

void print_version(void);

static void set_default_parameters(void);
static void process_parameters(int argc,char **argv);
static void help(void);
static void print_doctypes(void);
static void print_doctype_keys(void);

int main(int argc,char **argv)
{
  size_t preload_read;
  const char *preload_buffer;
  int yyparse_result;

  tree_init();

#ifdef WITH_CGI
  cgi_check_request();
  if (cgi_status < 0) {
    cgi_write_error_bad_req();
    return 0;
  }
#endif

  params_set_defaults();

#ifdef WITH_CGI
  if (!cgi_status)
    process_parameters(argc, argv);

  preload_buffer = charset_init_preload(param_inputf, &preload_read);

  if (cgi_status > 0)
    cgi_process_parameters(&preload_buffer, &preload_read);

  charset_auto_detect(preload_read);
  charset_preload_to_input(param_charset_in, preload_read);
  if (cgi_status == CGI_ST_MULTIPART)
    charset_cgi_boundary(boundary, boundary_len);
#else
  /* process command line arguments */
  process_parameters(argc, argv); 
  charset_init_preload(param_inputf, &preload_read);
  charset_auto_detect(preload_read);
  charset_preload_to_input(param_charset_in, preload_read);
#endif

  /* intialize the converter */
  saxStartDocument();
  if (param_inputf != stdin)
    parser_set_input(param_inputf);

  /* parse the input file and convert it */
  yyparse_result = yyparse();

  if (yyparse_result) {
    EXIT("Unrecoverable parse error");
  }

  charset_close();
  saxEndDocument();

#ifdef WITH_CGI
  if (!cgi_status) {
    /* write the output */
    if (writeOutput()) 
      EXIT("Bad state in writeOutput()");

    /* close de output file */
    if (param_outputf != stdout)
      fclose(param_outputf);
  } else {
    cgi_write_output();
  }
#else
  /* write the output */
  if (writeOutput()) 
    EXIT("Bad state in writeOutput()");
  
  /* close de output file */
  if (param_outputf != stdout)
    fclose(param_outputf);
#endif

  /* show final messages */
  write_end_messages();
  freeMemory();

  return 0;
}

static void process_parameters(int argc, char **argv)
{
  int i, fich, tmpnum;

  /* process command line arguments */
  for (i=1, fich=0; i<argc; i++) {
    if (!strcmp(argv[i], "-e")) {
      param_strict = 0;
    } else if (!strcmp(argv[i], "-t") && ((i+1) < argc)) {
      param_doctype = dtd_get_dtd_index(argv[++i]);
    } else if (!strcmp(argv[i], "-o") && ((i+1) < argc)) {
      /* open the output file */
      param_outputf = fopen(argv[++i], "wb");
      if (!param_outputf) {
	perror("fopen");
	EXIT("Could not open the output file for writing");
      }
    } else if (!strcmp(argv[i], "-l") && ((i+1) < argc)) {
      tmpnum= atoi(argv[++i]);
      if (tmpnum >= 40)
	param_chars_per_line = tmpnum; 
    } else if (!strcmp(argv[i], "-b") && ((i+1) < argc)) {
      tmpnum= atoi(argv[++i]);
      if (tmpnum >= 0 && tmpnum <= 16)
	param_tab_len = tmpnum; 
    } else if (!strcmp(argv[i], "--preserve-space-comments")) {
      param_pre_comments = 1;
    } else if (!strcmp(argv[i], "--no-protect-cdata")) {
      param_protect_cdata = 0;
    } else if (!strcmp(argv[i], "--compact-block-elements")) {
      param_compact_block_elms = 1;
    } else if (!strcmp(argv[i], "--compact-empty-elem-tags")) {
      param_compact_empty_elm_tags = 1;
    } else if (!strcmp(argv[i], "--empty-elm-tags-always")) {
      param_empty_tags = 1;
    } else if (!strcmp(argv[i], "--dos-eol")) {
      param_crlf_eol = 1;
    } else if (!fich && argv[i][0]!='-') {
      fich = 1;
      param_inputf = fopen(argv[i], "r");
      if (!param_inputf) {
	perror("fopen");
	EXIT("Could not open the input file for reading");
      }
    } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      help();
      exit(0);
    } else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
      print_version();
      exit(0);
    } else if (!strcmp(argv[i], "-L")) {
      print_doctype_keys();
      exit(0);
    } else if (!strcmp(argv[i], "--ics") && ((i+1) < argc)) {
      param_charset_in = charset_lookup_alias(argv[++i]);
      if (!param_charset_in) {
	EPRINTF1("Trying to set input character set: %s\n", argv[i]);
	EXIT("Unsupported character set");
      }
    } else if (!strcmp(argv[i], "--ocs") && ((i+1) < argc)) {
      param_charset_out = charset_lookup_alias(argv[++i]);
      if (!param_charset_out) {
	EPRINTF1("Trying to set output character set: %s\n", argv[i]);
	EXIT("Unsupported character set");
      }
    } else if (!strcmp(argv[i], "--lcs")) {
      charset_dump_aliases(stdout);
      exit(0);
    } else {
      help();
      exit(1);
    }
  } 
}

int yyerror(char *e)
{
  /* Let bison's error recovery mechanisms work */
  WARNING(e);
  return 0;
}

void exit_on_error(char *msg)
{
#ifdef WITH_CGI
    /* this function exits the program */
  if (cgi_status) {  
    cgi_write_error(msg);
  } else { 
#endif

#ifdef MSG_DEBUG
    fprintf(stderr,"!!%s(%d)[l%d]: %s\n",__FILE__,__LINE__,
	    parser_num_linea,msg);
#else
    fprintf(stderr,"Error [line %d]: %s\n", parser_num_linea, msg);    
#endif
    write_end_messages();

#ifdef WITH_CGI
  }
#endif

  freeMemory();
  exit(1);
}

static void help(void)
{
  print_version();
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "html2xhtml [<input_html_file>] [-t <output_doctype_key>] [-e]\n");
  fprintf(stderr, "           [-o <output_file>]\n");
  fprintf(stderr, "           [--ics <input_charset>] [--ocs <output_charset>]\n");
  fprintf(stderr, "           [-l <line_length>] [-b <tab_length>]\n");
  fprintf(stderr, "           [--preserve-space-comments] [--no-protect-cdata]\n");
  fprintf(stderr, "           [--compact-block-elements] [--empty-elm-tags-always]\n");
  fprintf(stderr, "           [--compact-empty-elm-tags] [--dos-eol]\n");
  fprintf(stderr, "\n");
  print_doctypes();
}

static void print_doctypes(void)
{
  int i;
  fprintf(stderr, "Setting output doctype:\n");
  for (i = 0; i < XHTML_NUM_DTDS; i++) {
    fprintf(stderr, "'-t %s': document type %s.\n",
	    dtd_key[i], dtd_name[i]);
  }
}

static void print_doctype_keys(void)
{
  int i;
  for (i = 0; i < XHTML_NUM_DTDS; i++) {
    fprintf(stdout, "%s\n", dtd_key[i]);
  }
}

void print_version(void)
{
#ifndef WITH_CGI
  fprintf(stderr, "html2xhtml version %s\n", VERSION);
#else
  fprintf(stderr, "html2xhtml version %s [cgi-enabled]\n", VERSION);
#endif
  fprintf(stderr, "DTD data based on DTDs as available on %s\n\n",
	  DTD_SNAPSHOT_DATE);
}



