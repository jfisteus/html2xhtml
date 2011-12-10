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
 * procesador.c
 *
 * (Jes�s Arias Fisteus)
 * 
 * Funciones que se ejecutan seg�n se van encontrando
 * elementos... en el documento html de entrada.
 * El analizador sint�ctico (yacc) notifica a este
 * m�dulo los componentes que se va encontrando en
 * el documento HTML de entrada, mediante
 * una interfaz semejante a la de SAX.
 *
 * En este m�dulo se llevan a cabo todas las 
 * comprobaciones necesarias y la conversi�n,
 * y se va generando un �rbol en memoria para
 * almacenar el documento XHTML resultante
 * (ver tree.c)
 *
 * Se implementan tambi�n las funciones necesarias
 * para imprimir el documento XHTML a partir
 * del �rbol. 
 *
 * Se activa el m�dulo mediante una llamada a
 * saxStartDocument() y se finaliza mediante saxEndDocument().
 * Una vez finalizado, se puede volcar la salida con
 * writeOutput().
 *
 */


#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "procesador.h"
#include "tree.h"
#include "dtd_util.h"
#include "mensajes.h"
#include "xchar.h"
#include "charset.h"
#include "params.h"
#include "snprintf.h"

#ifdef SELLAR
#define SELLO "translated by html2xhtml - http://www.it.uc3m.es/jaf/html2xhtml/"
#endif

#define MAX_NUM_ERRORS 20
#define ID_LIST_SIZE    8192

extern int is_ascii;

/* estructura principal */
document_t *document;  /* �rbol del documento */

/* estado del m�dulo */
static enum {ST_START,ST_PARSING,ST_END} state= ST_START;

static int doctype_mask= 0x7;
static int doctype= -1;
static int doctype_locked= 0;
static int doctype_detected= 0;

static tree_node_t *actual_element= NULL;  /* elemento actual */

/* new place recovery mode: variables set to non-null when this 
mode is active */
static int new_place_recovery_on;
static tree_node_t *new_place_recovery_elm;
static tree_node_t *new_place_recovery_father;

static int num_errores;


static int doctype_scan(const xchar *data);
static int doctype_set(int type, int lock);

static void set_attributes(tree_node_t *elm, xchar **atts);
static void elm_close(tree_node_t *nodo);
static int  insert_element(tree_node_t *nodo);
static void insert_chardata(const xchar *ch, int len, node_type_t type);
/* static void elm_meta_scan(tree_node_t *meta); */
static int  elm_check_prohibitions(int elmid);
static int  elm_is_father(int elmid, tree_node_t *nodo);
static int  text_contains_special_chars(const xchar *ch, int len);

#ifdef SELLAR
static void sellar_documento(void);
#endif


/* funciones de gesti�n de error */
static void err_att_default(tree_node_t *elm, xchar **atts);
static int err_html_struct(document_t *document, int elm_id);
static int err_child_no_valid(tree_node_t* elm_ptr);
static int err_att_req(tree_node_t *elm, int att_id, xchar **atts);
static int err_att_value(tree_node_t *elm, int att_id, const xchar *value);
static int err_content_invalid(tree_node_t* nodo, int hijos[], int num_hijos);
static int err_elm_desconocido(const xchar *nombre);

/* auxiliares */
static tree_node_t* err_aux_insert_elm(int elm_id, const xchar *content, int len);
static int remove_duplicate_elm(int elmid, tree_node_t* parent, int hijos[]);
static xchar* check_and_fix_att_value(xchar* value);

/* new output functions */
static void write_document(document_t *doc);
static int write_node(tree_node_t *node);
static int write_element(tree_node_t *node);
static int write_chardata(tree_node_t *node);
static int write_cdata_sec(tree_node_t *node);
static int write_whitespace_or_newline_if_needed(int next_data_len);
static int write_chardata_space_preserve(tree_node_t *node);
static int write_comment(tree_node_t *node);
static int write_start_tag(tree_node_t* nodo);
static int write_end_tag(tree_node_t* nodo);
static int write_indent(int len, int new_line);
static int write_indent_internal(int len, int new_line, int ignore_xml_space);
static int write_plain_data(xchar* text, int len);
static int cprintf_init(charset_t *to_charset, FILE *file);
static int cprintf_close(void);
static int cprintf(char *format, ...);
static int cwrite(const char *buf, size_t num);
static int cputc(int c);
static void cflush(void);
static size_t ccount_utf8_chars(const char *buf, size_t num_bytes);

/* element insertion variables */
static tree_node_t *ins_html;
static tree_node_t *ins_head;
static tree_node_t *ins_body;


/* special HTML parameters specification */
static char lt_escaped[]="&lt;";
static char amp_escaped[]="&amp;";
static char gt_escaped[]="&gt;";
static char lt_normal[]="<";
static char amp_normal[]="&";
static char gt_normal[]=">";
static char eol_unix[] = {0x0a, 0};
static char eol_dos[] = {0x0a, 0x0d, 0};

/* escaped or normal characters */
static char* lt;
static char* amp;
static char* gt;
static char* eol;
static size_t eol_len; /* initialized to strlen(eol) in writeOutput */

static int escape_chars;

/* variables para gesti�n de IDs */
static xchar* id_list[ID_LIST_SIZE];
static int id_list_num;
static void set_node_att(tree_node_t *nodo, int att_id, xchar *value, int is_valid);
static int id_search(const xchar *value);



/**
 * inicia la conversi�n
 *
 */
void saxStartDocument(void)
{
  EPRINTF("SAX.startDocument()\n"); 

  if (state!=ST_START || state!=ST_END) {

    state= ST_PARSING;
    
    /* reset variables */
    doctype= -1;
    doctype_locked= 0;
    doctype_detected= 0;
    actual_element= NULL;
    num_errores= 0;
    ins_html= NULL;
    ins_head= NULL;
    ins_body= NULL;
    id_list_num = 0;
    new_place_recovery_on = 0;

/*     tree_init(); */
    
    document= new_tree_document(doctype, -1);
  }

/*   if (param_charset) */
/*     strcpy(document->encoding, param_charset_out); */
  if (param_doctype != -1)
    doctype_set(param_doctype, 1);
}



/**
 * finaliza la conversi�n 
 *
 * si hay alg�n error, finaliza mediante la macro exit
 */
void saxEndDocument(void)
{
  EPRINTF("SAX.endDocument()\n");

  if (!document) EXIT("no document found!");
  if (!document->inicio) EXIT("document without root html element");

  if (actual_element)
    for ( ; actual_element; actual_element= actual_element->padre)
      elm_close(actual_element);

  if (!document->inicio) EXIT("document discarded");

/*   if (!document->encoding[0] && !is_ascii && param_charset_default) { */
/*     strcpy(document->encoding,param_charset_default); */
/*     INFORM("Default charset set"); */
/*   } */

#ifdef SELLAR
  sellar_documento();
#endif

  if (state== ST_PARSING) state= ST_END;
  if (state!= ST_END) EXIT("bad state");
}


/**
 * se recibe un tag de inicio de elemento
 *
 * atts ser� un array de cadenas, que sucesivamente
 * contendr� pares nombre/valor de atributos
 *
 */
void saxStartElement(const xchar *fullname, xchar **atts)
{
  int elm_ptr;
  xchar elm_name[ELM_NAME_LEN];
  tree_node_t *nodo;

#ifdef MSG_DEBUG
#ifndef PRO_NO_DEBUG
  int i;

  EPRINTF1("SAX.startElement(%s", (char *) fullname);
  if (atts != NULL) {
    for (i = 0;(atts[i] != NULL);i++) {
      EPRINTF1(", %s='", atts[i++]);
      if (atts[i] != NULL)
	EPRINTF1("%s'", atts[i]);
    }
  }
  EPRINTF(")\n");
#endif
#endif

  if (state!=ST_PARSING) {
    if (state == ST_END)
      INFORM("Element discarded after the html end tag");
    return;
  }

  /* se pasa a min�sculas */
  xtolower(elm_name,fullname,ELM_NAME_LEN);

  /* busca el elemento */
  if ((elm_ptr= dtd_elm_search(elm_name))<0) {
    if ((elm_ptr= err_elm_desconocido(elm_name))<0) {
      INFORM("elemento no encontrado\n");
      return;
    }
  }

  if ((!actual_element) && (doctype<0)) doctype_set(XHTML_TRANSITIONAL, 0);


  /* �vale para este DTD? */
  if (!(elm_list[elm_ptr].environment & doctype_mask)) {
    /* puede que sea algo relacionado con frames, y que no se declare
     * como <frameset>
     */
    if ((elm_ptr==ELMID_FRAMESET)||(elm_ptr==ELMID_FRAME)) {
      tree_node_t *html, *elm;
      int res;

      elm= NULL;

      if (document->inicio) {
	html= document->inicio;
	for (elm=html->cont.elemento.hijo; elm && (ELM_ID(elm)!=ELMID_BODY); 
	     elm=elm->sig);
      }
      /* si no existe BODY, se sit�a como frameset */
      if (!elm) res= doctype_set(XHTML_FRAMESET,1);
      if (elm || (res==-1)) {
	INFORM("elemento de tipo frameset no v�lido en este DTD (descartado)");
	return;
      }
    } else {
      INFORM("elemento no v�lido en este DTD (descartado)\n");
      return;
    }
  }


  if (!actual_element) {
    if (elm_ptr != ELMID_HTML)
      if (!err_html_struct(document, elm_ptr))
	EXIT("Does not begin with 'html'! Incorrect structure\n");
  }


  /* establece un nodo para el elemento */
  nodo= new_tree_node(Node_element);
  nodo->cont.elemento.elm_id= elm_ptr;


  /* a lo mejor no se puede insertar por haber sido insertado por
   * el conversor previamente, se usan los atributos del nuevo
   */ 
  if ((elm_ptr==ELMID_HTML)&&(ins_html)) {
    nodo= ins_html;
    actual_element= nodo;
    ins_html= NULL;
    DEBUG("insertado previamente por el conversor");
  } else if ((elm_ptr==ELMID_HEAD)&&(ins_head)) {
    nodo= ins_head;
    actual_element= nodo;
    ins_head= NULL;
    DEBUG("insertado previamente por el conversor");
  } else if ((elm_ptr==ELMID_BODY)&&(ins_body)) {
    nodo= ins_body;
    actual_element= nodo;
    ins_body= NULL;
    DEBUG("insertado previamente por el conversor");
  } else 
    /* inserta el nodo */
    if (insert_element(nodo) <0 ) {
      INFORM("elemento no insertado");
      return;
  }

  /* atributos */
  set_attributes(nodo, atts);

  /* If meta with att-equiv, remove it: it is unnecessary in XML */
  if (elm_ptr == ELMID_META) {
    att_node_t* att = tree_node_search_att(nodo, ATTID_HTTP_EQUIV_0);
    if (!att) {
      att = tree_node_search_att(nodo, ATTID_HTTP_EQUIV_1);
    }
    if (att) {
      tree_unlink_node(nodo);
    }
  }
}



