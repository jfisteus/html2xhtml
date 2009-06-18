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

/* included first to define _GNU_SOURCE if necessary */
#include "xchar.h"

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
static int compare_aliases(const char* alias1, const char* alias2);
static charset_t* guess_charset(size_t begin_pos);

#define MODE_ASCII 0
#define MODE_EBCDIC 1
static charset_t* read_charset_decl(int ini, int step, int mode,
				    charset_t* defaults);

#ifdef WITH_CGI
static char *stop_string = NULL;
static size_t stop_len = 0;
static size_t stop_matched = 0;
static int stop_step;

static void detect_boundary(const char *buf, size_t *nread);
#endif

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == 0x0a || c == 0x0d)

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
	else if (errno == EILSEQ) {
	  /* Invalid byte sequence found in the input */
	  if (outbuf_max >= 3) {
	    /* Dump the Unicode replacement character U+FFFD, which
	     * is represented as 0xEF 0xBF 0xBD in UTF-8.
	     */
	    outbuf[0] = 0xef;
	    outbuf[1] = 0xbf;
	    outbuf[2] = 0xbd;
	    outbuf += 3;
	    outbuf_max -= 3;
	    bufferpos++;
	    avail--;
	    convert_more = 1;
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
      else if (errno == EILSEQ) {
	/* UTF-8 character that cannot be represented in
	 * the output charset. Skip the character and continue.
	 */
	bufpos++;
	n--;
	while (n > 0 && (char)(bufpos[0] & 0xC0) == (char)0x80) {
	  bufpos++;
	  n--;
	}
	WARNING("Skipped a character: cannot be converted to output charset\n\
Use UTF-8 or UTF-16 output to avoid the problem.");
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

void charset_auto_detect(size_t bytes_avail) {
  if (state != preload) {
    WARNING("Charset must be in preview mode in order to autodetect encoding");
    return;
  }

  if (!param_charset_in) {
    param_charset_in = guess_charset(avail - bytes_avail);
  }

  if (!param_charset_in) {
    EXIT("Could not autodetect input character set.");
  }

  if (param_charset_in && !param_charset_out) {
    param_charset_out = param_charset_in;
    /* Put the output in UTF-16 instead of its BE/LE variants */
    if (param_charset_in == CHARSET_UTF_16BE
	|| param_charset_in == CHARSET_UTF_16LE) {
      param_charset_out = CHARSET_UTF_16;
    }
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
    cmp = compare_aliases(charset_aliases[m].alias, alias);
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

void charset_dump_aliases(FILE* out)
{
  int i;

  for (i = 0; i < CHARSET_ALIAS_NUM; i++) {
    /* dump the charset alias only if it is the preferred name */
    if (charset_aliases[i].alias == charset_aliases[i].preferred_name) {
      fputs(charset_aliases[i].alias, out);
      fputc('\n', out);
    }
  }
}

/*
 * Guess input charset based on http://www.w3.org/TR/REC-xml/#sec-guessing
 */
charset_t* guess_charset(size_t begin_pos)
{
  enum {none, be16, le16, be32, le32, u2143, u3412,
	ebcdic, ascii_comp} guess = none;
  charset_t* charset = NULL;
  unsigned char* buf = (unsigned char*) &buffer[begin_pos];

  if (state != preload) {
    EXIT("Charset must be in preview mode in order to guess encoding");
    return NULL;
  }

  if (avail - begin_pos < 4) {
    EXIT("Too small preload buffer");
    return NULL;
  }

  /* First, look for the byte order mark (BOM) */
  if (!buf[0] && !buf[1] && ((buf[2] == 0xfe && buf[3] == 0xff)
			     ||(buf[2] == 0xff && buf[3] == 0xfe))) {
    /* UCS-4 big-endian (1234) or unusual byte order (2143) */
    charset = CHARSET_UCS_4;
  } else if ((buf[0] == 0xff && buf[1] == 0xfe)
	     || (buf[0] == 0xfe && buf[1] == 0xff)) {
    if (!buf[2] && !buf[3]) {
      /* UCS-4 little-endian (4321) or unusual order (3412) */
      charset = CHARSET_UCS_4;
    } else {
      /* UTF-16 little-endian or big-endian */
      charset = CHARSET_UTF_16;
    }
  } else if (buf[0] == 0xef && buf[1] == 0xbb && buf[2] == 0xbf) {
    /* UTF-8 */
    charset = CHARSET_UTF_8;
  }

  /* If no BOM is found, try to guess by other means */
  if (!charset) {
    if (!buf[0]) {
      if (!buf[1]) {
	if (buf[2] == 0x3c && !buf[3]) {
	  guess = u2143;
	} else if (!buf[2] && buf[3] == 0x3c) {
	  guess = be32;
	}
      } else if (buf[1] == 0x3c && !buf[2]) {
	if (!buf[3]) {
	  guess = u3412;
	} else {
	  guess = be16;
	}
      }
    } else if (buf[0] = 0x3c) {
      if (!buf[1] && !buf[3]) {
	if (!buf[2]) {
	  guess = le32;
	} else {
	  guess = le16;
	}
      } else {
	guess = ascii_comp;
      }
    } else if (buf[0] = 0x4c && buf[1] && buf[2] && buf[3]) {
      guess = ebcdic;
    } else {
      /* By default, suppose it is ASCII-compatible */
      guess = ascii_comp;
    }

    /* try now to detect the charset declaration */
    switch (guess) {
    case be16:
      charset = read_charset_decl(1, 2, MODE_ASCII, CHARSET_UTF_16BE);
      if (charset == CHARSET_UTF_16) {
	/* bad declaration in the HTML file; fix it */
	INFORM("Input charset overridden to utf-16be");
	charset = CHARSET_UTF_16BE;
      }
      break;
    case le16:
      charset = read_charset_decl(0, 2, MODE_ASCII, CHARSET_UTF_16LE);
      if (charset == CHARSET_UTF_16) {
	/* bad declaration in the HTML file; fix it */
	INFORM("Input charset overridden to utf-16le");
	charset = CHARSET_UTF_16LE;
      }
      break;
    case be32:
      charset = read_charset_decl(begin_pos + 3, 4, MODE_ASCII, CHARSET_UCS_4);
      break;
    case le32:
      charset = read_charset_decl(begin_pos, 4, MODE_ASCII, CHARSET_UCS_4);
      break;
    case u2143:
      charset = read_charset_decl(begin_pos + 2, 4, MODE_ASCII, CHARSET_UCS_4);
      break;
    case u3412:
      charset = read_charset_decl(begin_pos + 1, 4, MODE_ASCII, CHARSET_UCS_4);
      break;
    case ebcdic:
      charset = read_charset_decl(begin_pos, 1, MODE_EBCDIC, NULL);
      break;
    case ascii_comp:
      charset = read_charset_decl(begin_pos, 1, MODE_ASCII, CHARSET_UTF_8);
      break;
    case none:
      /* It should never happen */
      charset = NULL;
      break;
    }
  }

  return charset;
}

/*
 * Try to locate the "encoding" declaration if XML, or the
 * meta/http-equiv declaration if HTML.
 *
 * In order to adapt to different encodings:
 * - ini: position of the first byte to read; ini >= 0.
 * - step: number of bytes that represent each character.
 * - mode: character codes are ascii-compatible (MODE_ASCII)
 *         or EBCDIC (MODE_EBCDIC).
 * - defaults: charset to return if no declaration found.
 *
 * Returns NULL if nothing meaningful can be read (probably because
 * ini and step are incorrect)
 */
#define SCAN_LEN 512
charset_t* read_charset_decl(int ini, int step, int mode, charset_t* defaults)
{
  char buf[SCAN_LEN];
  int len, i;
  charset_t* charset = NULL;

  if (mode == MODE_EBCDIC) {
    /* EBCDIC not yet supported */
    return NULL;
  }

  if (state != preload || (avail - ini) < 16 * step) {
    return defaults;
  }

  /*
   * Copy (lowercase) data to a temporal buffer,
   * in order to make it 1 byte per byte
   */
  for (i = ini, len = 0; i < avail && len < SCAN_LEN; i += step, len++) {
    buf[len] = tolower(buffer[i]);
  }

  /*
   * because EBCDIC is not yet supported, we can safely assume data in the
   * temporal buffer is now ASCII-compatible.
   */
  if (!strncmp(buf, "<?xml", 5)) {
    /* Data is XML; look for "encoding" */
    i = 5;
    while (!charset) {
      for ( ; i < len && IS_SPACE(buf[i]); i++);
      if (i == len) {
	return NULL;
      } else if (buf[i] == '?' && ++i < len && buf[i] == '>') {
	if (defaults) {
	  charset = defaults;
	} else {
	  charset = CHARSET_UTF_8;
	}
      } else if (i + 7 < len && !strncmp(&buf[i], "encoding", 8)) {
	int parse_ok = 0;
	/* look for = */
	i += 8;
	for ( ; i < len && IS_SPACE(buf[i]); i++);
	if (i < len && buf[i] == '=') {
	  /* look for " or ' */
	  for (i++; i < len && IS_SPACE(buf[i]); i++);
	  if (i < len && (buf[i] == '\'' || buf[i] == '\"')) {
	    int j = i;
	    for (i++; i < len && buf[i] != buf[j]; i++);
	    if (i < len) {
	      char tmp_char = buf[i];
	      buf[i] = 0;
	      parse_ok = 1;
	      charset = charset_lookup_alias(&buf[j + 1]);
	      buf[i] = tmp_char;
	      if (!charset) {
		EPRINTF1("Autodetected character set: %s\n", &buf[j + 1]);
		EXIT("Unsupported input character set (autodetected)");
	      }
	    }
	  }
	}
	if (!parse_ok) {
	  return NULL;
	}
      } else {
	/* not the encoding decl.; stop at the next whitespace */
	for ( ; i < len && !IS_SPACE(buf[i]) && buf[i] != '?'; i++);
      }
    }
  } else {
    /* Stop early if http-equiv does not appear in the buffer */
    char* pos = memmem(buf, len, "http-equiv", 10);
    if (pos) {
      /* 
       * The text "http-equiv" appears in the document. Check that
       * it is an attribute of meta and parse its value.
       */
      int found = 0;
      int ini;
      int meta_ini = 0;
      enum {normal, tag_name, tag_attrs, att_val_double,
	    att_val_simple, script, script_end, comment} scan_state = normal;
      i = 0;
      for ( ; i < len && !found; i++) {
	char c = buf[i];
	switch (scan_state) {
	case normal:
	  if (c == '<') {
	    if (i + 3 < len && buf[i + 1] == '!' && buf[i + 2] == '-'
		&& buf[i + 3] == '-') {
	      scan_state = comment;
	      i += 3;
	    } else {
	      scan_state = tag_name;
	      ini = i + 1;
	    }
	  }
	  break;
	case tag_name:
	  if (IS_SPACE(c)) {
	    if (i - ini == 4 && !memcmp("meta", &buf[ini], 4)) {
	      /* now look for the attributes http-equiv and content */
	      scan_state = tag_attrs;
	      meta_ini = i;
	    }
	  } else if (c == '>') {
	    scan_state = normal;
	  }  
	  if ((IS_SPACE(c) || c == '>')
	      && i - ini == 6 && !memcmp("script", &buf[ini], 6)) {
	    /* do nothing until </script> */
	      scan_state = script;
	  }
	  break;
	case tag_attrs:
	  if (c == '>') {
	    scan_state = normal;
	    if (meta_ini) {
	      int j;
	      char* attr;
	      int attr_len;
	      
	      attr = memmem(&buf[meta_ini], i - meta_ini, "http-equiv", 9);
	      if (attr) {
		attr_len = len - (attr - buf);
		for (j = 0; j < attr_len && attr[j] != '\''
		       && attr[j] != '\"'; j++);
		if (j < attr_len) {
		  ini = j + 1;
		  for (j = ini; j < attr_len && attr[j] != attr[ini - 1]; j++);
		  if (j < i && j == ini + 12) {
		    if (!memcmp("content-type", &attr[ini], 12)) {
		      found = 1;
		    }
		  }
		}
	      }

	      if (found) {
		int content_found = 0;
		attr = &buf[meta_ini];
		attr_len = len - meta_ini;
		while (attr && !content_found) {
		  attr = memmem(attr, attr_len, "content", 7);
		  if (attr) {
		    attr_len = len - (attr - buf);
		    if (attr_len > 7 && (IS_SPACE(attr[7]) || attr[7] == '=')) {
		      content_found = 1;
		    } else {
		      attr += 7;
		    }
		  }
		}
		if (content_found) {
		  for (j = 0; j < attr_len && attr[j] != '\''
			 && attr[j] != '\"'; j++);
		  if (j < attr_len) {
		    ini = j + 1;
		    for (j = ini; j < attr_len && attr[j] != attr[ini - 1];
			 j++);
		    if (j < attr_len) {
		      char *charset_decl;
		      attr[j] = 0;
		      charset_decl = strstr(&attr[ini], "charset=");
		      if (charset_decl) {
			charset_decl += 8;
			charset = charset_lookup_alias(charset_decl);
			if (!charset) {
			  WARNING("Unknown charset in meta/@ContentType");
			}
		      }
		    }
		  }
		}
	      }

	      meta_ini = 0;
	    }
	  } else if (c == '\'') {
	    scan_state = att_val_simple;
	  } else if (c == '\"') {
	    scan_state = att_val_double;
	  }
	  break;
	case att_val_double:
	  if (c == '\"') {
	    scan_state = tag_attrs;
	  }
	  break;
	case att_val_simple:
	  if (c == '\'') {
	    scan_state = tag_attrs;
	  }
	  break;
	case script:
	  if (c == '<' && i + 6 < len && !memcmp("script", &buf[ini], 6)) {
	    scan_state = script_end;
	    i += 6;
	  }	  
	  break;
	case script_end:
	  if (c == '>') {
	    scan_state = normal;
	  }
	  break;
	case comment:
	  if (c == '-' && i + 2 < len && buf[i + 1] == '-'
	      && buf[i + 2] == '>') {
	    scan_state = normal;
	    i += 2;
	  }	  
	  break;
	}
      }
    }

    /* By default, ISO-8859-1 */
    if (!charset) {
      if (!defaults || defaults == CHARSET_UTF_8) {
	charset = CHARSET_ISO_8859_1;
      } else {
	charset = defaults;
      }
    }
  }

  return charset;
  }

/*
 * Compare charset aliases. Characters "-" and "_" are computed as
 * equivalent.
 *
 */
int compare_aliases(const char* alias1, const char* alias2)
{
  int i;

  for (i = 0; ; i++) {
    if (!alias1[i] && !alias2[i]) {
      return 0;
    } else if (!alias1[i]) {
      return -1;
    } else if (!alias2[i]) {
      return 1;
    } else if ((alias1[i] == '_' || alias1[i] == '-')
	       && (alias2[i] == '_' || alias2[i] == '-')) {
      /* nothing, just continue in the loop */
    } else {
      int c1 = tolower(alias1[i]);
      int c2 = tolower(alias2[i]);
      if (c1 != c2) {
	  return (c1 < c2 ? -1 : 1);
      }
    }
  }

  /* never should get here */
  return 0;
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

    /* error from error.h is not portable, and therefore should not be used 
     * This is a temporary fix, until a replacement for error is provided
     */
/*     if (errno == EINVAL) */
/*       error(0, 0, "Error: conversion from '%s' to '%s' not available", */
/* 	    from_charset, to_charset); */
/*     else */

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
