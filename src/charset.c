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
 * charset.c
 * 
 * Funtions that convert the input from any character encoding into
 * UTF-8, which is used internally by the parser and the converter.
 *
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <iconv.h>
#include <errno.h>
#include <string.h>

#include "charset.h"
#include "mensajes.h"
#include "tree.h"
#include "params.h"

static iconv_t cd;
static FILE *file;
static char buffer[CHARSET_BUFFER_SIZE];
static char *bufferpos;
static int avail;
static enum {closed, finished, eof, input, output, preload} state = closed;

static void read_block(void);
static void read_interactive(void);
static void open_iconv(const char *to_charset, const char *from_charset);

#ifdef WITH_CGI
static char *stop_string = NULL;
static size_t stop_len = 0;
static size_t stop_matched = 0;
static int stop_step;

static void detect_boundary(const char *buf, size_t *nread);
#endif

void charset_init_input(const charset_t *charset_in, FILE *input_file)
{
  if (state != closed) {
    WARNING("Charset initialized, closing it now");
    charset_close();
  }

  open_iconv(CHARSET_INTERNAL_ENC, charset_in->iconv_name);
  bufferpos = buffer;
  avail = 0;
  file = input_file;
  state = input;

  DEBUG("charset_init_input() executed");
}

void charset_init_output(const charset_t *charset_out, FILE *output_file)
{
  if (state != closed) {
    WARNING("Charset initialized, closing it now");
    charset_close();
  }

  open_iconv(charset_out->iconv_name, CHARSET_INTERNAL_ENC);
  file = output_file;
  state = output;

  DEBUG("charset_init_output() executed");
}

char *charset_init_preload(FILE *input_file, size_t *bytes_read)
{
  if (state != closed) {
    WARNING("Charset initialized, closing it now");
    charset_close();
  }

  file = input_file;
  bufferpos = buffer;
  avail = 0;
  read_block();
  *bytes_read = avail;

  state = preload;
  return buffer;
}

void charset_preload_to_input(const charset_t *charset_in, size_t bytes_avail)
{
  if (state != preload) {
    EXIT("Not in preload state");
  }

  open_iconv(CHARSET_INTERNAL_ENC, charset_in->iconv_name);
  bufferpos = buffer + avail - bytes_avail;
  avail = bytes_avail;
  state = input;
}

void charset_close()
{
  size_t wrote;

  if (state == input || (state == eof && avail > 0)) {
    WARNING("Charset closed, but input still available");
  }

  if (state == output) {
    /* reset the state */
    bufferpos = buffer;
    avail = CHARSET_BUFFER_SIZE;
    iconv (cd, NULL, NULL, &bufferpos, &avail);
    if (avail < CHARSET_BUFFER_SIZE) {
      /* write the output */
      wrote = fwrite(buffer, 1, CHARSET_BUFFER_SIZE - avail, file);
      if (wrote < 0) {
	perror("fwrite()");
	EXIT("Error writing a data block to the output");
      }
    }
  }

  if (state != closed) { 
    state = closed;
    if (state != preload)
      iconv_close(cd);
  }

#ifdef WITH_CGI
  stop_string = NULL;
  stop_len = 0;
  stop_matched = 0;
#endif

  DEBUG("charset_close() executed");
}

int charset_read(char *outbuf, size_t num, int interactive)
{
  size_t nconv;
  size_t outbuf_max = num;
  int convert_more;

  DEBUG("in charset_read()");
  EPRINTF1("    to be read %d bytes\n", num); 

  if (state != input && state != eof)
    return 0;

  do {
    convert_more = 0;

    /* read more data from file into the input buffer if needed */
    if (state != eof && bufferpos == buffer && avail < 16) {
      if (!interactive)
	read_block();
      else
	read_interactive();
    }

    /* convert the input into de internal charset */
    if (avail > 0) {
      nconv = iconv(cd, &bufferpos, &avail, &outbuf, &outbuf_max);
      if (nconv == (size_t) -1) {
	if (errno == EINVAL) {
	  /* Some bytes in the input were not converted. Move
	   * them to the beginning of the buffer so that
	   * they can be used in the next round.
	   */
	  if (state != eof) {
	    memmove (buffer, bufferpos, avail);
	    bufferpos = buffer;
	    convert_more = 1;
	  } else {
	    WARNING("Some bytes discarded at the end of the input");
	    avail = 0;
	  }
	}
	else if (errno != E2BIG) {
	  /* It is a real problem. Stop the conversion. */
	  perror("inconv");
	  if (fclose(file) != 0)
	    perror ("fclose");
	  EXIT("Error while converting the input into the internal charset");
	}
      } else {
	/* coversion OK, no input left; read more now */
	bufferpos = buffer;
	avail = 0;
	if (state != eof)
	  convert_more = 1;
      }
    }
  } while (convert_more && !interactive);

  /* if no more input, put the conversion in the initial state */
  if (state == eof && avail == 0) {
    nconv = iconv (cd, NULL, NULL, &outbuf, &outbuf_max);
    if (nconv != (size_t) -1 || errno != E2BIG)
      state = finished;
  }

  EPRINTF1("    actually read %d bytes\n", num - outbuf_max); 

  return num - outbuf_max;
}