/**
 * se encuentra un tag de finalizaci�n de elemento
 *
 */
void saxEndElement(const xchar *name)
{
  int elm_ptr;
  xchar elm_name[ELM_NAME_LEN];
  tree_node_t *nodo;

  EPRINTF1("SAX.endElement(%s)\n",name);

  if (state!=ST_PARSING) return;

  xtolower(elm_name,name,ELM_NAME_LEN);
  if ((elm_ptr= dtd_elm_search(elm_name))<0) {
    if ((elm_ptr= err_elm_desconocido(elm_name))<0) {
      INFORM("elemento no encontrado\n");
      return;
    }
  }

  /* busca al antecesor que coincida */
  for (nodo=actual_element; (nodo) && (ELM_ID(nodo)!=elm_ptr); 
       nodo=nodo->padre);
  /*elm_close(nodo);*/

  /* nodo= tree_search_elm_up(actual_element, elm_ptr); */

  if (!nodo) DEBUG("cerrado elemento no abierto")
  else {
    tree_node_t *p;

    /* se cierran todos los nodos hasta llegar a este */
    for (p=actual_element; p != nodo; p=p->padre) elm_close(p);

    /* cierra el nodo y actualiza actual_element */
    elm_close(nodo);
    if (!new_place_recovery_on || new_place_recovery_elm != actual_element) {
      actual_element= nodo->padre;
    } else {
      new_place_recovery_on = 0;
      actual_element = new_place_recovery_father;
    }
    if (!actual_element) state= ST_END;
  }
}



/**
 * se encuentra una referencia a car�cter o
 * a entidad
 *
 */
void saxReference(const xchar *name)
{
  EPRINTF1("SAX.reference(%s)\n",name);

  if (state!=ST_PARSING || !actual_element) return;

  /* si es una referencia a entidad, se comprueba que sea v�lida */
  if (name[1]!='#') 
    if (dtd_ent_search(name)==-1) {
      if (!strcmp(name, "&percnt;")) {
	 insert_chardata("%", 1, Node_chardata);
      } else
	INFORM("referencia a entidad desconocida");
      return;
    }

  /* en este punto, o es una referencia a caracter o es
   * una referencia a entidad conocida:
   * se introduce en el documento xhtml
   */
  insert_chardata(name,xstrlen(name), Node_chardata);
}



/**
 * Notificaci�n de informaci�n textual
 * Se recibe la cadena con el texto
 * y el n�mero de caracteres
 *
 */
void saxCharacters(const xchar *ch, int len)
{
  EPRINTF2("SAX.characters(%d)[%s]\n",len,ch);

  if (state!=ST_PARSING || !actual_element) return;

  insert_chardata(ch, len, Node_chardata);
}

/**
 * CDATA section (when input is XML)
 *
 */
void saxCDataSection(const xchar *ch, int len)
{
  node_type_t type;

  EPRINTF2("SAX.cdatasection(%d)[%s]\n",len,ch);

  if (state!=ST_PARSING || !actual_element) return;

  /* by default, mark as CDATA section */
  type = Node_cdata_sec;

  /* but if it is inside style and does not contain & or < 
   * then insert it as normal character data 
   */
  if (ELM_ID(actual_element) == ELMID_STYLE
      && !text_contains_special_chars(ch, len))
    type = Node_chardata;

  insert_chardata(ch, len, type);
}

void saxWhiteSpace()
{
  EPRINTF("SAX.whitespace()\n");

  if (state!=ST_PARSING) return;

  /* insert the whitespace only if the actual element has
     mixed contenttype. If not, ignore it because it is irrelevant. */
  if (actual_element 
      && ELM_PTR(actual_element).contenttype[doctype] == CONTTYPE_MIXED) {
    xchar *data = tree_malloc(1);
    data[0] = 0x20; /* a whitespace */
    insert_chardata(data, 1, Node_chardata);
  }
}


/**
 * Se encuentra un comentario
 * Se recibe el texto del comentario
 *
 */
void saxComment(const xchar *value)
{
/*   tree_node_t *nodo; */

  EPRINTF1("SAX.comment(%s)\n",value);

  if (state!=ST_PARSING) {
    INFORM("Comment discarded");
    return;
  }
  
  if (actual_element)
    tree_link_data_node(Node_comment, actual_element, value, strlen(value));
  else
    INFORM("comentario antes de elemento ra�z");
}



/**
 * se encuentra marcaci�n <!DOCTYPE
 * se recibe todo el contenido
 *
 */
void saxDoctype(const xchar *data)
{
  int doc;

  EPRINTF("SAX.doctype\n");

#if 0
  fprintf(stderr, "SAX.doctype(");
#endif

  if (state!=ST_PARSING) return;

  if (!doctype_detected) {
    doctype_detected= 1;
    /* try to guess the document type */
    if ((doc=doctype_scan(data))!=-1) doctype_set(doc,0);
  } else {
    INFORM("More than one doctype found: only the first one is processed");
  }
}


/**
 * XML Processing Instruction
 *
 * e.g. <?xml version="1.0" encoding="iso-8859-1" ?>
 *
 */
void saxXmlProcessingInstruction(const xchar *fullname, xchar **atts)
{
/*   xchar pi_name[ELM_NAME_LEN]; */
/*   xchar att_name[ATT_NAME_LEN]; */
/*   int i, j; */
/*   int encoding_set; */

#ifdef MSG_DEBUG
#ifndef PRO_NO_DEBUG
  EPRINTF1("SAX.xmlProcessingInstruction(%s", (char *) fullname);
  if (atts != NULL) {
    int k;
    for (k = 0;(atts[k] != NULL);k++) {
      EPRINTF1(", %s='", atts[k++]);
      if (atts[k] != NULL)
	EPRINTF1("%s'", atts[k]);
    }
  }
  EPRINTF(")\n");
#endif
#endif

/*   encoding_set = 0; */

/*   /\* convert to lowercase the PI name *\/ */
/*   xtolower(pi_name, fullname, ELM_NAME_LEN); */

/*   if (!strncmp("xml", pi_name, 4)) { */
/*     /\* <?xml ... ?> Look for encoding *\/ */
/*     if (atts) { */
/*       for (i=0; atts[i]; i+=2) { */
/* 	xtolower(att_name, atts[i], ATT_NAME_LEN); */
/* 	if (!strncmp("encoding", att_name, 9)) { */
/* 	  if (!document->encoding[0] && xstrnlen(atts[i+1], 33) < 32) { */
/* 	    /\* convert encoding to uppercase *\/ */
/* 	    for (j=0; atts[i+1][j]; j++) { */
/* 	      if ((atts[i+1][j]>='a')&&(atts[i+1][j]<='z')) */
/* 		document->encoding[j]= atts[i+1][j] - 0x20; */
/* 	      else  */
/* 		document->encoding[j]= atts[i+1][j]; */
/* 	    } */
/* 	    document->encoding[j]= 0; */
/* 	    encoding_set = 1; */
/* 	    DEBUG("saxXmlProcessingInstruction()"); */
/* 	    EPRINTF1("   encoding=%s\n",document->encoding); */
/* 	  } else { */
/* 	    INFORM("Discarded encoding in saxXmlProcessingInstruction()"); */
/* 	    EPRINTF1("   encoding=%s\n", atts[i+1]);	     */
/* 	  } */
/* 	} */
/*       } /\* for *\/ */
/*     } /\* if (atts) *\/ */
/*     if (!encoding_set) { */
/*       strcpy(document->encoding, "UTF-8"); */
/*     } */
/*   } /\* if <?xml *\/ */
}



void saxError(xchar *data)
{
  EPRINTF1("SAX.error(%s)\n",data);
  num_errores++;

  INFORM("error en el documento de entrada");
  EPRINTF2("   texto '%s' [%x...] no emparejado\n",data,data[0]);

  if (num_errores>MAX_NUM_ERRORS) 
    EXIT("too many errors in the lexer");
}


/**
 * Write the output XHTML document
 *
 * returns 0 (OK) or < 0 (error)
 */
int writeOutput() 
{
  if (state != ST_END) return -1;
  if (!document) return -2;
  
  escape_chars= param_cgi_html_output;
  if (!param_cgi_html_output) {
    gt= gt_normal;
    lt= lt_normal;
    amp= amp_normal;
  } else {
    gt= gt_escaped;
    lt= lt_escaped;
    amp= amp_escaped;
  }

  if (!param_crlf_eol)
    eol = eol_unix;
  else
    eol = eol_dos;
  eol_len = strlen(eol);

  /* vuelca la salida */
  write_document(document);
  return 0;
}

/*
 * libera la memoria asignada en la conversi�n
 *
 */
void freeMemory()
{
  tree_free();
  state = ST_END;
}




/*
 * -------------------------------------------------------------------
 * funciones internas
 * -------------------------------------------------------------------
 *
 */

/*
 * busca el tipo de salida a generar
 * y el tipo de entrada a partir de
 * <!DOCTYPE ...>
 *
 */
