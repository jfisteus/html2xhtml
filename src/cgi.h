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
 * cgi.h
 * 
 * Funtions related to the CGI mode of the converter.
 *
 */

#ifndef CGI_H
#define CGI_H

extern int cgi_status;
extern char *boundary;
extern int boundary_len;

#define CGI_ST_NOCGI 0
#define CGI_ST_DIRECT 1
#define CGI_ST_MULTIPART 3
#define CGI_ST_UNITIALIZED -255

#define CGI_OK 0
#define CGI_ERR_METHOD -1
#define CGI_ERR_NOCGI -2
#define CGI_ERR_PARAMS -3
#define CGI_ERR_OTHER -9

#define MIME_TYPE_XHTML "application/xhtml+xml"
#define MIME_TYPE_HTML  "text/html"
#define MIME_TYPE_XHTML_LEN 21
#define MIME_TYPE_HTML_LEN  9

/*
 * Checks wether the request comes from the CGI interface.
 *
 * Returns:
 *   CGI_ST_NOCGI if it is not a CGI request
 *   CGI_ST_DIRECT if it is a valid CGI request (direct input)
 *   CGI_ST_MULTIPART if it is a valid CGI request (multipart/form-data)
 *   CGI_ERR_METHOD if it is an invalid CGI request (bad method)
 *   CGI_ERR_OTHER if it is an invalid CGI request because of other reasons
 */  
int cgi_check_request(void);

/*
 * Processes the execution parameters from the CGI input or query string.
 * Receives a buffer for reading standard input (if multipart) with 'input_len'
 * bytes. 'input_len' is updated by substracting all the bytes until the 
 * beginning of the actual HTML input.
 *
 * Returns:
 *   CGI_OK if parameters set without errors
 *   CGI_ERR_PARAMS if an error occurred parsing parameters
 *   CGI_ERR_NOCGI if the request is not a CGI
 */
int cgi_process_parameters(const char **input, size_t *input_len);

/*
 * Writes the output of the CGI in HTML mode.
 */
void cgi_write_output(void);

/*
 * Writes an error output (general CGI error).
 */
void cgi_write_error_bad_req(void);

/*
 * Writes an error message about the conversion from HTML to XHTML
 * itself.
 */
void cgi_write_error(char *msg);

#endif
