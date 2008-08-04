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
static enum {closed, finished, eof, open} state = closed;

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
  state = open;

  DEBUG("charset_init() executed");
}

void charset_close()
{
  if (state == open || (state == eof && avail > 0)) {
    WARNING("Charset closed, but input still available");
  }
  if (state != closed) 
    iconv_close(cd);

  DEBUG("charset_close() executed");
}

int charset_read(char *outbuf, size_t num)
{
  size_t nread;
  size_t nconv;
  size_t outbuf_max = num;
  int read_again = 1;

  DEBUG("in charset_get_input()");
  EPRINTF1("    read %d bytes\n", num); 

  if (state != open && state != eof)
    return 0;

  /* read more data from file into the input buffer if needed */
  if (state != eof && bufferpos == buffer && avail < 16) {
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