static int doctype_scan(const xchar *data)
{
  char buffer[512];
  int i;

  /* if the document is already XHTML, maintain its doctype
   * by default 
   */
  strncpy(buffer, data, 512);
  for (i = 0; i < DTD_NUM; i++) {
    if (xsearch(buffer, dtd_string[i]))
      return i;
  }

  /* if not, try to detect HTML doctypes */
  xtolower(buffer, data, 512);
  if (xsearch(buffer, "transitional")) return XHTML_TRANSITIONAL;
  else if (xsearch(buffer, "loose.dtd")) return XHTML_TRANSITIONAL;
  else if (xsearch(buffer, "strict")) return XHTML_STRICT;
  else if (xsearch(buffer, "frameset")) return XHTML_FRAMESET;
  else return -1;
}



/*
 * establece doctype al valor 'type', que puede
 * ser: XHTML_TRANSITIONAL, XHTML_STRICT, XHTML_FRAMESET
 *
 * si 'lock', no se permite volver a cambiarlo
 *
 * si hace el cambio, devuelve 0
 * si el cambio no se puede realizar por estar bloqueado, devuelve -1
 *
 */
static int doctype_set(int type, int lock)
{
  char msg[256];

  if (doctype_locked) return -1;

  sprintf(msg,"establecido doctype %d",type);
  INFORM(msg);

  if (lock) doctype_locked= 1;
  doctype= type;
  doctype_mask= DTD_MASK(type);
  if (document) document->xhtml_doctype= type;

  return 0;
}





/**
 * codifica en el nodo de elemento los
 * atributos de la lista, que viene en pares
 * nombre-valor
 *
 * por ejemplo:
 *   atts[0] -> nombre del atributo 1
 *   atts[1] -> valor del atributo 1
 *   atts[2] -> nombre del atributo 2
 *   atts[3] -> valor del atributo 2
 * (...)
 *
 */
static void set_attributes(tree_node_t *elm, xchar **atts)
{
  int elm_ptr;
  int i;
  xchar att_name[ATT_NAME_LEN];
  int att_ptr;

  elm_ptr= elm->cont.elemento.elm_id;

  /* atributos */
  if (atts) {
    for (i=0; atts[i]; i+=2) {
      xtolower(att_name,atts[i],ATT_NAME_LEN);
      if ((att_ptr=
	   dtd_att_search_list(att_name,elm_list[elm_ptr].attlist[doctype])
	   )<0) {
	INFORM("");
	EPRINTF1("\"%s\" atributo no encontrado en este DTD\n",att_name);
      }
      else {
	/* check that att is not already in the element */
	if (!tree_node_search_att(elm, att_ptr)) {

	  /* se comprueba si el valor est� bien formado, y se corrige si no */
	  atts[i+1] = check_and_fix_att_value(atts[i+1]);
	  
	  /* atributo v�lido ��comprobar valor y tipo!! */
	  switch (dtd_att_is_valid(att_ptr, atts[i+1])) {
	  case 1: /* ok */
	    set_node_att(elm, att_ptr, atts[i+1], 1);
	    break;
	  case 2: /* ok, pero en min�sculas */
	    {
	      xchar lowercase[128];
	      xtolower(lowercase, atts[i+1], 128);
	      set_node_att(elm, att_ptr, lowercase, 1);
	    }
	    break;
	  case 0:
	    /* valor incorrecto, se intenta arreglar */
	    if (!err_att_value(elm, att_ptr, atts[i+1])) {
	      INFORM("");
	      EPRINTF1("\"%s\" atributo con valor incorrecto\n",att_name);
	    }
	    break;
	  }
	}
      } /* else */
    } /* for i */
  }


  /* 
   * ahora hay que comprobar que no falte ning�n valor que
   * sea #REQUIRED
   *
   */
  for (i=0, att_ptr= elm_list[elm_ptr].attlist[doctype][0];
       att_ptr>=0;
       att_ptr= elm_list[elm_ptr].attlist[doctype][++i]) {
    
    if (att_list[att_ptr].defaultDecl== DEFDECL_REQUIRED) {
      if (!tree_node_search_att(elm, att_ptr)) {
	/* intenta arreglarlo */
	if (!err_att_req(elm, att_ptr,atts)) {
	  WARNING("atributo obligatorio no especificado");
	  EPRINTF1("      \"%s\"\n",att_list[att_ptr].name);
	}
      }
    } /* if REQUIRED */
  } /* for */ 


  /* rutina por defecto de detecci�n y 
   * correcci�n de otros errores 
   */
  err_att_default(elm,atts);

  


}








/*
 * inserta un nodo de elemento en el �rbol
 * del documento
 *
 * comprueba si es un contenido v�lido y busca
 * d�nde insertarlo si no lo es
 *
 * si no le encuentra hueco, devuelve <0
 *
 */ 
int insert_element(tree_node_t *nodo)
{
  int elm_ptr;
  int insertado;

  elm_ptr= ELM_ID(nodo);

  if (!actual_element) {
    link_root_node(document, nodo);
    insertado= 1;
  }
  else {
    if (dtd_can_be_child(elm_ptr,ELM_ID(actual_element),doctype)) {
      /* comprueba las prohibiciones de elementos de XHTML */
      if (!elm_check_prohibitions(elm_ptr)) {
	DEBUG("elemento descartado: no cumple las excepciones de XHTML");
	insertado= 0;
      
      } else insertado= 1;
    }
    else insertado= err_child_no_valid(nodo);
    


    if (insertado == 1) {
      link_node(nodo, actual_element, LINK_MODE_CHILD);
      DEBUG("insert_element()");
      EPRINTF1("   insertado elemento %s correctamente\n",ELM_PTR(nodo).name);
    } else if (insertado == 0) {
      DEBUG("elemento descartado:");
      EPRINTF1("   %s\n",ELM_PTR(nodo).name);
    }
    /* if (insertado == 2) -> inserted by err_child_no_valid in another place */

  } /* else */

  /* si es un elemento vac�o, se cierra */
  if (insertado) {
    if (elm_list[elm_ptr].contenttype[doctype]!=CONTTYPE_EMPTY) {
      actual_element= nodo;
    }
    else elm_close(nodo);
  }
  
  /* si es html se inserta xmlns */
  if (insertado && elm_ptr== ELMID_HTML) {
    set_node_att(nodo, ATTID_XMLNS, 
		      dtd_att_read_buffer(att_list[ATTID_XMLNS].defaults), 1);
  }

  if (insertado) return 0;
  else return -1;
}



/*
 * realiza a cabo tareas relacionadas con el cierre de
 * un elemento
 *
 *
 */
static void elm_close(tree_node_t *nodo)
{
  DEBUG("elm_close()");
  EPRINTF1("cerrando elemento %s\n",ELM_PTR(nodo).name);

  if (ELM_PTR(nodo).contenttype[doctype]==CONTTYPE_CHILDREN) {
    /* si es de tipo child se comprueba su contenido */
    int content[16384];
    int i, num;
    tree_node_t *elm;

    for (i=0, num=0, elm= nodo->cont.elemento.hijo;
	 (i<16384) && elm;
	 i++, elm= elm->sig)
      if (elm->tipo==Node_element) content[num++]= ELM_ID(elm);

    
    if (i==16384) EXIT("internal error: variable 'i' overflow");
    if (dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],content,num)!=1) {
      /* children no v�lido: a intentar corregirlo */
      if (!err_content_invalid(nodo,content,num))
	WARNING("invalid element content");
    }
    else DEBUG("child v�lido");
  }
}





static void insert_chardata(const xchar *ch, int len, node_type_t type)
{
/*   tree_node_t *nodo; */

  if (!actual_element) {
    num_errores++;
    if (num_errores>MAX_NUM_ERRORS) EXIT("too many errors");
    return;
  }

  /* �si el elemento no es de tipo mixed? */
  if (ELM_PTR(actual_element).contenttype[doctype]!=CONTTYPE_MIXED) {
    if (dtd_can_be_child(ELMID_P, ELM_ID(actual_element),doctype)) {
      /* inserta un elemento <p> como contenedor */
      tree_node_t *p;

      p= new_tree_node(Node_element);
      p->cont.elemento.elm_id= ELMID_P;
      link_node(p, actual_element, LINK_MODE_CHILD);
      actual_element= p;
      DEBUG("[ERR] insertado elemento <p> para contener PCDATA");
    } else 
      /* si el padre es <ul> o <ol>, se inserta <li> */ 
	if((ELM_ID(actual_element)==ELMID_UL)
	   ||(ELM_ID(actual_element)==ELMID_OL)) {
	actual_element= err_aux_insert_elm(ELMID_LI,NULL,0);
	DEBUG("[ERR] insertado elemento li");
    } else {
      INFORM("intento de introducir datos en tipo no mixed");
      return;
    }
  }

  tree_link_data_node(type, actual_element, ch, len);

/*   /\* se crea un nodo para los datos *\/ */
/*   nodo= new_tree_node(Node_chardata); */

/*   link_node(nodo, actual_element, LINK_MODE_CHILD); */

/*   /\* le pasa los datos al nodo *\/ */
/*   tree_set_node_data(nodo, ch, len); */
}




/*
 * Returns 1 if at least one '&' or one '<' appear in text
 * or 0 otherwise
 */
static int text_contains_special_chars(const xchar *ch, int len)
{
  int i;

  for (i = 0; i < len; i++) {
    if (ch[i] == '&' || ch[i] == '<')
      return 1;
  }
  return 0;
}









/*
 * recibe un elemento de tipo META
 *
 * - si contiene http-equiv (Content-Type) se busca
 *    informaci�n sobre el sistema de codificaci�n de 
 *    caracteres
 *
 */
/* static void elm_meta_scan(tree_node_t *meta) */
/* { */
/*   att_node_t *att; */

/*   DEBUG("elm_meta_scan()"); */

/*   /\* busca http-equiv *\/ */
/*   att= tree_node_search_att(meta,ATTID_HTTP_EQUIV_0); */
/*   if (!att) att= tree_node_search_att(meta,ATTID_HTTP_EQUIV_1); */

/*   if (att) { */
/*     char *attval,*cad; */
/*     att_node_t *content; */

/*     attval= tree_index_to_ptr(att->valor); */
/*     if (attval && !strcasecmp(attval,"Content-Type")  */
/* 	&& ((content=tree_node_search_att(meta,ATTID_CONTENT)))){ */
     