size_t charset_write(char *buf, size_t num)
{
  int convert_again = 1;
  char *bufpos = buf;
  size_t n = num;
  size_t nconv;
  int wrote;

  DEBUG("in charset_write()");
  EPRINTF1("    write %d bytes\n", num); 

  if (state != output)
    return;

  while (convert_again) {
    convert_again = 0;
    bufferpos = buffer;
    avail = CHARSET_BUFFER_SIZE;

    nconv = iconv(cd, &bufpos, &n, &bufferpos, &avail);
    if (nconv == (size_t) -1) {
      if (errno == EINVAL) {
	/* Some bytes in the input were not converted.
	 * The caller has to feed them again later.
	 */

      } 
      else if (errno == E2BIG) {
	/* The output buffer is full; no problem, just 
	 * write and convert again
	 */
	convert_again = 1;
      }
      else {
	/* It is a real problem. Stop the conversion. */
	perror("inconv");
	if (fclose(file) != 0)
	  perror ("fclose");
	EXIT("Error while converting into the output charset");
      }
    }

    /* write the output */
    wrote = fwrite(buffer, 1, CHARSET_BUFFER_SIZE - avail, file);
    if (wrote < 0) {
      perror("fwrite()");
      EXIT("Error writing a data block to the output");
    }
  }

  /* Return the number of bytes of the internal encoding wrote.
   * The caller must feed later the bytes not wrote. 
   */
  return num - n;
}

void charset_auto_detect() {
  char *p;
  int max;
  int len;

  if (state != preload) {
    WARNING("Charset must be in preview mode in order to autodetect encoding");
    return;
  }

  if (!param_charset_in && !strncmp(bufferpos, "<?xml", 5)) {
    /* XML input, default UTF-8 */
    p = memmem(bufferpos, avail < 100 ? avail : 100, "encoding", 8);
    if (p) {
      max = avail;
      for ( ; max > 0 && p[0] != '\"'; p++, max--);
      for (p++, len = 0; max > 0 && p[len] != '\"'; len++, max--);
      if (max > 0) {
	char tmpchar = p[len];
	p[len] = 0;
	param_charset_in = charset_lookup_alias(p);
	p[len] = tmpchar;
      }
    }

    if (!param_charset_in)
      param_charset_in = CHARSET_UTF_8;
  } else if (!param_charset_in) {
    /* HTML input, default ISO-8859-1 */
    param_charset_in = CHARSET_ISO_8859_1;
  }

  if (param_charset_in && !param_charset_out) {
    param_charset_out = param_charset_in;
  }
}

charset_t* charset_lookup_alias(const char* alias)
{
  int a, b, m;
  int cmp;

  /* Perform binary search to locate the alias */
  a = 0;
  b = CHARSET_ALIAS_NUM - 1;

  while (a <= b) {
    m = (a + b) / 2;
    cmp = strcasecmp(charset_aliases[m].alias, alias);
    if (!cmp) {
      /* found! */
      break;
    } else if (cmp < 0) {
      a = m + 1;
    } else {
      b = m - 1;
    }
  }

  if (a <= b) {
    return &charset_aliases[m];
  } else {
    return NULL;
  }
}

