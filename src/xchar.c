/***************************************************************************
 *   Copyright (C) 2007, 09 by Jesus Arias Fisteus                         *
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
 * xchar.c
 *
 * (Jesús Arias Fisteus)
 *
 * para tratar con tipos alternativos de
 * caracteres al tipo 'char'.
 *
 * hecho para que si en el futuro se manejan
 * otros juegos de caracteres distintos de ASCII
 * no sea necesario cambiar todo el código del
 * proyecto.
 *
 * Define el tipo xchar, que de momento es char,
 * pero que en un futuro se puede ajustar a otro
 * tipo, como wchar_t, sin tener que realizar demasiados
 * cambios en el resto del programa
 * 
 */



#include "xchar.h"
#include "../config.h"

#include <string.h>


/* 
 * conversión a minúsculas 
 *
 * en principio, se supone ASCII
 *
 * ¿usar tabla para la conversión? 
 *
 */
void xtolower(xchar *dest, const xchar *src, size_t dest_len)
{
  int i;

  for (i=0; src[i] && (i<(dest_len-1)); i++)
    if ((src[i]>='A')&&(src[i]<='Z'))
      dest[i]= src[i] + 0x20;
    else dest[i]= src[i];

  dest[i]=0;
}





/*
 * busca la subcadena substr en str
 *
 * devuelve NULL si no aparece
 * o la posición en que la encontró en str
 *
 */
xchar *xsearch(const xchar *str, const xchar *substr)
{
  return strstr(str,substr);
}





/*
 * devuelve la longitud de una cadena (en caracteres)
 *
 */
size_t xstrlen(const xchar *str)
{
  return strlen(str);
}

/*
 * devuelve la longitud de una cadena (en caracteres)
 *
 */
size_t xstrnlen(const xchar *str, int maxlen)
{
  int i;

  for (i = 0; i < maxlen && str[i]; i++); 

  return i;
}



/*
 * devuelve la longitud de una cadena (bytes) contando
 * el treminador nulo
 *
 */
size_t xstrsize(const xchar *str)
{
  return strlen(str)+1;
}

#ifndef HAVE_MEMMEM
/*
 * The following memmem function has been copied from
 * the GNU C Library. It is used for those systems that do
 * not provide the memmem function.
 *
 * The copyright notice of glibc-2.7/string/memmem.c is:
 */

/* Copyright (C) 1991,92,93,94,96,97,98,2000,2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <stddef.h>
#include <string.h>

#ifndef _LIBC
# define __builtin_expect(expr, val)   (expr)
#endif

#undef memmem

/* Return the first occurrence of NEEDLE in HAYSTACK.  */
void *
memmem (haystack, haystack_len, needle, needle_len)
     const void *haystack;
     size_t haystack_len;
     const void *needle;
     size_t needle_len;
{
  const char *begin;
  const char *const last_possible
    = (const char *) haystack + haystack_len - needle_len;

  if (needle_len == 0)
    /* The first occurrence of the empty string is deemed to occur at
       the beginning of the string.  */
    return (void *) haystack;

  /* Sanity check, otherwise the loop might search through the whole
     memory.  */
  if (__builtin_expect (haystack_len < needle_len, 0))
    return NULL;

  for (begin = (const char *) haystack; begin <= last_possible; ++begin)
    if (begin[0] == ((const char *) needle)[0] &&
	!memcmp ((const void *) &begin[1],
		 (const void *) ((const char *) needle + 1),
		 needle_len - 1))
      return (void *) begin;

  return NULL;
}
#endif