/*       attval= tree_index_to_ptr(content->valor); */
/*       if (attval && ((cad=strstr(attval,"charset=")))) { */
/* 	for ( ; *cad!='='; cad++); */
/* 	if (!document->encoding[0] && (xstrnlen(cad, 33)<32)) { */
/* 	  int i; */
/* 	  cad++; */
/* 	  /\* pasa a may�sculas *\/ */
/* 	  for (i=0; cad[i]; i++) */
/* 	    if ((cad[i]>='a')&&(cad[i]<='z')) */
/* 	      document->encoding[i]= cad[i] - 0x20; */
/* 	    else document->encoding[i]= cad[i]; */
/* 	  document->encoding[i]= 0; */

/* 	  DEBUG("elm_meta_scan()"); */
/* 	  EPRINTF1("   encoding=%s\n",document->encoding); */
/* 	} else { */
/* 	  INFORM("elm_meta_scan()"); */
/* 	  EPRINTF1("   se descarta encoding=%s\n",cad); */
/* 	} */
/*       } */
/*     } */
/*   } /\* if http-equiv *\/ */
/* } */




#if 0
static void sellar_documento(void)
{
  tree_node_t *sello;
  int          att_ptr;

  sello= new_tree_node(Node_element);
  sello->cont.elemento.elm_id= ELMID_META;

  att_ptr=dtd_att_search_list("name",elm_list[ELMID_META].attlist[doctype]);
  set_node_att(sello,att_ptr,"filter",1);

  att_ptr=dtd_att_search_list("content",elm_list[ELMID_META].attlist[doctype]);
#ifdef SELLAR
  set_node_att(sello,att_ptr,SELLO,1);
#endif

  if (document->inicio && document->inicio->cont.elemento.hijo &&
      ELM_ID(document->inicio->cont.elemento.hijo)==ELMID_HEAD)
    link_node(sello,document->inicio->cont.elemento.hijo,LINK_MODE_CHILD);
}
#endif




/*
 * chequea si se cumplen las excepciones recogidas en la norma
 * de XHTML en el ap�ndice B de la norma de XHTML 1.0
 *
 * devuelve 1 si es correcto o 0 si incumple la norma
 * 
 */
static int elm_check_prohibitions(int elmid)
{
  switch (elmid) {
  case ELMID_A:
    return !elm_is_father(ELMID_A,actual_element);
  case ELMID_IMG:
  case ELMID_OBJECT:
  case ELMID_BIG:
  case ELMID_SMALL:
  case ELMID_SUB:
  case ELMID_SUP:
    return !elm_is_father(ELMID_PRE,actual_element);
  case ELMID_INPUT:
  case ELMID_SELECT:
  case ELMID_TEXTAREA:
  case ELMID_BUTTON:
  case ELMID_FIELDSET:
  case ELMID_IFRAME:
  case ELMID_ISINDEX:
    return !elm_is_father(ELMID_BUTTON,actual_element);
  case ELMID_LABEL:
    return ((!elm_is_father(ELMID_LABEL,actual_element))
	    &&(!elm_is_father(ELMID_BUTTON,actual_element)));
  case ELMID_FORM:
    return ((!elm_is_father(ELMID_BUTTON,actual_element))
	    &&(!elm_is_father(ELMID_FORM,actual_element)));
  default:
      return 1;
  }
}


/*
 * comprueba si alg�n elemento que contenga a 'nodo', 
 * o el propio elemento 'nodo', tiene identificador 'elmid'
 *
 * devuelve 1 si es as�, o 0 si no
 *
 */
static int elm_is_father(int elmid, tree_node_t *nodo)
{
  for ( ; nodo ; nodo= nodo->padre)
    if (ELM_ID(nodo)==elmid) return 1;

  return 0;
}



void set_node_att(tree_node_t *nodo, int att_id, xchar *value, int is_valid)
{
  xchar* new_value = value;
  xchar *tmp_value;
  int len = strlen(value);

  if (att_list[att_id].attType== ATTTYPE_ID) {
    /* si es ID, comprueba que no se repita y registra el valor */
    while (id_search(new_value)) {
      len++;
      tmp_value = (xchar*) tree_malloc(len + 1);
      strcpy(tmp_value, new_value);
      tmp_value[len - 1] = '_';
      tmp_value[len] = 0;
      new_value = tmp_value;
    }

    /* se a�ade el valor a la lista */
    if (id_list_num < ID_LIST_SIZE - 1)
      id_list[id_list_num++] = new_value;
    else
      WARNING("lista de atributos id desbordada");
      /* jjjjjjjj */
  }

  tree_set_node_att(nodo, att_id, new_value, is_valid);
}

int id_search(const xchar* value)
{
  int i;

  for (i = 0; i < id_list_num; i++) {
    if (!strcmp(id_list[i], value))
      return 1;
  } 

  return 0;
}







/*
 * ===================================================================
 * FUNCIONES PARA GESTI�N DE ERRORES
 * ===================================================================
 *
 */

/*
 * Funciones que se ejecutan para atender
 * casos de error en el documento HTML 
 * e intentar arreglarlos
 *
 * En general, estas funciones devuelven 0 si
 * no arreglaron el error o >0 si lo hicieron
 *
 * Estas funciones ser�n invocadas cuando, procesando
 * el documento de entrada, el programa se encuentre
 * con problemas que no puede solucionar con los
 * procedimientos est�ndar
 *
 * Por ejemplo, si un atributo obligatorio no
 * aparece en el documento HTML, la funci�n err_att_req
 * intentar� obtener un valor v�lido para ella.
 * Si lo consigue, inserta en el �rbol del documento
 * el atributo con dicho valor. Si no, no hace nada
 * y devuelve 0 para indicarlo a quien la invoc�
 * 
 * Cuando, probando, se encuentren casos que el
 * procedimiento est�ndar no soluciona, debe modificarse
 * en este fichero la funci�n adecuada para gestionar
 * dichos casos
 *
 */





/**
 * si un elemento es de tipo children y su contenido no verifica
 * el modelo, se intenta corregir en esta funci�n
 *
 * devuelve 0 si no lo consigue y 1 si lo hace
 *
 */
static int err_content_invalid(tree_node_t* nodo, int hijos[], int num_hijos)
{
  tree_node_t* actual_bak= actual_element;
  int corregido= 0;
  int check_again = 0;

  DEBUG("[ERR] err_content_invalid()");

  switch (ELM_ID(nodo)) {
  case ELMID_HTML:
      {
	int in_body;
	
	in_body= ELMID_PRE;
	if (doctype==XHTML_FRAMESET) in_body= ELMID_FRAME;
	
	/* si est� mal HTML, intenta arreglarlo */
	if (!num_hijos) {
	  /* faltan HEAD y BODY */
	  actual_element= nodo;
	  corregido= err_html_struct(document,in_body);;
	}
	else if ((num_hijos==1)&&(hijos[0]==ELMID_HEAD)) {
	  /* falta BODY */
	  actual_element= nodo->cont.elemento.hijo;
	  corregido= err_html_struct(document,in_body);
	}
	else if ((num_hijos==1)&&
		 ((hijos[0]==ELMID_BODY)||(hijos[0]==ELMID_FRAMESET))) {
	  /* falta HEAD */
	  tree_node_t *body;
	  if (!(body= tree_search_elm_child(nodo,ELMID_BODY)))
	    body= tree_search_elm_child(nodo,ELMID_FRAMESET);
	  if (body) {
	    tree_unlink_node(body);
	    actual_element= nodo;
	    corregido= err_html_struct(document,ELMID_BODY);
	    link_node(body,nodo,LINK_MODE_CHILD);
	  }
	}
	

	actual_element= actual_bak;
	break;
      }

  case ELMID_HEAD:
    {
      int num_title = 0;
      int num_base = 0;
      int i;
      int* h;

      /* Count the number of title and base elements */
      for (i = 0; i < num_hijos; i++) {
	if (hijos[i] == ELMID_TITLE)
	  num_title++;
	else if (hijos[i] == ELMID_BASE) {
	  num_base++;
	}
      }

      if (num_title == 0) {
	/* Element title missing */
	h= tree_malloc((num_hijos + 1)*sizeof(int));
	for (i = 0; i < num_hijos; i++)
	  h[i + 1] = hijos[i];
	h[0]= ELMID_TITLE;
	if (dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],
			       h, num_hijos + 1) == 1) {
	  tree_node_t* title;
	  title = new_tree_node(Node_element);
	  title->cont.elemento.elm_id = ELMID_TITLE;
	  link_node(title, nodo, LINK_MODE_FIRST_CHILD);
	  corregido = 1;
	}
      } else if (num_title > 1) {
	/* More than one title, keep only the first one */
	num_hijos -= remove_duplicate_elm(ELMID_TITLE, nodo, hijos);
	check_again = 1;
      }

      if (num_base > 1) {
	/* More than one base, keep only the first one */
	num_hijos -= remove_duplicate_elm(ELMID_BASE, nodo, hijos);
	check_again = 1;
      }
      break;
    }
    
  case ELMID_OL:
  case ELMID_UL:
    {
      /* se descarta, porque no contiene informaci�n */
      tree_unlink_node(nodo);
      INFORM("[ERR] descartado el elemento");
      corregido=1;
      break;
    }

  case ELMID_TR:
    {
      /* se inserta TD a pesar de que no contiene
       * informaci�n, porque puede propagar el problema
       * de contenido inv�lido hacia sus padres
       */
      tree_node_t *td;
      td= new_tree_node(Node_element);
      td->cont.elemento.elm_id= ELMID_TD;
      link_node(td,nodo,LINK_MODE_CHILD);
      corregido= 1;
      INFORM("insertado elemento td");
      break;
    }
  case ELMID_TABLE:
  case ELMID_TBODY:
  case ELMID_THEAD:
    {
      /* si no tiene contenido, se cierra */
      /* se intenta solucionar con <TR> al final */
      int *hijos2= tree_malloc((num_hijos+1)*sizeof(int));
      tree_node_t *tr,*td;
      
      if (!num_hijos) {
	tree_unlink_node(nodo);
	corregido= 1;
	INFORM("[ERR] descartado el elemento");
      } else {
	memcpy(hijos2,hijos,num_hijos*sizeof(int));
	hijos2[num_hijos]= ELMID_TR;
	if (dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],
			       hijos2,num_hijos+1)==1) {
	  /* OK, se inserta */
	  tr= new_tree_node(Node_element);
	  tr->cont.elemento.elm_id= ELMID_TR;
	  link_node(tr,nodo,LINK_MODE_CHILD);
	  td= new_tree_node(Node_element);
	  td->cont.elemento.elm_id= ELMID_TD;
	  link_node(td,tr,LINK_MODE_CHILD);
	  corregido= 1;
	  INFORM("[ERR] insertados <TR><TD></TD></TR>");
	}
      }

