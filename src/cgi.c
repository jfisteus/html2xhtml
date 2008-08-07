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

#include "tree.h"
#include "cgi.h"
#include "params.h"
#include "dtd_util.h"

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
    else if (length <= 0) {
      cgi_status = CGI_ERR_OTHER;
    } else if (!strncmp(type, "multipart/form-data; boundary=", 30)) {
      boundary = tree_strdup(&type[30]);
      boundary_len = strlen(boundary);
      cgi_status = CGI_ST_MULTIPART;
    } else if (!strncmp(type,"text/html", 9))
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

  return CGI_OK;
}

static int process_params_query_string()
{
  return CGI_ERR_OTHER;
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
    if ((!strncmp(name, "output", 6) || !strncmp(name, "salida", 6))
	&& value_len == 5 && !strncmp(value, "plain", 5)) {
      param_cgi_html_output = 0;
      return 1;
    }
  } else if (name_len == 9) {
    /* param "tablen" */
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
