/***************************************************************************
 *   Copyright (C) 2007 by Jesus Arias Fisteus   *
 *   jaf@it.uc3m.es   *
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


#include "procesador.h"
#include "dtd_names.h"
#include "dtd_util.h"
#include "mensajes.h"

/* parser Yacc / lex */
int yyparse(void);
void parser_set_input(FILE *input);  /* in html.l */

void help(void);
void print_version(void);
void print_doctypes(void);

char *param_charset;
char *param_charset_default;
int   param_estricto;
int   param_doctype;
FILE *outputf;

int main(int argc,char **argv)
{
  int i, fich;
  FILE* input_file;

  /* default parameters */
  param_charset= NULL;
  param_charset_default = "iso-8859-1";
  param_estricto= 1;
  param_doctype= -1;
  outputf = stdout;

  /* process command line arguments */
  for (i=1, fich=0; i<argc; i++) {
    if (!strcmp(argv[i],"-e")) {
      param_estricto= 0;
    } else if (!strcmp(argv[i],"-c") && ((i+1)<argc)) {
      i++;
      param_charset= argv[i];
    } else if (!strcmp(argv[i],"-t") && ((i+1)<argc)) {
      i++;
      param_doctype= dtd_get_dtd_index(argv[i]);
    } else if (!strcmp(argv[i],"-d") && ((i+1)<argc)) {
      i++;
      param_charset_default= argv[i];
    } else if (!fich && argv[i][0]!='-') {
      fich= 1;
      parser_set_input(fopen(argv[i],"r"));
    } else if (!strcmp(argv[i],"--help") || !strcmp(argv[i],"-h")) {
      help();
      exit(0);
    } else if (!strcmp(argv[i],"--version") || !strcmp(argv[i],"-v")) {
      print_version();
      exit(0);
    } else {
      help();
      exit(1);
    }
  } 


  saxStartDocument();

  yyparse();

  saxEndDocument();

  /* escribe la salida en modo normal */
  if (writeOutput(0)) EXIT("Bad state in writeOutput()");

  /* vuelca las estadísticas del módulo de mensajes */
  mensajes_fin();

  freeMemory();

  return 0;
}

int yyerror (char *e)
{
  EXIT(e);
}


void exit_on_error(char *msg)
{
  fprintf(stderr,"!!%s(%d)[l%d]: %s\n",__FILE__,__LINE__,
	  parser_num_linea,msg);

  mensajes_fin();
  freeMemory();

  exit(1);
}


void help(void)
{
  print_version();
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "html2xhtml [<input_html_file>] [-c <input_encoding>]\n"); 
  fprintf(stderr, "           [-d <input_encoding>] [-t <output_doctype_key>] [-e]\n");
  fprintf(stderr, "\n");
  print_doctypes();
}

void print_doctypes(void)
{
  int i;
  fprintf(stderr, "Setting output doctype:\n");
  for (i = 0; i < XHTML_NUM_DTDS; i++) {
    fprintf(stderr, "'-t %s': document type %s.\n",
	    dtd_key[i], dtd_name[i]);
  }
}

void print_version(void)
{
  fprintf(stderr, "html2xhtml version %s\n\n", VERSION);
}