/*       free(hijos2); */
      break;
    }
  }

  if (!corregido && check_again) {
    corregido = dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],
				   hijos, num_hijos);
  }

  if (!corregido && param_strict) {
    /* se descarta el elemento */
    if (nodo!=document->inicio) tree_unlink_node(nodo);
    else document->inicio= NULL;
    INFORM("se descarta el elemento por no tener contenido v�lido");
    corregido= 1;
  }

  return corregido;
}

static int remove_duplicate_elm(int elmid, tree_node_t* parent, int hijos[])
{
  tree_node_t* p;
  tree_node_t* next;
  int keep_node;
  int removed;
  int i;

  removed = 0;
  keep_node = 1;
  for (i = 0, p = parent->cont.elemento.hijo; p; i++) {
    next = p->sig;
    if (p->tipo == Node_element && ELM_ID(p) == elmid) {
      if (!keep_node) {
	tree_unlink_node(p);
	removed++;
      } else {
	keep_node = 0;
      }
    } else {
      hijos[i - removed] = hijos[i];
    }
    p = next;
  }

  return removed;
}




/** 
 * el elemento no es un hijo v�lido para el nodo actual
 * se intenta solucionar esto
 *
 */ 
static int err_child_no_valid(tree_node_t* nodo)
{
  int insertado;
  tree_node_t* actual;

  EPRINTF1("err_child_no_valid(%s)\n", ELM_PTR(nodo).name);

  /* si el padre es <ul> o <ol>, se inserta <li> */
  if (((ELM_ID(actual_element)==ELMID_OL)||(ELM_ID(actual_element)==ELMID_UL))
      && (dtd_can_be_child(ELM_ID(nodo),ELMID_LI,doctype))){
    actual_element= err_aux_insert_elm(ELMID_LI,NULL,0);
    DEBUG("[ERR] insertado elemento li");
    return 1;
  } 

  /* si el padre es <table> y se inserta <th> o <td>, se inserta
   * antes <tr>
   */
  if (((ELM_ID(actual_element)==ELMID_TABLE)||
       (ELM_ID(actual_element)==ELMID_THEAD)||
       (ELM_ID(actual_element)==ELMID_TBODY) ) &&
      ((ELM_ID(nodo)==ELMID_TH)||(ELM_ID(nodo)==ELMID_TD))) {
      actual_element= err_aux_insert_elm(ELMID_TR,NULL,0);
      DEBUG("[ERR] insertado elemento tr");
      return 1;
  } 

  /* si el elemento est� dentro de TABLE, THEAD, TBODY o TR se descarta */
  if ((ELM_ID(actual_element)==ELMID_TABLE)||
      (ELM_ID(actual_element)==ELMID_THEAD)||
      (ELM_ID(actual_element)==ELMID_TBODY)||
      (ELM_ID(actual_element)==ELMID_TR)) {
    INFORM("[ERR] el elemento se descarta por estar en una tabla");
    return 0;
  }



  /* en principio, se asciende en la jerarqu�a hasta
   * que sea un hijo v�lido
   */
  for (insertado=0, actual= actual_element; actual; actual= actual->padre) {
    if (dtd_can_be_child(ELM_ID(nodo),ELM_ID(actual),doctype)) {
      insertado= 1;
      break;
    }
  } /* for */
  
  /* si se le encontr� hueco, se cierran los elementos hasta �l */
  if (insertado) {
    /* control de estructura */
    if ((ELM_ID(nodo)==ELMID_HEAD)
	|| (ELM_ID(nodo)==ELMID_BODY)
	|| (ELM_ID(nodo)==ELMID_FRAMESET)) insertado=0;
    else for ( ; actual_element != actual; actual_element= actual_element->padre)
      elm_close(actual_element);
  } else {
    /* puede ser un caso de estructura incorrecta */
    insertado= err_html_struct(document, ELM_ID(nodo));
    if (!insertado && actual_element && 
	dtd_can_be_child(ELM_ID(nodo),ELMID_P,doctype) &&
	dtd_can_be_child(ELMID_P,ELM_ID(actual_element),doctype)) {
      /* se inserta en <p> */
      tree_node_t *p;
      
      p= new_tree_node(Node_element);
      p->cont.elemento.elm_id= ELMID_P;
      link_node(p, actual_element, LINK_MODE_CHILD);
      actual_element= p;
      insertado= 1;
      DEBUG("[ERR] insertado elemento p como padre");
    } else if (ELM_ID(actual_element) == ELMID_HTML
	       && (actual = tree_search_elm_child(actual_element, ELMID_BODY))) {
      if (dtd_can_be_child(ELM_ID(nodo), ELMID_BODY, doctype)) {
	/* Insert the new element inside the body element */
	actual_element = actual;
	insertado = 1;
      } else if (dtd_can_be_child(ELM_ID(nodo), ELMID_P, doctype)) {
	/* Insert the new element inside a new p element inside the
	 * body element.
	 */
	tree_node_t *p;
	p= new_tree_node(Node_element);
	p->cont.elemento.elm_id= ELMID_P;
	link_node(p, actual, LINK_MODE_CHILD);
	actual_element = p;
	insertado = 1;
      }
    }
  }

  if (!insertado && ELM_ID(nodo) == ELMID_STYLE) {
    /* si es un elemento style, lo metemos dentro de head */

    /* el primer hijo de <html> es <head> */
    tree_node_t* head = document->inicio->cont.elemento.hijo;
    link_node(nodo, head, LINK_MODE_CHILD);
    insertado = 2;
    
    /* activate the new place recovery mode */
    new_place_recovery_on = 1;
    new_place_recovery_elm = nodo;
    new_place_recovery_father = actual_element;

    DEBUG("Inserted style in head");
  }

  return insertado;
}



/**
 * intenta corregir los casos en que el valor de un atributo no es
 * correcto
 *
 * devuelve 0 si no lo consigue
 *
 */ 
static int err_att_value(tree_node_t *elm, int att_id, const xchar *value)
{
  char *correcto= NULL;

  /* CASO 1: valor 'center' en lugar de 'middle' */
  if (!strcasecmp(value,"center") && (dtd_att_is_valid(att_id, "middle")))
    correcto= tree_strdup("middle");

  /* CASO 2: valor contiene '<' o '&' */
  else if (dtd_att_val_search_errors(value)!=-1) {
    int pos;
    char *tmp;
    correcto= tree_strdup(value);
    while ((pos=dtd_att_val_search_errors(correcto))!=-1) {
      if (correcto[pos]=='&') {
	tmp= tree_malloc(strlen(correcto)+1+4);
	memcpy(tmp,correcto,pos);
	tmp[pos]= 0;
	strcat(tmp,"&amp;");
	strcat(tmp,&correcto[pos+1]);
/* 	free(correcto); */
	correcto= tmp;
      } else if (correcto[pos]=='<') {
	tmp= tree_malloc(strlen(correcto)+1+3);
	memcpy(tmp,correcto,pos);
	tmp[pos]= 0;
	strcat(tmp,"&lt;");
	strcat(tmp,&correcto[pos+1]);
/* 	free(correcto); */
	correcto= tmp;
      } else {
/* 	free(correcto); */
	correcto= NULL;
	break;
      }
    }
  }
  
  /* se cambia el valor si se tom� la decisi�n */
  if (correcto) {
    set_node_att(elm, att_id, correcto, 1);
    DEBUG("err_att_value");
    EPRINTF3("   [ERR] valor atributo %s cambiado de '%s' a '%s'\n",
	    att_list[att_id].name, value, correcto);
/*     free(correcto); */
    return 1;
  }
  else return 0;
}






/**
 * intenta corregir los casos en que no aparece un atributo
 * obligatorio
 *
 * devuelve 0 si no lo consigue
 *
 */
static int err_att_req(tree_node_t *elm, int att_id, xchar **atts)
{
  char *valor= NULL;

  /* CASO 3: si el elemento es 'script' y el atributo 'type'
   * se pone el tipo MIME del lenguaje: en principio, se soporta
   * JavaScript. Si es otro, se toma del atributo 'language' o
   * se deja vac�o
   */
  if ((ELM_ID(elm)==ELMID_SCRIPT) && (!strcmp(att_list[att_id].name,"type"))
      && atts) {
    /* busca atributo 'language' */
    int i;
    for (i=0; atts[i] && strcasecmp(atts[i],"language"); i+=2);
    if (atts[i]) {
      if ((xsearch(atts[i+1],"javascript"))||(xsearch(atts[i+1],"Javascript"))||
	  (xsearch(atts[i+1],"JavaScript")))
	valor= tree_strdup("text/javascript");
      else valor= tree_strdup(atts[i+1]);
    } 
    else valor= tree_strdup("text/javascript"); 
  }

  /* CASO 4: si el elemento es 'style' y el atributo 'type'
   * se pone el tipo MIME del lenguaje: en principio, se
   * coloca text/css, pero se podr�a mejorar este comportamiento
   * tomando el de http-equiv en META
   *
   */
  else if ((ELM_ID(elm)==ELMID_STYLE) && (!strcmp(att_list[att_id].name,"type"))) {
    valor= tree_strdup("text/css"); 
  }




  /* CASO 1: es de tipo CDATA -> se pone valor vac�o "" */
  else if (att_list[att_id].attType== ATTTYPE_CDATA) {
    valor= tree_strdup("");
  }
  
  /* CASO 2: es de tipo ID -> se toma el de 'name' si lo hay */
  else if ((att_list[att_id].attType== ATTTYPE_ID) && atts) {
    int i;
    for (i=0; atts[i] && strcasecmp(atts[i],"name"); i+=2);
    if (atts[i] && dtd_att_is_valid(att_id,atts[i+1]))
      valor= tree_strdup(atts[i+1]);
  }


  /* se establece el atributo si se tom� la decisi�n */
  if (valor) {
    set_node_att(elm, att_id, valor, 1);
    DEBUG("err_att_req");
    EPRINTF2("   [ERR] atributo obligatorio '%s' inclu�do con valor '%s'\n",
	    att_list[att_id].name, valor);
/*     free(valor); */
    return 1;
  }
  else return 0;
}





