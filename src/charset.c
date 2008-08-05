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
 * charset.c
 * 
 * Funtions that convert the input from any character encoding into
 * UTF-8, which is used internally by the parser and the converter.
 *
 */

#include <stdio.h>
#include <iconv.h>
#include <errno.h>
#include <string.h>

#include "charset.h"
#include "mensajes.h"

static iconv_t cd;
static FILE *file;
static char buffer[CHARSET_BUFFER_SIZE];
static char *bufferpos;
static int avail;
static enum {closed, finished, eof, input, output} state = closed;

static void read_block(void);
static void read_interactive(void);

void charset_init_input(const char *charset_in, FILE *input_file)
{
  if (state != closed) {
    WARNING("Charset initialized, closing it now");
    charset_close();
  }

  file = input_file;
  cd = iconv_open(CHARSET_INTERNAL_ENC, charset_in);
  if (cd == (iconv_t) -1) {
    /* Something went wrong.  */
    if (errno == EINVAL)
      error(0, 0, "Error: conversion from '%s' to '%s' not available",
	    charset_in, CHARSET_INTERNAL_ENC);
    else
      perror ("iconv_open");

    if (fclose(file) != 0)
      perror ("fclose");
    EXIT("Conversion aborted");
  }

  bufferpos = buffer;
  avail = 0;
  state = input;

  DEBUG("charset_init_input() executed");
}

void charset_init_output(const char *charset_out, FILE *output_file)
{
  if (state != closed) {
    WARNING("Charset initialized, closing it now");
    charset_close();
  }

  file = output_file;
  cd = iconv_open(charset_out, CHARSET_INTERNAL_ENC);
  if (cd == (iconv_t) -1) {
    /* Something went wrong.  */
    if (errno == EINVAL)
      error(0, 0, "Error: conversion from '%s' to '%s' not available",
	    CHARSET_INTERNAL_ENC, charset_out);
    else
      perror ("iconv_open");

    if (fclose(file) != 0)
      perror ("fclose");
    EXIT("Conversion aborted");
  }

  state = output;

  DEBUG("charset_init_output() executed");
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
    iconv_close(cd);
    state = closed;
  }

  DEBUG("charset_close() executed");
}

int charset_read(char *outbuf, size_t num, int interactive)
{
  size_t nconv;
  size_t outbuf_max = num;

  DEBUG("in charset_read()");
  EPRINTF1("    read %d bytes\n", num); 

  if (state != input && state != eof)
    return 0;

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
	} else {
	  WARNING("Some bytes discarded at the end of the input");
	  avail = 0;
	}
      } 
      else if (errno == E2BIG) {
	/* The output buffer is full; no problem, just 
	 * leave the rest of the input buffer for
	 * the next call.
	 */
      }
      else {
	/* It is a real problem. Stop the conversion. */
	perror("inconv");
	if (fclose(file) != 0)
	  perror ("fclose");
	EXIT("Error while converting the input into the internal charset");
      }
    } else {
      /* coversion OK, no input left; read more in the next call */
      bufferpos = buffer;
      avail = 0;
    }
  }

  /* if no more input, put the conversion in the initial state */
  if (state == eof && avail == 0) {
    iconv (cd, NULL, NULL, &outbuf, &outbuf_max);
    state = finished;
  }

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

static void read_block()
{
  size_t nread;
  int read_again = 1;

  while (read_again) {
    nread = fread(bufferpos, 1, sizeof (buffer) - avail, file);
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
