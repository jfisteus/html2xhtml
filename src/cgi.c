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

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tree.h"
#include "cgi.h"
#include "params.h"
#include "dtd_util.h"
#include "procesador.h"
#include "mensajes.h"
#include "charset.h"

#ifdef WITH_CGI

int cgi_status = CGI_ST_UNITIALIZED;

char *boundary = NULL;
int boundary_len = 0;

static int process_params_multipart(const char **input, size_t *input_len);
static int process_params_query_string();
static int mult_skip_boundary(const char **input, size_t *input_len);
static int mult_param_name_len(const char *input, size_t avail);
static int mult_skip_double_eol(const char **input, size_t *input_len);
static int mult_param_value_len(const char *input, size_t avail);
static int set_param(const char *name, size_t name_len,
		     const char *value, size_t value_len);
static void cgi_write_header(void);
static void cgi_write_footer();
static charset_t* lookup_charset(const char* alias, size_t alias_len);

#ifdef CGI_DEBUG
static void cgi_debug_write_input(void);
static void cgi_debug_write_state(void);
#endif

int cgi_check_request()
{
  char *method;
  char *type;
  char *query_string;
  char *l;
  int length= -1;

  method = getenv("REQUEST_METHOD");
  type = getenv("CONTENT_TYPE");
  query_string = getenv("QUERY_STRING");

  if (type && method && query_string) {
    l = getenv("CONTENT_LENGTH");
    if (l) 
      length = atoi(l);

    if (strcasecmp(method, "POST"))
      cgi_status = CGI_ERR_METHOD;
    else if (length <= 0)
      cgi_status = CGI_ERR_OTHER;
    else if (!strncmp(type, "multipart/form-data; boundary=", 30)) {
      boundary = tree_strdup(&type[30]);
      boundary_len = strlen(boundary);
      cgi_status = CGI_ST_MULTIPART;
    } else if ((!strncmp(type, MIME_TYPE_HTML, MIME_TYPE_HTML_LEN)
		&& !type[MIME_TYPE_HTML_LEN])
	       || !strncmp(type, MIME_TYPE_XHTML, MIME_TYPE_XHTML_LEN)
		&& !type[MIME_TYPE_XHTML_LEN])
      cgi_status = CGI_ST_DIRECT;
    else
      cgi_status = CGI_ERR_OTHER;
  } else {
    cgi_status = CGI_ST_NOCGI;
  }

  return cgi_status;
} 

int cgi_process_parameters(const char **input, size_t *input_len)
{
  int error = CGI_OK;

  if (cgi_status == CGI_ST_DIRECT) {
    param_cgi_html_output = 0;
    process_params_query_string();
  } else if (cgi_status == CGI_ST_MULTIPART) {
    param_cgi_html_output = 1;
    process_params_multipart(input, input_len);
  } else {
    error = CGI_ERR_NOCGI;
  }

  return error;
}

void cgi_write_error_bad_req()
{
  fprintf(stdout, "Content-Type:%s\n", MIME_TYPE_HTML);
  
  /* invalid request */
  if (cgi_status == CGI_ERR_METHOD) {
     /* method != POST */
    fprintf(stdout, "Status:405 Method not allowed\n\n");
    if (param_cgi_html_output) {
      fprintf(stdout,"<html><head><title>html2xhtml-Error</title></head><body>\
                      <h1>405 Method not allowed</h1></body></html>");
    }
  }
  else {
    fprintf(stdout,"Status:400 Bad request\n\n");
    if (param_cgi_html_output) {
      fprintf(stdout, "<html><head><title>html2xhtml-Error</title></head>\
                       <body><h1>400 Bad Request</h1></body></html>");
    }
  }  
}

void cgi_write_output()
{
  if (param_cgi_html_output) {
    fprintf(stdout, "Content-Type:%s; charset=%s\n\n", MIME_TYPE_HTML,
	    param_charset_out->preferred_name);
    cgi_write_header();
  } else {
    fprintf(stdout, "Content-Type:%s; charset=%s\n\n", MIME_TYPE_XHTML,
	    param_charset_out->preferred_name);
  }

  /* write the XHTML output */
  if (writeOutput()) 
    EXIT("Incorrect state in writeOutput()");

  if (param_cgi_html_output)
    cgi_write_footer();
}