/**
 * intenta corregir los casos en que haya errores o situaciones
 * especiales en atributos, no detectadas por otros m�todos
 *
 * se ejecuta por defecto siempre
 *
 */
static void err_att_default(tree_node_t *elm, xchar **atts)
{

  /* si posee atributo 'name', se pasa su valor a 'id' */
  switch (ELM_ID(elm)) {
  case ELMID_A:
  case ELMID_APPLET:
  case ELMID_FORM:
  case ELMID_FRAME:
  case ELMID_IFRAME:
  case ELMID_IMG:
  case ELMID_MAP:
    {
      int i;
      int att_ptr;

      if (atts) {
	for (i=0; atts[i]; i+=2)  
	  if (!strcmp(atts[i],"name")) {
	    att_ptr= dtd_att_search_list("id",ELM_PTR(elm).attlist[doctype]);
	    if ((att_ptr != -1) && (!tree_node_search_att(elm,att_ptr))) {
	      if (dtd_att_is_valid(att_ptr,atts[i+1])) {
		set_node_att(elm, att_ptr, atts[i+1], 1);
		DEBUG("");
		EPRINTF1("   [ERR] insertado atributo 'id' con valor '%s'\n",
			 atts[i+1]);
	      }
	      else INFORM("se descarta atributo ID con valor incorrecto");
	      break;
	    }
	  }
	break;
      } /* if atts */
      break;
    }
  case ELMID_PRE:
  case ELMID_SCRIPT:
  case ELMID_STYLE:
    if (!tree_node_search_att(elm, ATTID_XML_SPACE)
	&& dtd_att_search_list_id(ATTID_XML_SPACE, 
				  ELM_PTR(elm).attlist[doctype]) >= 0) {
      set_node_att(elm,ATTID_XML_SPACE, "preserve", 1); 
    }
    break;
  } /* switch */
}





/**
 * intenta corregir situaciones en que no se respeta la estructura
 * b�sica de HTML: <html><head>...</head><body>...</body>
 *
 * devuelve 0 si no lo consigue
 *
 * 
 * [] si no empez� el documento
 *   - si recibe <head>, inserta <html>
 *   - si recibe <body>, inserta <html> <head> </head>
 *   - si recibe elemento de <head>, inserta <html> <head>
 *   - si recibe elemento de <body>, inserta <html> <head> </head> <body>
 *
 */
static int err_html_struct(document_t *document, int elm_id)
{
  int ok= 0;
  tree_node_t *html=NULL, *head=NULL, *body=NULL;
  tree_node_t *nodo;

  html= document->inicio;
  if (html) {
    head= tree_search_elm_child(html,ELMID_HEAD);
    if (doctype!=XHTML_FRAMESET)
      body= tree_search_elm_child(html,ELMID_BODY);
    else 
      body= tree_search_elm_child(html,ELMID_FRAMESET);
  }


  switch (elm_id) {
  case ELMID_HEAD:
    if (html) break;
    /* establece un nodo para el elemento html */
    if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);
    actual_element= html;
    ok= 1;
    DEBUG("err_html_struct()");
    EPRINTF("   [ERR] introducido elemento <html>\n");
    break;

  case ELMID_BODY:
  case ELMID_FRAMESET:
    if (html && head) break;
    /* establece un nodo para el elemento html */
    if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);	
    /* establece un nodo para el elemento head */
    if (!head) {
      actual_element= html;
      head= err_aux_insert_elm(ELMID_HEAD,NULL,0);
      /* establece un nodo para el elemento title */
      nodo= err_aux_insert_elm(ELMID_TITLE,"****",4);
      /* cierra title y head */
      elm_close(nodo);
      elm_close(head);
    }
    for (nodo=actual_element; nodo && (nodo!=html); nodo=nodo->padre)
      elm_close(nodo);
    actual_element= html;
    ok=1;
    DEBUG("err_html_struct()");
    EPRINTF("   [ERR] introducido <html> <head> <title> </title> </head> \n");
    break;
 

  default:
    /* elemento de HEAD */
    if (!head && !body && dtd_can_be_child(elm_id,ELMID_HEAD,doctype)) {
      /* se abre <html><head> */
      /* establece un nodo para el elemento html */
      if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);	
      /* establece un nodo para el elemento head */
      head= err_aux_insert_elm(ELMID_HEAD,NULL,0);
      ok=1;
      DEBUG("err_html_struct()");
      EPRINTF("   [ERR] introducido <html> <head>\n");
   
      /* elemento de BODY */
    } else if (!body && (doctype!=XHTML_FRAMESET) && 
	       dtd_can_be_child(elm_id,ELMID_BODY,doctype)) {
      /* establece un nodo para el elemento html */
      if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);	
      /* establece un nodo para el elemento head */
      if (!head) {
	head= err_aux_insert_elm(ELMID_HEAD,NULL,0);
	/* establece un nodo para el elemento title */
	nodo= err_aux_insert_elm(ELMID_TITLE,"****",4);
	/* cierra title y head */
	elm_close(nodo);
	elm_close(head);
      }
      
      for(nodo=actual_element; nodo && (nodo!=html); nodo=nodo->padre)
	elm_close(nodo);
      actual_element= html;

      /* establece un nodo para el elemento body */
      body= err_aux_insert_elm(ELMID_BODY,NULL,0);
      ok=1;
      DEBUG("err_html_struct()");
      EPRINTF("   [ERR] introducido <html> <head> <title> </title> </head> <body>\n");
	
    } else if (!body && (doctype==XHTML_FRAMESET) && 
	       dtd_can_be_child(elm_id,ELMID_FRAMESET,doctype)) {
      /* establece un nodo para el elemento html */
      if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);	
      /* establece un nodo para el elemento head */
      if (!head) {
	head= err_aux_insert_elm(ELMID_HEAD,NULL,0);
	/* establece un nodo para el elemento title */
	nodo= err_aux_insert_elm(ELMID_TITLE,"****",4);
	/* cierra title y head */
	elm_close(nodo);
	elm_close(head);
      }
      
      for(nodo=actual_element; nodo && (nodo!=html); nodo=nodo->padre)
	elm_close(nodo);
      actual_element= html;

      /* establece un nodo para el elemento body */
      body= err_aux_insert_elm(ELMID_FRAMESET,NULL,0);
      ok=1;
      DEBUG("err_html_struct()");
      EPRINTF("   [ERR] introducido <html> <head> <title> </title> </head> <frameset>\n");
    }
  } /* switch */


  return ok;
}


/*
 * si se intenta insertar un elemento con nombre desconocido
 *
 */
static int err_elm_desconocido(const xchar *nombre)
{
  /* se mira si es alg�n elemento no normativo u obsoleto conocido */
  if (!strcmp(nombre,"listing") || !strcmp(nombre,"plaintext") ||
      !strcmp(nombre,"xmp")) {
    /* se debe insertar <pre> */
    return ELMID_PRE;
  }/* else if (!strcmp(nombre,"layer") || !strcmp(nombre,"ilayer"))
      return ELMID_IFRAME;*/

  return -1;
}




/* funciones auxiliares del subm�dulo */
static tree_node_t* err_aux_insert_elm(int elm_id, const xchar *content, int len)
{
  tree_node_t *elm; 
/*   tree_node_t *data; */

  /* crea e inserta el nodo de elemento */
  elm= new_tree_node(Node_element);
  elm->cont.elemento.elm_id= elm_id;
  insert_element(elm);
  set_attributes(elm,NULL);
  
  /* si tiene contenido, se lo inserta */
  if (content && (len>0)) {
    tree_link_data_node(Node_chardata, elm, content, len);
  }

  /* si es HTML, HEAD o BODY, marca las variables de inserci�n */
  switch (elm_id) {
  case ELMID_HTML:
    ins_html= elm;
    break;
  case ELMID_HEAD:
    ins_head= elm;
    break;
  case ELMID_BODY:
    ins_body= elm;
    break;
  }

  /* gesti�n de atributos */
  set_attributes(elm,NULL);


  return elm;
}


/*
 * Comprueba si el valor de un atributo est� bien formado:
 * - no contiene "<" ni "&" sin pertenecer a ref. a entidad
 * - nota: las comillas vienen ya filtradas por el parser, as�
 *   que no se comprueban aqu�.
 *
 * Devuelve:
 * - si est� bien formado: puntero a la misma cadena
 * - si no: puntero a una nueva cadena corregida
 */