static void read_block()
{
  size_t nread;
  int read_again = 1;

#ifdef WITH_CGI
  if (stop_string && stop_matched > 0) {
    /* refill the buffer with the partially matched data */
    memcpy(&buffer[avail], stop_string, stop_matched);
    avail += stop_matched;
  }
#endif

  while (read_again) {
    nread = fread(buffer + avail, 1, sizeof (buffer) - avail, file);
    read_again = 0;
    if (nread == 0) {
      if (ferror(file)) {
	if (errno != EINTR) {
	  perror("read");
	  EXIT("Error reading the input");
	} else {
	  /* interrupted: read again */
	  read_again = 1;
	}
      } else {
	/* End of input file */
	state = eof;
      }
    }
  }

#ifdef WITH_CGI
  if (stop_string && nread > 0) {
    if (stop_matched > 0) {
      /* the stop string was partially matched */
      int len;

      len = stop_len - stop_matched;
      if (len > nread)
	len = nread;
      if (!memcmp(&buffer[avail], &stop_string[stop_matched], len)) {
	/* stop string found (or still partially) */
	nread = -stop_matched;
	stop_matched += len;
	if (stop_matched == stop_len)
	  state = eof;
      } else {
	stop_matched = 0;
      }
    }
    if (!stop_matched)
      detect_boundary(&buffer[avail], &nread);
  }
#endif

  avail += nread;
}

static void read_interactive()
{
  int c = '*';
  int n;
  size_t max_size;

  max_size = sizeof(buffer) - avail;

  for (n = 0; n < max_size && (c = getc(file)) != EOF && c != '\n'; ++n)
    bufferpos[n] = (char) c;

  if (c == '\n')
    bufferpos[n++] = (char) c;
  else if (c == EOF) {
    if (ferror(file)) {
      perror("getc()");
      EXIT("interactive input failed");
    } else
      state = eof;
  }

  avail += n;
}

static void open_iconv(const char *to_charset, const char *from_charset)
{
  cd = iconv_open(to_charset, from_charset);
  if (cd == (iconv_t) -1) {
    /* Something went wrong.  */
    if (errno == EINVAL)
      error(0, 0, "Error: conversion from '%s' to '%s' not available",
	    from_charset, to_charset);
    else
      perror ("iconv_open");

    if (fclose(file) != 0)
      perror ("fclose");
    EXIT("Conversion aborted");
  }
}

#ifdef WITH_CGI
void charset_cgi_boundary(const char *str, size_t len)
{
  int i;

  if (state == input) {
    /* construct the stop string \r\n--boundary */
    stop_len = 4 + len;
    stop_string = tree_malloc(stop_len + 1);
    stop_string[0] = '\r';
    stop_string[1] = '\n';
    stop_string[2] = '-';
    stop_string[3] = '-';
    memcpy(&stop_string[4], str, len);
    stop_string[stop_len] = 0;
    stop_matched = 0;

    /* optimization: normally, there are a lot of '-' in a row */
    for (i = 4; i < stop_len && stop_string[i] == '-'; i++);
    stop_step = i - 2;

    /* if input data available in the buffer, look for the boundary */
    detect_boundary(bufferpos, &avail);
  }
}

static void detect_boundary(const char *buf, size_t *nread)
{
  int i;
  int ini, middle, end;
  int pos = 1; /* no need to scan first two, as they should be crlf */
  int k;
  
  do {
    /* scan the bytes read to find the "---" pattern */
    pos += stop_step;
    for ( ; pos < *nread && buf[pos] != '-'; pos += stop_step);
    i = pos;
    if (i >= *nread)
      i = *nread - 1;
    if (buf[i] == '-') {
      /* look backwards */
      middle = i;
      k = (pos - stop_step >= 1) ? pos - stop_step : 1;
      for ( ; i > k && buf[i] == '-'; i--);
      if (buf[i - 1] == '\r' && buf[i] == '\n') {
	/* look forward */
	ini = i - 1;
	end = ini + stop_len;
	end = (end <= *nread) ? end : *nread;
	if (middle == end - 1 || !memcmp(&buf[middle + 1], 
					 &stop_string[middle + 1 - ini],
					 end - middle - 1)) {
	  stop_matched = end - ini;
	  *nread = ini;
	  if (stop_matched == stop_len)
	    state = eof;
	}
      }
    } else if (buf[i] == '\r') {
      (*nread)--;
      stop_matched = 1;
    } else if (buf[i] == '\n' && buf[i - 1] == '\r') {
      *nread -= 2;
      stop_matched = 2;
    }
  } while (pos < *nread && !stop_matched);
}
#endif