void cgi_write_error(char *msg)
{  
  fprintf(stdout, "Content-Type:%s\n", MIME_TYPE_HTML);
  fprintf(stdout, "Status:400 Bad request\n\n");
  fprintf(stdout, "<html><head><title>html2xhtml-Error</title></head><body>");
  fprintf(stdout, "<h1>400 Bad Request</h1>");
  fprintf(stdout, "<p>An error has been detected while parsing the input");
  if (parser_num_linea > 0) 
    fprintf(stdout, " at line %d. Please, ", parser_num_linea);
  else fprintf(stdout, ". Please, ");
  fprintf(stdout, "check that you have uploaded a HTML document.</p>");
  if (msg)
    fprintf(stdout, "<p>Error: %s</p>", msg);

#ifdef CGI_DEBUG
  cgi_debug_write_state();
#endif

  fprintf(stdout, "</body></html>");  
}

static int process_params_multipart(const char **input, size_t *input_len)
{
  int html_found = 0;
  const char *param_name;
  int param_name_len;
  int param_value_len;

  while (!html_found) {
    /* skip the boundary */
    if (!mult_skip_boundary(input, input_len))
      return CGI_ERR_PARAMS;

    /* read the parameter */
    if (strncmp(*input, "Content-Disposition: form-data; name=\"", 38))
      return CGI_ERR_PARAMS;
    *input += 38;
    *input_len -= 38;
    if ((param_name_len = mult_param_name_len(*input, *input_len)) < 0)
      return CGI_ERR_PARAMS;
    param_name = *input;
    if (!mult_skip_double_eol(input, input_len))
      return CGI_ERR_PARAMS;

    /* it can be the html file or other parameter */
    if (param_name_len == 4 && !strncmp("html", param_name, 4)) {
      html_found = 1;
    } else {
      if ((param_value_len = mult_param_value_len(*input, *input_len)) < 0)
	return CGI_ERR_PARAMS;
      set_param(param_name, param_name_len, *input, param_value_len);
      *input += param_value_len + 2;
      *input_len -= param_value_len + 2;
    }
  }

  if (param_cgi_html_output)
    param_charset_out = CHARSET_UTF_8;

  return CGI_OK;
}

static int process_params_query_string()
{
  char *query_str;
  int len;
  int pos;
  int name_pos, eq_pos;

  query_str = getenv("QUERY_STRING");
  if (query_str) {
    len = strlen(query_str);
    pos = 0;
    while (pos < len) {
      /* read one parameter */
      name_pos = pos;
      for (pos = pos + 1; pos < len && query_str[pos] != '='; pos++);
      if (pos == len)
	return CGI_ERR_PARAMS;
      eq_pos = pos;
      for (pos = pos + 1; pos < len && query_str[pos] != '&'; pos++);

      /* process this parameter */
      set_param(&query_str[name_pos], eq_pos - name_pos,
		&query_str[eq_pos + 1], pos - eq_pos - 1);

      /* advance to the next parameter */
      pos++;
    }
  }

  return CGI_OK;
}