xchar* check_and_fix_att_value(xchar* value)
{
  xchar* fixed = value;
  int i, k;
  int size_inc = 0;
  xchar tmpchar;

  /* special marks: in values, characters with code 1 and 2 are
   * introduced for signaling, respectively, '<' or '&' that
   * need to be fixed
   */

  /* first round: detect errors and count the increment of size
   * necessary to correct them
   */
  for (i = 0; value[i]; i++) {
    if (value[i] == '&' && value[i+1] == '#' && value[i+2] == 'x') {
      /* character reference (hexadecimal) */
      for (k = i + 3; 
	   (value[k] >= '0' && value[k] <= '9')
	     || (value[k] >= 'a' && value[k] <= 'f') 
	     || (value[k] >= 'A' && value[k] <= 'F'); 
	   k++);
      if (value[k] != ';') {
	/* needs to be fixed insert "amp;" */
	size_inc += 4;
	value[i] = 2;
	i = k - 1; /* next iteration at index k */
      } else {
	i = k; /* next iteration at index k + 1 */
      }
      
    } else if (value[i] == '&' && value[i+1] == '#') {
      /* character reference */
      for (k = i + 2; value[k] >= '0' && value[k] <= '9'; k++);
      if (value[k] != ';') {
	/* needs to be fixed insert "amp;" */
	size_inc += 4;
	value[i] = 2;
	i = k - 1; /* next iteration at index k */
      } else {
	i = k; /* next iteration at index k + 1 */
      }
      
    } else if (value[i] == '&') {
      /* entity reference */
      /* LETTER     ([\x41-\x5a]|[\x61-\x7a]|[\xc0-\xd6]|[\xd8-\xf6]|[\xf8-\xff]) */
      for (k = i + 1;
	   (value[k] >= 0x41 && value[k] <= 0x5a)
	   || (value[k] >= 0x61 && value[k] <= 0x7a)
	   || ((unsigned)value[k] >= 0xc0 && (unsigned)value[k] <= 0xd6)
	   || ((unsigned)value[k] >= 0xd8 && (unsigned)value[k] <= 0xf6)
	   || ((unsigned)value[k] >= 0xf8);
	   k++);
      if (value[k] != ';') {
	/* needs to be fixed insert "amp;" */
	size_inc += 4;
	value[i] = 2;
	i = k - 1; /* next iteration at index k */
      } else {
	/* check the entity name */
	tmpchar = value[k+1];
	value[k+1] = 0;
	if (dtd_ent_search(&value[i]) == -1) {
	  /* needs to be fixed insert "amp;" */
	  size_inc += 4;
	  value[i] = 2;
	  i = k - 1; /* next iteration at index k */
	} else {
	  i = k; /* next iteration at index k + 1 */
	}
	value[k+1] = tmpchar;
      }
    } else if (value[i] == '<') {
      size_inc += 3;
      value[i] = 1;
    }
  }

  /* fix marked entries */
  if (size_inc > 0) {
    fixed = (xchar*) tree_malloc(strlen(value) + 1 + size_inc);

    for (i = 0, k = 0; value[i]; i++, k++) {
      /* copy the value and replace marked characters */
      if (value[i] == 1) {
	fixed[k    ] = '&';
	fixed[k + 1] = 'l';
	fixed[k + 2] = 't';
	fixed[k + 3] = ';';
	k += 3;
      } else if (value[i] == 2) {
	fixed[k    ] = '&';
	fixed[k + 1] = 'a';
	fixed[k + 2] = 'm';
	fixed[k + 3] = 'p';
	fixed[k + 4] = ';';
	k += 4;
      } else {
	fixed[k] = value[i];
      }
    }
    fixed[k] = 0;
  }

  return fixed;
}



/*
 * ===================================================================
 * Write the XHTML output
 * ===================================================================
 *
 */

/* internal variables of the output module */
static int xml_space_on;
static int inline_on;
static int indent;
static int chars_in_line;
static int whitespace_needed;
static int inside_cdata_sec;

#define CBUFFER_SIZE 32768
static char cbuffer[CBUFFER_SIZE];
static int cbuffer_pos;
static int cbuffer_avail;

/*
 * Writes to output the document
 *
 */ 
static void write_document(document_t *doc)
{
  tree_node_t *p;
  int indent;

  indent = 0;
  xml_space_on = 0;
  inside_cdata_sec = 0;

  cprintf_init(param_charset_out, param_outputf);

  if (!param_generate_snippet) {
      /* write <?xml... */
      cprintf("%s?xml version=\"1.0\"", lt);
    /*   if (document->encoding[0])  */
    /*     cprintf(" encoding=\"%s\"",document->encoding); */
      cprintf(" encoding=\"%s\"", param_charset_out->preferred_name);
      cprintf("?%s%s%s", gt, eol, eol);

      /* write <!DOCTYPE... */
      cprintf("%s!DOCTYPE html%s   %s%s   \"%s\" %s%s",
          lt, eol, doctype_string[doctype], eol, dtd_string[doctype], gt, eol);
  }
  p = doc->inicio;
  indent = 0;
  write_node(p);
  cprintf(eol);

  cprintf_close();
}

static int write_node(tree_node_t *node)
{
  int len = 0;

  switch (node->tipo) {
  case Node_element:
    len = write_element(node);
    break;
  case Node_chardata:
    if (!xml_space_on)
      len = write_chardata(node);
    else
      len = write_chardata_space_preserve(node);
    break;
  case Node_cdata_sec:
    len = write_cdata_sec(node);
    break;
  case Node_comment:
    len = write_comment(node);
    break;
  }

  return len;
}

static int should_write_element(tree_node_t* nodo)
{
    return !(param_generate_snippet && (ELM_ID(nodo) == ELMID_HEAD));
}

static int should_write_tags(tree_node_t* nodo)
{
    return !(param_generate_snippet && (ELM_ID(nodo) == ELMID_HTML
                                        || ELM_ID(nodo) == ELMID_BODY));
}

static int write_element(tree_node_t *elm)
{
  int len = 0;
  tree_node_t *n;
  int is_block;
  int xml_space_activated;
  int writes_tags;

  if (!should_write_element(elm))
    return 0;
    
  writes_tags = should_write_tags(elm);
  is_block = dtd_elm_is_block(ELM_ID(elm));

  /* activate "xml:space preserve" if necessary */
  if (!xml_space_on 
      && (tree_node_search_att(elm, ATTID_XML_SPACE)
	  || ELM_ID(elm) == ELMID_SCRIPT
	  || ELM_ID(elm) == ELMID_STYLE)) {
    if (ELM_ID(elm) == ELMID_STYLE)
      len += write_indent(indent, 1);
    else if (ELM_ID(elm) != ELMID_SCRIPT && is_block)
      len += cprintf(eol);
    xml_space_activated = 1;
    xml_space_on = 1;
  } else {
    xml_space_activated = 0;
  }

  /* write start tag */
//  if (param_generate_snippet && elm == html
  if (writes_tags) {
    if (is_block) {
       len += write_indent(indent, 1);
       inline_on = 0;
    } else {
      if (!inline_on) {
        inline_on = 1;
        whitespace_needed = 0;
        if (!param_compact_block_elms)
      len += write_indent(indent, 1);
      }
    }
    len += write_start_tag(elm);
  }

  /* process children nodes */
  n = elm->cont.elemento.hijo;
  if (is_block && writes_tags)
    indent += param_tab_len;
  while (n) {
    len += write_node(n);
    n = n->sig;
  }
  if (is_block && writes_tags)
    indent -= param_tab_len;

  /* write end tag if not empty */
  if (writes_tags) {
    if (elm->cont.elemento.hijo) {
    if (is_block) {
      if (inline_on) {
    inline_on = 0;
    if (!param_compact_block_elms)
      len += write_indent(indent, 1);
      } else {
    len += write_indent(indent, 1);
      }
    }
    len += write_end_tag(elm);
    } else if (!param_empty_tags
         && elm_list[ELM_ID(elm)].contenttype[doctype] != CONTTYPE_EMPTY) {
    len += write_end_tag(elm);
    }
  }

  /* deactivate "xml:space preserve" if activated */
  if (xml_space_activated) {
    xml_space_on = 0;
  }

  return len;
}


static int write_chardata(tree_node_t *node)
{
  int i;
  int pos;
  int data_len;
  xchar *data;
  int bytes_to_print;
  int chars_to_print;
  int num;
  int printed;

  data = (char*)tree_index_to_ptr(node->cont.chardata.data);
  data_len = node->cont.chardata.data_len;
  pos = 0;
  num = 0;

  while (pos < data_len) {
    /* filter blanks at the beginning of this data block */
    for (i = pos; 
	 i < data_len
	   && (data[i] == 0x0a || data[i] == 0x0d 
	       || data[i] == ' ' || data[i] == '\t'); 
	 i++);

    if (!inline_on && pos == 0 && i < data_len) {
      inline_on = 1;
      whitespace_needed = 0;
      if (!param_compact_block_elms)
	num += write_indent(indent, 1);
    }

    /* put one whitespace instead, but only if no new line is printed */
    if (i != pos && chars_in_line != indent)
      whitespace_needed = 1;

    /* find the next breakpoint */
    pos = i;
    chars_to_print = 0;
    bytes_to_print = 0;
    for ( ; 
	  i < data_len && data[i] != 0x0a && data[i] != 0x0d 
	    && data[i] != ' ' && data[i] != '\t';
	  i++, bytes_to_print++) {
      if ((char)(data[i] & 0xC0) != (char) 0x80)
	chars_to_print++;
    }
    
    if (bytes_to_print) {
      num += write_whitespace_or_newline_if_needed(chars_to_print);
      printed = write_plain_data(&data[pos], bytes_to_print);
      num += printed;
      pos += bytes_to_print;
    } 
  }

  return num;
}

static int write_cdata_sec(tree_node_t *node)
{
  int len;

  len = 0;

  if (!inline_on && !xml_space_on) {
    inline_on = 1;
    whitespace_needed = 0;
    len += write_indent(indent, 1);
  }

  /* write opening markup only if previous node was not
   * another CDATA section 
   */
  if (!inside_cdata_sec) {
    /* write the opening markup (with // if xml_space_on <- (script|style) */
    if (param_protect_cdata && xml_space_on) {
      len += cprintf("//%s![CDATA[", lt);
      chars_in_line += 11;
    } else {
      write_whitespace_or_newline_if_needed(9);
      len += cprintf("%s![CDATA[", lt);
      chars_in_line += 9;
    }
  }

  /* write the data node itself */
  len += write_chardata_space_preserve(node);

  /* write the closing markup if next node is not a CDATA
   * section 
   */
  if (!node->sig || node->sig->tipo != Node_cdata_sec) {
    /* write the closing markup */
    if (param_protect_cdata && xml_space_on) {
      len += cprintf("//]]%s", gt);
      chars_in_line += 5;
    } else {
      len += cprintf("]]%s", gt);
      chars_in_line += 3;
    }
    inside_cdata_sec = 0;
  } else {
    inside_cdata_sec = 1;
  }

  return len;
}

