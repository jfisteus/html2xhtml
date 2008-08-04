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
static FILE *input;
static char inbuf[CHARSET_BUFFER_SIZE];
static char *inpos;
static int inavail;
static int eof_reached;
static int closed;

void charset_init(const char *charset_in, FILE *input_file)
{
  input = input_file;
  cd = iconv_open(CHARSET_INTERNAL_ENC, charset_in);
  if (cd == (iconv_t) -1) {
    /* Something went wrong.  */
    if (errno == EINVAL)
      error(0, 0, "Error: conversion from '%s' to '%s' not available",
	    charset_in, CHARSET_INTERNAL_ENC);
    else
      perror ("iconv_open");

    if (fclose(input) != 0)
      perror ("fclose");
    EXIT("Conversion aborted");
  }

  inpos = inbuf;
  inavail = 0;
  eof_reached = 0;
  closed = 0;

  DEBUG("charset_init() executed");
}

int charset_get_input(char *buf, size_t num)
{
  size_t nread;
  size_t nconv;
  size_t buf_max = num;

  DEBUG("in charset_get_input()");
  EPRINTF1("    read %d bytes\n", num); 

  if (closed)
    return 0;

  /* read more data from file into the input buffer if needed */
  if (!eof_reached && inpos == inbuf && inavail < 16) {
    nread = fread(inpos, 1, sizeof (inbuf) - inavail, input);
    if (nread == 0) {
      if (ferror(input)) {
	perror("read");
	EXIT("Error reading the input");
      } else {
	/* End of input file */
	eof_reached = 1;
      }
    }
    inavail += nread;
  }

  /* convert the input into de internal charset */
  if (inavail > 0) {
    nconv = iconv(cd, &inpos, &inavail, &buf, &buf_max);
    if (nconv == (size_t) -1) {
      if (errno == EINVAL) {
	/* Some bytes in the input were not converted. Move
	 * them to the beginning of the buffer so that
	 * they can be used in the next round.
	 */
	if (!eof_reached) {
	  memmove (inbuf, inpos, inavail);
	  inpos = inbuf;
	} else {
	  WARNING("Some bytes discarded at the end of the input");
	  inavail = 0;
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
	if (fclose(input) != 0)
	  perror ("fclose");
	EXIT("Error while converting the input into the internal charset");
      }
    } else {
      /* coversion OK, no input left; read more in the next call */
      inpos = inbuf;
      inavail = 0;
    }
  }

  /* if no more input, put the conversion in the initial state */
  if (eof_reached && inavail == 0) {
    iconv (cd, NULL, NULL, &buf, &buf_max);
    closed = 1;
  }

  return num - buf_max;
}