static int set_param(const char *name, size_t name_len,
		     const char *value, size_t value_len)
{
  int tmpnum;

  if (name_len == 4) {
    /* param "type"/"tipo" */
    if (!strncmp(name, "type", 4) || !strncmp(name, "tipo", 4)) {
      tmpnum = dtd_get_dtd_index_n(value, value_len);
      if (tmpnum >= 0) {
	param_doctype = tmpnum;
	return 1;
      }
    }
  } else if (name_len == 6) {
    /* param "output"/"salida" */
    if ((!strncmp(name, "output", 6) || !strncmp(name, "salida", 6))) {
      if (value_len == 5 && !strncmp(value, "plain", 5)) {
	param_cgi_html_output = 0;
	return 1;
      } else if (value_len == 4 && !strncmp(value, "html", 4)) {
	param_cgi_html_output = 1;
	return 1;
      }
    }
  } else if (name_len == 7) {
    /* param "dos-eol" */
    if (!strncmp(name, "dos-eol", 7)) {
      if (value_len == 1 && value[0] == '1')
	param_crlf_eol = 1;
    }
  } else if (name_len == 9) {
    /* param "tablength" */
    if (!strncmp(name, "tablength", 9)) {
      char num[value_len + 1];
      memcpy(num, value, value_len);
      num[value_len] = 0;
      tmpnum= atoi(value);
      if (tmpnum >= 0 && tmpnum <= 16) {
	param_tab_len= tmpnum;
	return 1;
      }
    }
  } else if (name_len == 10) {
    /* param "linelength" */
    if (!strncmp(name, "linelength", 10)) {
      char num[value_len + 1];
      memcpy(num, value, value_len);
      num[value_len] = 0;
      tmpnum= atoi(value);
      if (tmpnum >= 40) {
	param_chars_per_line= tmpnum;
	return 1;
      }
    }
  } else if (name_len == 13) {
    if (!strncmp(name, "input-charset", 13)) {
      if (value_len == 4 && !strncmp(value, "auto", 4)) {
	/* input charset auto-detection; explicited for cases
	 * when this parameter is received from a Web form
	 * (there needs to be an "auto" option in the form).
	 */
	param_charset_in = NULL;
      } else {
	param_charset_in = lookup_charset(value, value_len);
      }
    }
  } else if (name_len == 14) {
    if (!strncmp(name, "output-charset", 14)) {
      if (value_len == 4 && !strncmp(value, "auto", 4)) {
	/* output charset: same as input */
	param_charset_out = NULL;
      } else {
	param_charset_out = lookup_charset(value, value_len);
      }
    }
  } else if (name_len == 16) {
    if (!strncmp(name, "no-protect-cdata", 16)) {
      if (value_len == 1 && value[0] == '1')
	param_protect_cdata = 0;
    }
  } else if (name_len == 21) {
    if (!strncmp(name, "empty-elm-tags-always", 21)) {
      if (value_len == 1 && value[0] == '1')
	param_empty_tags = 1;
    }
  } else if (name_len == 22) {
    if (!strncmp(name, "compact-block-elements", 22)) {
      if (value_len == 1 && value[0] == '1')
	param_compact_block_elms = 1;
    }
  } else if (name_len == 23) {
    if (!strncmp(name, "preserve-space-comments", 23)) {
      if (value_len == 1 && value[0] == '1')
	param_pre_comments = 1;
    } else if (!strncmp(name, "compact-empty-elem-tags", 23)) {
      if (value_len == 1 && value[0] == '1')
	param_compact_empty_elm_tags = 1;
    }
  }
}

static charset_t* lookup_charset(const char* alias, size_t alias_len) {
  char aliasz[64];
  if (alias_len < 64) {
    memcpy(aliasz, alias, alias_len);
    aliasz[alias_len] = 0;
    return charset_lookup_alias(aliasz);
  } else {
    return NULL;
  }
}

/*
 * Returns 1 if correctly skipped the boundary, 0 otherwise.
 */
static int mult_skip_boundary(const char **input, size_t *input_len)
{
  int adv = 4 + boundary_len; /* '--' + boundary + '\r\n' */

  if (*input_len < adv || (*input)[0] != '-' || (*input)[1] != '-'
      || (*input)[adv - 2] != '\r' || (*input)[adv - 1] != '\n')
    return 0;

  if (!strncmp(boundary, (*input) + 2, boundary_len)) {
    *input_len -= adv;
    *input += adv;
    return 1;
  } else {
    return 0;
  }
}

static int mult_param_name_len(const char *input, size_t avail)
{
  int len;

  for (len = 0; len < avail && input[len] != '\r' && input[len] != '\"'; len++);
  if (len == avail || input[len] != '\"')
    return -1;
  else
    return len;
}