static int write_chardata_space_preserve(tree_node_t *node)
{
  int data_len;
  xchar *data;
  int num;

  data = (char*)tree_index_to_ptr(node->cont.chardata.data);
  data_len = node->cont.chardata.data_len;
  num = 0;

  num += write_plain_data(data, data_len);

  return num;
}

static int write_comment(tree_node_t *comm)
{
  int num;
  int prev_inline;

  num = write_indent(indent, 1);

  write_whitespace_or_newline_if_needed(4); /* strlen("<!--") */

  if (param_pre_comments) {
    num += cprintf("%s!--", lt);
    chars_in_line += 4;
    num += write_chardata_space_preserve(comm);
  } else {
    num += cprintf("%s!-- ", lt);
    chars_in_line += 5;
    prev_inline = inline_on;
    inline_on = 1;
    indent += param_tab_len;
    num += write_chardata(comm);
    indent -= param_tab_len;
    inline_on = prev_inline;
  }

  if (param_pre_comments) {
    num += cprintf("--%s", gt);
    chars_in_line += 3;
  } else {
    num += cprintf(" --%s", gt);
    chars_in_line += 4;
  }

  return num;
}

static int write_start_tag(tree_node_t* nodo)
{
  att_node_t *att;
  char limit;
  char *value;
  int num;
  char *text;
  int chars_to_print;
  int i;
  char *elm_name;
  int elm_name_len;
  int printed;

  elm_name = elm_list[ELM_ID(nodo)].name;
  elm_name_len = strlen(elm_name);
  num = 0;

  if (nodo->cont.elemento.hijo)
    num += write_whitespace_or_newline_if_needed(1 + elm_name_len);
  else
    num += write_whitespace_or_newline_if_needed(3 + elm_name_len);

  printed = cprintf("%s%s", lt, elm_name);
  num += printed;
  chars_in_line += printed;

  for (att= nodo->cont.elemento.attlist; att; att= att->sig)
    if (att->es_valido) {
      value= (char*)tree_index_to_ptr(att->valor);

      if (xsearch(value,"\"")) limit= '\'';
      else limit= '\"';

      /* does this attribute fit in this line? */
      if (inline_on 
	  && (3 + strlen(att_list[att->att_id].name) 
	      + strlen(value) + chars_in_line) > param_chars_per_line) {
	num += write_indent_internal(indent, 1, 1);
      } else {
	cputc(' ');
	num++;
	chars_in_line++;
      }

      /* write name=(single|double)quote */
      printed = cprintf("%s=%c", att_list[att->att_id].name, limit);
      num += printed;
      chars_in_line += printed;

      chars_to_print = strlen(value);
      text = value;
      while (chars_to_print > 0) {
	if (text[0] == '&') {
	  num += cprintf("%s", amp);
	  chars_in_line++;
	  text++;
	  chars_to_print--;
	} else if (text[0] == '<') {
	  num += cprintf("%s", lt);
	  chars_in_line++;
	  text++;
	  chars_to_print--;
	} else if (text[0] == 0x0a) {
	  num += cprintf(" ");
	  chars_in_line++;
	  text++;
	  chars_to_print--;
	} else if (text[0] == 0x0d) {
	  num += cprintf(" ");
	  chars_in_line++;
	  if (chars_to_print > 1 && text[1] == 0x0a) {
	    text += 2;
	    chars_to_print -= 2;
	  } else {
	    text++;
	    chars_to_print--;
	  }
	}
	
	for (i=0; i < chars_to_print; i++)
	  if (text[i] == '&' || text[i] == '<' 
	      || text[i] == 0x0a || text[i] == 0x0d) {
	    break;
	  }
	
	/* print this fragment of text */
	printed = cwrite(text, i);
	num += printed;
	chars_in_line += printed;
	text += i;
	chars_to_print -= i;
      }
      
      num += cprintf("%c", limit);
      chars_in_line++;
    }

  if ((param_empty_tags && !nodo->cont.elemento.hijo)
      || elm_list[ELM_ID(nodo)].contenttype[doctype] == CONTTYPE_EMPTY) {
    if (!param_compact_empty_elm_tags) {
      num += cprintf(" /");
    } else {
      num += cprintf("/");
    }
    chars_in_line++;
  }
  
  num+=cprintf(gt);
  chars_in_line++;

  return num;
}

static int write_end_tag(tree_node_t* nodo)
{
  int num;
  int printed;
  char *elm_name;
  int elm_name_len;
  
/*   if (!nodo->cont.elemento.hijo) */
/*     return 0; */

  elm_name = elm_list[nodo->cont.elemento.elm_id].name;
  elm_name_len = strlen(elm_name);
  num = 0;

  num += write_whitespace_or_newline_if_needed(3 + elm_name_len);

  printed = cprintf("%s/%s%s",lt,
		    elm_list[nodo->cont.elemento.elm_id].name, gt);
  num += printed;
  chars_in_line += printed;

  return num;
}

/*
 * In inline mode, if a whitespace is needed, writes it or,
 * if the text following it doesn't fit in the line, writes
 * a new line instead. If no whitespace needs to be written,
 * then the current line must be continued, even if it is
 * overflown.
 *
 */
static int write_whitespace_or_newline_if_needed(int next_data_len)
{
  int num = 0;

  if (inline_on && whitespace_needed) {
    /* does the next text fit in this line */
    if ((chars_in_line + next_data_len + 1) > param_chars_per_line) {
      /* write a new line */
      num += write_indent(indent, 1);
    } else {
      /* write a whitespace only */
      cputc(' ');
      num++;
      chars_in_line++;
    }

    whitespace_needed = 0;
  }

  return num;
}

static int write_plain_data(xchar* text, int len)
{
  int num;
  int i;
  int pos;
  int wrote;

  i = 0;
  num = 0;
  while (i < len) {
    if (text[i] == '&') {
      num += cprintf("%s", amp);
      i++;
      chars_in_line++;
    } else if (text[i] == '<') {
      num += cprintf("%s", lt);
      i++;
      chars_in_line++;
    } else if (text[i] == 0x0a) {
      cwrite(eol, eol_len);
      num++;
      i++;
      chars_in_line = 0;
    } else if (text[i] == 0x0d) {
      cwrite(eol, eol_len);
      num++;
      chars_in_line = 0;
      if (i + 1 < len && text[i + 1] == 0x0a) {
	i += 2;
      } else { 
	i++;
      }
    }

    pos = i;
    for ( ; i < len; i++) {
      if (text[i] == '&' || text[i] == '<' 
	  || text[i] == 0x0d || text[i] == 0x0a) {
	break;
      }
    }
      
    /* print the fragment */
    if (i > pos) {
      wrote = cwrite(&text[pos], i - pos);
      num += wrote;
      chars_in_line += wrote;
    }
  }

  return num;
}

static int write_indent(int len, int new_line)
{
  write_indent_internal(len, new_line, 0);
}

static int write_indent_internal(int len, int new_line, int ignore_xml_space)
{
  int i;

  if (xml_space_on && !ignore_xml_space) {
    return 0;
  }

  chars_in_line = indent;

  if (new_line) {
    cprintf(eol);
  }

  for (i = 0; i < len; i++) 
    cputc(' ');

  if (new_line)
    return len + 1;
  else
    return len;
}

static int cprintf_init(charset_t *to_charset, FILE *file)
{
  cbuffer_pos = 0;
  cbuffer_avail = CBUFFER_SIZE;
  charset_init_output(to_charset, file);
}

static int cprintf_close()
{
  /* write the last block */
  if (cbuffer_pos > 0)
    cflush();

  /* close the charset converter */
  charset_close();
}

static int cprintf(char *format, ...)
{
  va_list ap;
  int written;
  int chars_written;

  va_start(ap, format);
  written = vsnprintf(&cbuffer[cbuffer_pos], cbuffer_avail, format, ap); 
  va_end(ap);

  /* buffer overflow? */
  if (written >= cbuffer_avail) {
    if (written >= CBUFFER_SIZE) {
      EXIT("Output buffer overflow");
    }
    /* write the current buffer, excluding the new data */
    cflush();

    /* write again the new data to the buffer, but now at the beginning */
    va_start(ap, format);
    written = vsnprintf(&cbuffer[cbuffer_pos], cbuffer_avail, format, ap);
    va_end(ap);
  }

  if (written < 0) {
    perror("vsnprintf()");
    EXIT("Error when writing output");
  }

  /* count the number of UTF-8 chars (written is in bytes, not chars) */
  chars_written = ccount_utf8_chars(&cbuffer[cbuffer_pos], written);

  cbuffer_pos += written;
  cbuffer_avail -= written;

  return (int) chars_written;
}

static int cwrite(const char *buf, size_t num)
{
  size_t chars_written;

  if (num > CBUFFER_SIZE) {
    EXIT("Output buffer overflow");
  }

  if (num > cbuffer_avail) {
    cflush();
  }

  /* count the number of UTF-8 chars (written is in bytes, not chars) */
  chars_written = ccount_utf8_chars(buf, num);

  memcpy(&cbuffer[cbuffer_pos], buf, num);
  cbuffer_pos += num;
  cbuffer_avail -= num;

  return (int) chars_written;
}

static int cputc(int c)
{
  if (cbuffer_avail == 0)
    cflush();
  cbuffer[cbuffer_pos++] = (char) c;
  cbuffer_avail--;
}

static void cflush()
{
  int wrote = charset_write(cbuffer, cbuffer_pos);
  if (wrote < cbuffer_pos) {
    /* feed again unwrote bytes */
    memmove(cbuffer, &cbuffer[wrote], cbuffer_pos - wrote);
    cbuffer_pos = cbuffer_pos - wrote;
    cbuffer_avail = CBUFFER_SIZE - cbuffer_pos;
  } else {
    /* all the output bytes have been wrote */
    cbuffer_pos = 0;
    cbuffer_avail = CBUFFER_SIZE;
  }
}

static size_t ccount_utf8_chars(const char *buf, size_t num_bytes)
{
  size_t num_chars = 0;
  int i;

  for (i = 0; i < num_bytes; i++) {
    if ((char)(buf[i] & 0xC0) != (char)0x80)
      num_chars++;
  }

  return num_chars;
}