static int mult_param_value_len(const char *input, size_t avail)
{
  int len;

  for (len = 0; 
       len < (avail - 1) && input[len] != '\r' && input[len+1] != '\n';
       len++);
  if (len == avail - 1)
    return -1;
  else
    return len;
}

static int mult_skip_double_eol(const char **input, size_t *input_len)
{
  int skipped = 0;

  while (!skipped && *input_len >= 4) {
    for ( ; (*input_len) > 0 && **input != '\r'; (*input_len)--, (*input)++);
    if (*input_len >= 4) {
      if ((*input)[1] == '\n' && (*input)[2] == '\r' && (*input)[3] == '\n')
	skipped = 1;
      *input_len -= 4;
      *input += 4;
    }
  }

  return skipped;
}

static void cgi_write_header()
{
  fprintf(stdout, "<?xml version=\"1.0\"");
  fprintf(stdout," encoding=\"%s\"", param_charset_out->preferred_name);
  fprintf(stdout,"?>\n\n");

  fprintf(stdout, "\
<!DOCTYPE html\n\
   PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n\
   \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n\n");

  fprintf(stdout, "\
<html xmlns='http://www.w3.org/1999/xhtml'>\n\
   <head>\n\
     <title>html2xhtml - page converted</title>\n\
     <link type='text/css' href='../html2xhtml/html2xhtml.css' \
rel='stylesheet'/>\n\
   </head>\n");
  
  fprintf(stdout, "\
  <body>\n\
    <div id='container'>\n");

  fprintf(stdout, "\
      <div id='top'>\n\
        <h1>html2xhtml</h1>\n\
        <p>The document has been converted</p>\n\
      </div>\n");

  fprintf(stdout, "\
      <div id='content-left-aligned'>\n\
        <p>\n\
	  <a href='../html2xhtml/'>Main page</a> | \n\
	  <a href='../html2xhtml/download.html'>Download</a> |\n\
	  <a href='../html2xhtml/advanced.html'>Online conversion</a> |\n\
	  <a href='../html2xhtml/web-api.html'>Web API</a> |\n\
	  <a href='../html2xhtml/news.html'>News</a> |\n\
	  <a href='../xhtmlpedia/'>Xhtmlpedia</a>\n\
        </p>\n");

  fprintf(stdout, "\
        <p>The input document has been succesfully converted. If you want\n\
          to save it in a file, copy and paste it in a text editor.</p>\n\
        <pre class='document' xml:space='preserve'>\n");
}

static void cgi_write_footer()
{
  fprintf(stdout, "\
</pre>\n\
        <p>Remember that you can also\n\
          <a href='../html2xhtml/download.html'>download\n\
          html2xhtml</a> and run it in your computer, or invoke\n\
          the service from your own programs through its\n\
          <a href='../html2xhtml/web-api.html'>Web API</a>.</p>\n\
      </div>\n\
      <div id='footer'>\n\
        <img src='../html2xhtml/h2x.png' alt='html2xhtml logo' />\n\
        <i>html2xhtml %s</i>, copyright 2001-2010 <a href=\
'http://www.it.uc3m.es/jaf/index.html'>Jesús Arias Fisteus</a>; \
2001 Rebeca Díaz Redondo, Ana Fernández Vilas\n\
      </div>\n\
    </div>\n", VERSION);

#ifdef CGI_DEBUG
  cgi_debug_write_state();
#endif

  fprintf(stdout, "  </body>\n</html>\n");
}

#ifdef CGI_DEBUG
static void cgi_debug_write_input()
{
  int i;
  int c;

  fprintf(stdout,"Content-Type:%s\n\n","text/plain");

  while (1){
    c=fgetc(stdin);
    if (c==EOF) break;
    fputc(c,stdout);
  }
}

static void cgi_debug_write_state()
{
  fprintf(stdout,"<hr/><p>Internal state:</p>");
  fprintf(stdout,"<ul>");
  fprintf(stdout,"<li>CGI status: %d</li>", cgi_status);
  fprintf(stdout,"<li>HTML output: %d</li>", param_cgi_html_output);
  fprintf(stdout,"</ul>");
}
#endif

#endif
