/***************************************************************************
 *   Copyright (C) 2007 by JesÃºs Arias Fisteus   *
 *   jaf@it.uc3m.es   *
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
 * (Jesús Arias Fisteus)
 * 
 * Funciones que se ejecutan según se van encontrando
 * elementos... en el documento html de entrada.
 * El analizador sintáctico (yacc) notifica a este
 * módulo los componentes que se va encontrando en
 * el documento HTML de entrada, mediante
 * una interfaz semejante a la de SAX.
 *
 * En este módulo se llevan a cabo todas las 
 * comprobaciones necesarias y la conversión,
 * y se va generando un árbol en memoria para
 * almacenar el documento XHTML resultante
 * (ver tree.c)
 *
 * Se implementan también las funciones necesarias
 * para imprimir el documento XHTML a partir
 * del árbol. 
 *
 * Se activa el módulo mediante una llamada a
 * saxStartDocument() y se finaliza mediante saxEndDocument().
 * Una vez finalizado, se puede volcar la salida con
 * writeOutput().
 *
 */




#include <stdio.h>
#include <string.h>


#include "procesador.h"
#include "tree.h"

#include <dtd_util.h>
#include <mensajes.h>
#include <xchar.h>

#ifdef SELLAR
#define SELLO "translated by html2xhtml - http://www.it.uc3m.es/jaf/conversor.html"
#endif


#define MAX_NUM_ERRORES 20
#define ID_LIST_SIZE    8192

/* estructura principal */
document_t *document;  /* árbol del documento */

/* estado del módulo */
static enum {ST_START,ST_PARSING,ST_END} state= ST_START;

static int doctype_mask= 0x7;
static int doctype= -1;
static int doctype_locked= 0;
static int ejecutado_doctype= 0;

static tree_node_t *actual_element= NULL;  /* elemento actual */

static int num_errores= 0;


static int doctype_scan(const xchar *data);
static int doctype_set(int type, int lock);

static void volcar_salida(document_t *doc);
static int elm_output_start_tag(tree_node_t* nodo);
static int elm_output_end_tag(tree_node_t* nodo);
static int data_output(tree_node_t *nodo, int nl, int ident);
static int comment_output(tree_node_t *nodo, int nl, int ident);
static int new_line_output(int ident);

static void set_attributes(tree_node_t *elm, xchar **atts);
static void elm_close(tree_node_t *nodo);
static int insert_element(tree_node_t *nodo);
static void insert_chardata(const xchar *ch, int len);
static void elm_meta_scan(tree_node_t *meta);
static int  elm_check_prohibitions(int elmid);
static int  elm_is_father(int elmid, tree_node_t *nodo);

#ifdef SELLAR
static void sellar_documento(void);
#endif


/* funciones de gestión de error */
static void err_att_default(tree_node_t *elm, xchar **atts);
static int err_html_struct(document_t *document, int elm_id);
static int err_child_no_valid(tree_node_t* elm_ptr);
static int err_att_req(tree_node_t *elm, int att_id, xchar **atts);
static int err_att_value(tree_node_t *elm, int att_id, const xchar *value);
static int err_content_invalid(tree_node_t* nodo, int hijos[], int num_hijos);
static int err_elm_desconocido(const xchar *nombre);

/* auxiliares */
static tree_node_t* err_aux_insert_elm(int elm_id, const xchar *content, int len);

static xchar* check_and_fix_att_value(xchar* value);



extern char *param_charset;
extern char *param_charset_default;
extern int   param_estricto;
extern int   param_doctype;
extern int   is_ascii;
extern FILE *outputf;

int param_chars_per_line= 80;
int param_tab_len= 2;


/* variables de inserción anticipada de elementos */
static tree_node_t *ins_html;
static tree_node_t *ins_head;
static tree_node_t *ins_body;


/* variables para representar los caracteres especiales de HTML */
static char lt_escaped[]="&lt;";
static char amp_escaped[]="&amp;";
static char gt_escaped[]="&gt;";
static char lt_normal[]="<";
static char amp_normal[]="&";
static char gt_normal[]=">";

/* apuntarán a la versión escogida: escaped o normal */
static char* lt;
static char* amp;
static char* gt;

static int escape_chars;

/* variables para gestión de IDs */
static xchar* id_list[ID_LIST_SIZE];
static int id_list_num;
static void set_node_att(tree_node_t *nodo, int att_id, xchar *value, int is_valid);
static int id_search(const xchar *value);



/**
 * inicia la conversión
 *
 */
void saxStartDocument(void)
{
  EPRINTF("SAX.startDocument()\n"); 

  if (state!=ST_START && state!=ST_END) EXIT("estado incorrecto");

  state= ST_PARSING;

  /* reset variables */
  doctype= -1;
  doctype_locked= 0;
  ejecutado_doctype= 0;
  actual_element= NULL;
  num_errores= 0;
  ins_html= NULL;
  ins_head= NULL;
  ins_body= NULL;
  id_list_num = 0;

  /* inicia el módulo 'tree' */
  tree_init();

  document= new_tree_document(doctype, -1);

  if (param_charset) strcpy(document->encoding,param_charset);
  if (param_doctype!=-1) doctype_set(param_doctype,1);
}



/**
 * finaliza la conversión 
 *
 * si hay algún error, finaliza mediante la macro exit
 */
void saxEndDocument(void)
{
  EPRINTF("SAX.endDocument()\n");

  if (!document) EXIT("¡no hay documento!");
  if (!document->inicio) EXIT("documento sin elemento raíz???");

  if (actual_element)
    for ( ; actual_element; actual_element= actual_element->padre)
      elm_close(actual_element);

  if (!document->inicio) EXIT("documento descartado");

  if (!document->encoding[0] && !is_ascii && param_charset_default) {
    strcpy(document->encoding,param_charset_default);
    INFORM("Se establece codificación por defecto");
  }

#ifdef SELLAR
  sellar_documento();
#endif

  if (state== ST_PARSING) state= ST_END;
  if (state!= ST_END) EXIT("estado incorrecto");
}


/**
 * se recibe un tag de inicio de elemento
 *
 * atts será un array de cadenas, que sucesivamente
 * contendrá pares nombre/valor de atributos
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


  if (state!=ST_PARSING) EXIT("estado incorrecto");


  /* se pasa a minúsculas */
  xtolower(elm_name,fullname,ELM_NAME_LEN);

  /* busca el elemento */
  if ((elm_ptr= dtd_elm_search(elm_name))<0) {
    if ((elm_ptr= err_elm_desconocido(elm_name))<0) {
      INFORM("elemento no encontrado\n");
      return;
    }
  }

  if ((!actual_element) && (doctype<0)) doctype_set(XHTML_TRANSITIONAL, 0);


  /* ¿vale para este DTD? */
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
      /* si no existe BODY, se sitúa como frameset */
      if (!elm) res= doctype_set(XHTML_FRAMESET,1);
      if (elm || (res==-1)) {
	INFORM("elemento de tipo frameset no válido en este DTD (descartado)");
	return;
      }
    } else {
      INFORM("elemento no válido en este DTD (descartado)\n");
      return;
    }
  }


  if (!actual_element) {
    if (elm_ptr != ELMID_HTML)
      if (!err_html_struct(document, elm_ptr))
	EXIT("¡No empieza con elemento 'html'! Estructura incorrecta\n");
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

  /* si es <script> o <style>, se abre sección CDATA */
  if ((elm_ptr== ELMID_SCRIPT)||(elm_ptr== ELMID_STYLE)) {

    /* se crea un nodo para los datos */
    nodo= new_tree_node(Node_chardata);
    link_node(nodo, actual_element, LINK_MODE_CHILD);

    /* le pasan los datos al nodo */
    tree_set_node_data(nodo, "<![CDATA[",9);
  }

  /* si es <meta>, se comprueban atributos interesantes */
  if (elm_ptr==ELMID_META) elm_meta_scan(nodo);
}



/**
 * se encuentra un tag de finalización de elemento
 *
 */
void saxEndElement(const xchar *name)
{
  int elm_ptr;
  xchar elm_name[ELM_NAME_LEN];
  tree_node_t *nodo;

  EPRINTF1("SAX.endElement(%s)\n",name);

  if (state!=ST_PARSING) EXIT("estado incorrecto");

  xtolower(elm_name,name,ELM_NAME_LEN);
  if ((elm_ptr= dtd_elm_search(elm_name))<0) {
    if ((elm_ptr= err_elm_desconocido(elm_name))<0) {
      INFORM("elemento no encontrado\n");
      return;
    }
  }

  /* si es <script> o <style>, se cierra sección CDATA */
  if ((elm_ptr== ELMID_SCRIPT)||(elm_ptr== ELMID_STYLE)) {
    if (ELM_ID(actual_element)==elm_ptr) {

    /* se crea un nodo para los datos */
    nodo= new_tree_node(Node_chardata);
    link_node(nodo, actual_element, LINK_MODE_CHILD);
    
    /* le pasan los datos al nodo */
    tree_set_node_data(nodo, "]]>", 3);
    } else INFORM("¿qué pasa con el script? No es el elemento actual");
  } 


  /* busca al entecesor que coincida */
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
    actual_element= nodo->padre;
    if (!actual_element) state= ST_END;
  }

}



/**
 * se encuentra una referencia a carácter o
 * a entidad
 *
 */
void saxReference(const xchar *name)
{
  EPRINTF1("SAX.reference(%s)\n",name);

  if (state!=ST_PARSING) EXIT("estado incorrecto");

  if (!actual_element) return;

  /* si es una referencia a entidad, se comprueba que sea válida */
  if (name[1]!='#') 
    if (dtd_ent_search(name)==-1) {
      INFORM("referencia a entidad desconocida");
      return;
    }
  
  /* en este punto, o es una referencia a caracter o es
   * una referencia a entidad conocida:
   * se introduce en el documento xhtml
   */
  insert_chardata(name,xstrlen(name));
}



/**
 * Notificación de información textual
 * Se recibe la cadena con el texto
 * y el número de caracteres
 *
 */
void saxCharacters(const xchar *ch, int len)
{
  EPRINTF2("SAX.characters(%d)[%s]\n",len,ch);

  if (state!=ST_PARSING) return;

  insert_chardata(ch, len);
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
    INFORM("comentarios obivados");
    return;
  }
  
  if (actual_element) {

    tree_link_data_node(Node_comment, actual_element, value, strlen(value));

/*     /\* se crea un nodo para el comentario *\/ */
/*     nodo= new_tree_node(Node_comment); */

/*     link_node(nodo, actual_element, LINK_MODE_CHILD); */
/*     /\* le pasa los datos al nodo *\/ */
/*     tree_set_node_data(nodo, value, xstrlen(value)); */
  } 
  else
    INFORM("comentario antes de elemento raíz");
}



/**
 * se encuentra marcación <!DOCTYPE
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

  if (state!=ST_PARSING) EXIT("estado incorrecto");
  if (ejecutado_doctype) EXIT("DOCTYPE otra vez...");

  ejecutado_doctype= 1;

#if 0
  fprintf(stderr,"%s\n",data);
#endif

  /* intenta averiguar el tipo de documento */
  if ((doc=doctype_scan(data))!=-1) doctype_set(doc,0);
}



void saxError(xchar *data)
{
  EPRINTF1("SAX.error(%s)\n",data);
  num_errores++;

  INFORM("error en el documento de entrada");
  EPRINTF2("   texto '%s' [%x...] no emparejado\n",data,data[0]);

  if (num_errores>MAX_NUM_ERRORES) 
    EXIT("demasiados caracteres sin emparejar");
}


/**
 * imprime la salida
 *
 * mode: 0 normal
 *       1 pre (utiliza referencias a entidades)
 *
 * devuelve 0 (correcto) o <0 (error)
 */
int writeOutput(int mode) 
{
  if (state != ST_END) return -1;
  if (!document) return -2;
  
  escape_chars= mode;
  if (!mode) {
    gt= gt_normal;
    lt= lt_normal;
    amp= amp_normal;
  } else {
    gt= gt_escaped;
    lt= lt_escaped;
    amp= amp_escaped;
  }

  /* vuelca la salida */
  volcar_salida(document);
  return 0;
}

/*
 * libera la memoria asignada en la conversión
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

  xtolower(buffer,data,512);
  if (xsearch(buffer,"transitional")) return XHTML_TRANSITIONAL;
  else if (xsearch(buffer,"loose.dtd")) return XHTML_TRANSITIONAL;
  else if (xsearch(buffer,"strict")) return XHTML_STRICT;
  else if (xsearch(buffer,"frameset")) return XHTML_FRAMESET;
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

	  /* se comprueba si el valor está bien formado, y se corrige si no */
	  atts[i+1] = check_and_fix_att_value(atts[i+1]);
	  
	  /* atributo válido ¡¡comprobar valor y tipo!! */
	  switch (dtd_att_is_valid(att_ptr, atts[i+1])) {
	  case 1: /* ok */
	    set_node_att(elm, att_ptr, atts[i+1], 1);
	    break;
	  case 2: /* ok, pero en minúsculas */
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
   * ahora hay que comprobar que no falte ningún valor que
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


  /* rutina por defecto de detección y 
   * corrección de otros errores 
   */
  err_att_default(elm,atts);

  


}








/*
 * inserta un nodo de elemento en el árbol
 * del documento
 *
 * comprueba si es un contenido válido y busca
 * dónde insertarlo si no lo es
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
    


    if (insertado) {
      link_node(nodo, actual_element, LINK_MODE_CHILD);
      DEBUG("insert_element()");
      EPRINTF1("   insertado elemento %s correctamente\n",ELM_PTR(nodo).name);
    } else {
      DEBUG("elemento descartado:");
      EPRINTF1("   %s\n",ELM_PTR(nodo).name);
    }
  } /* else */

  /* si es un elemento vacío, se cierra */
  if (insertado) {
    if (elm_list[elm_ptr].contenttype[doctype]!=CONTTYPE_EMPTY)
      actual_element= nodo;
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

    
    if (i==16384) EXIT("desbordada la variable content");
    if (dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],content,num)!=1) {
      /* children no válido: a intentar corregirlo */
      if (!err_content_invalid(nodo,content,num))
	WARNING("child no válido");
    }
    else DEBUG("child válido");
  }
}





static void insert_chardata(const xchar *ch, int len)
{
/*   tree_node_t *nodo; */

  if (!actual_element) {
    num_errores++;
    if (num_errores>MAX_NUM_ERRORES) EXIT("demasiados errores");
    return;
  }

  /* ¿si el elemento no es de tipo mixed? */
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

  tree_link_data_node(Node_chardata, actual_element, ch, len);

/*   /\* se crea un nodo para los datos *\/ */
/*   nodo= new_tree_node(Node_chardata); */

/*   link_node(nodo, actual_element, LINK_MODE_CHILD); */

/*   /\* le pasa los datos al nodo *\/ */
/*   tree_set_node_data(nodo, ch, len); */
}















/*
 * recibe un elemento de tipo META
 *
 * - si contiene http-equiv (Content-Type) se busca
 *    información sobre el sistema de codificación de 
 *    caracteres
 *
 */
static void elm_meta_scan(tree_node_t *meta)
{
  att_node_t *att;

  DEBUG("elm_meta_scan()");

  /* busca http-equiv */
  att= tree_node_search_att(meta,ATTID_HTTP_EQUIV_0);
  if (!att) att= tree_node_search_att(meta,ATTID_HTTP_EQUIV_1);

  if (att) {
    char *attval,*cad;
    att_node_t *content;

    attval= tree_index_to_ptr(att->valor);
    if (attval && !strcasecmp(attval,"Content-Type") 
	&& ((content=tree_node_search_att(meta,ATTID_CONTENT)))){
     
      attval= tree_index_to_ptr(content->valor);
      if (attval && ((cad=strstr(attval,"charset=")))) {
	for ( ; *cad!='='; cad++);
	if (!document->encoding[0] && (strlen(cad)<32)) {
	  int i;
	  cad++;
	  /* pasa a mayúsculas */
	  for (i=0; cad[i]; i++)
	    if ((cad[i]>='a')&&(cad[i]<='z'))
	      document->encoding[i]= cad[i] - 0x20;
	    else document->encoding[i]= cad[i];
	  document->encoding[i]= 0;

	  DEBUG("elm_meta_scan()");
	  EPRINTF1("   encoding=%s\n",document->encoding);
	} else {
	  INFORM("elm_meta_scan()");
	  EPRINTF1("   se descarta encoding=%s\n",cad);
	}
      }
    }
  } /* if http-equiv */
}





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





/*
 * chequea si se cumplen las excepciones recogidas en la norma
 * de XHTML en el apéndice B de la norma de XHTML 1.0
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
 * comprueba si algún elemento que contenga a 'nodo', 
 * o el propio elemento 'nodo', tiene identificador 'elmid'
 *
 * devuelve 1 si es así, o 0 si no
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

    /* se añade el valor a la lista */
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
 * FUNCIONES PARA GESTIÓN DE ERRORES
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
 * Estas funciones serán invocadas cuando, procesando
 * el documento de entrada, el programa se encuentre
 * con problemas que no puede solucionar con los
 * procedimientos estándar
 *
 * Por ejemplo, si un atributo obligatorio no
 * aparece en el documento HTML, la función err_att_req
 * intentará obtener un valor válido para ella.
 * Si lo consigue, inserta en el árbol del documento
 * el atributo con dicho valor. Si no, no hace nada
 * y devuelve 0 para indicarlo a quien la invocó
 * 
 * Cuando, probando, se encuentren casos que el
 * procedimiento estándar no soluciona, debe modificarse
 * en este fichero la función adecuada para gestionar
 * dichos casos
 *
 */





/**
 * si un elemento es de tipo children y su contenido no verifica
 * el modelo, se intenta corregir en esta función
 *
 * devuelve 0 si no lo consigue y 1 si lo hace
 *
 */
static int err_content_invalid(tree_node_t* nodo, int hijos[], int num_hijos)
{
  tree_node_t* actual_bak= actual_element;
  int corregido= 0;

  DEBUG("[ERR] err_content_invalid()");

  switch (ELM_ID(nodo)) {
  case ELMID_HTML:
      {
	int in_body;
	
	in_body= ELMID_PRE;
	if (doctype==XHTML_FRAMESET) in_body= ELMID_FRAME;
	
	/* si está mal HTML, intenta arreglarlo */
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
      /* HEAD: ¿falta TITLE? */
      int *h= tree_malloc((num_hijos+1)*sizeof(int));
      int i;
      h[0]= ELMID_TITLE;
      for (i=0;i<num_hijos;i++) h[i+1]= hijos[i];
      if (dtd_is_child_valid(ELM_PTR(nodo).contentspec[doctype],
			     h,num_hijos+1)==1) {
	tree_node_t *title;
	title= new_tree_node(Node_element);
	title->cont.elemento.elm_id= ELMID_TITLE;
	link_node(title,nodo,LINK_MODE_FIRST_CHILD);
	corregido= 1;
      }
/*       free(h); */
      break;
    }
    
  case ELMID_OL:
  case ELMID_UL:
    {
      /* se descarta, porque no contiene información */
      tree_unlink_node(nodo);
      INFORM("[ERR] descartado el elemento");
      corregido=1;
      break;
    }

  case ELMID_TR:
    {
      /* se inserta TD a pesar de que no contiene
       * información, porque puede propagar el problema
       * de contenido inválido hacia sus padres
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


  if (!corregido && param_estricto) {
    /* se descarta el elemento */
    if (nodo!=document->inicio) tree_unlink_node(nodo);
    else document->inicio= NULL;
    INFORM("se descarta el elemento por no tener contenido válido");
    corregido= 1;
  }


  return corregido;
}






/** 
 * el elemento no es un hijo válido para el nodo actual
 * se intenta solucionar esto
 *
 */ 
static int err_child_no_valid(tree_node_t* nodo)
{
  int insertado;
  tree_node_t* actual;

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

  /* si el elemento está dentro de TABLE, THEAD, TBODY o TR se descarta */
  if ((ELM_ID(actual_element)==ELMID_TABLE)||
      (ELM_ID(actual_element)==ELMID_THEAD)||
      (ELM_ID(actual_element)==ELMID_TBODY)||
      (ELM_ID(actual_element)==ELMID_TR)) {
    INFORM("[ERR] el elemento se descarta por estar en una tabla");
    return 0;
  }



  /* en principio, se asciende en la jerarquía hasta
   * que sea un hijo válido
   */
  for (insertado=0, actual= actual_element; actual; actual= actual->padre) {
    if (dtd_can_be_child(ELM_ID(nodo),ELM_ID(actual),doctype)) {
      insertado= 1;
      break;
    }
  } /* for */
  
  /* si se le encontró hueco, se cierran los elementos hasta él */
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
    }
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
  
  /* se cambia el valor si se tomó la decisión */
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
   * se deja vacío
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
    else valor= tree_strdup(""); 
  }

  /* CASO 4: si el elemento es 'style' y el atributo 'type'
   * se pone el tipo MIME del lenguaje: en principio, se
   * coloca text/css, pero se podría mejorar este comportamiento
   * tomando el de http-equiv en META
   *
   */
  else if ((ELM_ID(elm)==ELMID_STYLE) && (!strcmp(att_list[att_id].name,"type"))) {
    valor= tree_strdup("text/css"); 
  }




  /* CASO 1: es de tipo CDATA -> se pone valor vacío "" */
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


  /* se establece el atributo si se tomó la decisión */
  if (valor) {
    set_node_att(elm, att_id, valor, 1);
    DEBUG("err_att_req");
    EPRINTF2("   [ERR] atributo obligatorio '%s' incluído con valor '%s'\n",
	    att_list[att_id].name, valor);
/*     free(valor); */
    return 1;
  }
  else return 0;
}





/**
 * intenta corregir los casos en que haya errores o situaciones
 * especiales en atributos, no detectadas por otros métodos
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
    }
  } /* switch */



  
  /* si el nodo es PRE, STYLE o SCRIPT, inserta xml:space='preserve' */
  if (((ELM_ID(elm)==ELMID_PRE)||(ELM_ID(elm)==ELMID_SCRIPT)||
      (ELM_ID(elm)==ELMID_STYLE))
      && !tree_node_search_att(elm, ATTID_XML_SPACE)) {

    set_node_att(elm,ATTID_XML_SPACE,"preserve",1);
  }

}





/**
 * intenta corregir situaciones en que no se respeta la estructura
 * básica de HTML: <html><head>...</head><body>...</body>
 *
 * devuelve 0 si no lo consigue
 *
 * 
 * [] si no empezó el documento
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

  /*
  if (document->inicio) {
    html= document->inicio;
    if (html->cont.elemento.hijo) {
      if (ELM_ID(html->cont.elemento.hijo)==ELMID_HEAD) {
	head=html->cont.elemento.hijo;
	if (head->sig && 
	     (((doctype==XHTML_FRAMESET)&&(ELM_ID(head->sig)==ELMID_BODY))
	      || ((doctype==XHTML_FRAMESET)&&(ELM_ID(head->sig)==ELMID_FRAMESET))))
	  body= head->sig;
      } 
    }
  }
  */

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
    if (!head && dtd_can_be_child(elm_id,ELMID_HEAD,doctype)) {
      /* se abre <html><head> */
      /* establece un nodo para el elemento html */
      if (!html) html= err_aux_insert_elm(ELMID_HTML,NULL,0);	
      /* establece un nodo para el elemento head */
      if (!head) head= err_aux_insert_elm(ELMID_HEAD,NULL,0);
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
  /* se mira si es algún elemento no normativo u obsoleto conocido */
  if (!strcmp(nombre,"listing") || !strcmp(nombre,"plaintext") ||
      !strcmp(nombre,"xmp")) {
    /* se debe insertar <pre> */
    return ELMID_PRE;
  }/* else if (!strcmp(nombre,"layer") || !strcmp(nombre,"ilayer"))
      return ELMID_IFRAME;*/

  return -1;
}




/* funciones auxiliares del submódulo */
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

/*     data= new_tree_node(Node_chardata); */
/*     link_node(data, elm, LINK_MODE_CHILD); */
/*     tree_set_node_data(data,content,len); */
  }

  /* si es HTML, HEAD o BODY, marca las variables de inserción */
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

  /* gestión de atributos */
  set_attributes(elm,NULL);


  return elm;
}


/*
 * Comprueba si el valor de un atributo está bien formado:
 * - no contiene "<" ni "&" sin pertenecer a ref. a entidad
 * - nota: las comillas vienen ya filtradas por el parser, así
 *   que no se comprueban aquí.
 *
 * Devuelve:
 * - si está bien formado: puntero a la misma cadena
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
  }

  return fixed;
}




















/*
 * ===================================================================
 * FUNCIONES PARA VOLCAR EL ÁRBOL A UN DOCUMENTO XHTML
 * ===================================================================
 *
 */

/* para preservar espacios en blanco en la salida */
static int xml_space_on;



/*
 * vuelca la salida XHTML a partir de los datos
 * recogidos en el árbol
 *
 */ 
static void volcar_salida(document_t *doc)
{
  tree_node_t *p,*ant;
  int allow_child;
  int dir;
  int chars_of_line;   /* número de caracteres escritos en la última línea */
  int indent; /* número de caracteres a identar */

  chars_of_line= 0;
  indent= 0;
  xml_space_on= 0;

  /* vuelca <?xml... */
  fprintf(outputf,"%s?xml version=\"1.0\"", lt);
  if (document->encoding[0]) 
    fprintf(outputf," encoding=\"%s\"",document->encoding);
  fprintf(outputf,"?%s\n\n",gt);


  /* vuelca <!DOCTYPE... */
  fprintf(outputf,"%s!DOCTYPE html\n   %s\n   \"%s\" %s\n\n", 
	  lt, doctype_string[doctype], dtd_string[doctype], gt);

  
  for (p=ant=doc->inicio, allow_child=1, dir=NODE_CHILD; 
       p;
       ant=p, dir= tree_walk(&p,allow_child)) {

    /* cierra el tag anterior si p no es hijo suyo */
    if ((dir!=NODE_CHILD) && allow_child && (ant->tipo==Node_element)) {
      if (tree_node_search_att(ant,ATTID_XML_SPACE))
	xml_space_on--;
      if (dtd_elm_is_block(ELM_ID(ant)) && !xml_space_on)
	indent-= param_tab_len;
      if (ELM_PTR(ant).contenttype[doctype] != CONTTYPE_EMPTY) {
	if (dtd_elm_is_block(ELM_ID(ant)))
	  chars_of_line= new_line_output(indent);
	chars_of_line+= elm_output_end_tag(ant);
      }
    }
    
    if (p->tipo==Node_element) {
      switch (dir) {
      case NODE_BROTHER:
      case NODE_CHILD:
        if (dtd_elm_is_block(ELM_ID(p)) && !xml_space_on) {
	  chars_of_line= new_line_output(indent);
	  indent+= param_tab_len;
	}
	chars_of_line+= elm_output_start_tag(p);
	allow_child= 1;
	break;
	
      case NODE_FATHER:
	if (tree_node_search_att(p,ATTID_XML_SPACE)) 
	  xml_space_on--;
        if (dtd_elm_is_block(ELM_ID(p)) && !xml_space_on)
	  indent-= param_tab_len;
	if ((ant->tipo==Node_element) && 
	    dtd_elm_is_block(ELM_ID(ant))) 
	  chars_of_line=new_line_output(indent);

	chars_of_line+= elm_output_end_tag(p);
	allow_child= 0;
	break;
      }
    }
    else if (p->tipo==Node_chardata) {
      chars_of_line= data_output(p, chars_of_line, indent);
      allow_child= 0;
    }
    
    else if (p->tipo==Node_comment) {
      chars_of_line= comment_output(p, chars_of_line, indent);
      allow_child= 0;
    }

  } /* for */

  /* vuelca una última línea */
  new_line_output(0);
}




static int elm_output_start_tag(tree_node_t* nodo)
{
  att_node_t *att;
  char limite;
  char *valor;
  int num;
  char *text;
  int chars_to_print;
  int i;

  if (tree_node_search_att(nodo,ATTID_XML_SPACE)) xml_space_on++;

  num= fprintf(outputf,"%s%s",lt,elm_list[nodo->cont.elemento.elm_id].name);

  for (att= nodo->cont.elemento.attlist; att; att= att->sig)
    if (att->es_valido) {
      valor= (char*)tree_index_to_ptr(att->valor);

      if (xsearch(valor,"\"")) limite= '\'';
      else limite= '\"';
      
      /* escribe nombre=comilla */
      num+= fprintf(outputf," %s=%c",att_list[att->att_id].name, limite);
      
      /* el valor se escribe por trozos, para filtrar & y < si fuese
       * necesario en modo CGI
       */
      
      chars_to_print = strlen(valor);
      text = valor;
      while (chars_to_print > 0) {
	if (text[0]=='&') {
	  num += fprintf(outputf,"%s",amp);
	  text++;
	  chars_to_print--;
	} else if (text[0]=='<') {
	  num += fprintf(outputf,"%s",lt);
	  text++;
	  chars_to_print--;
	} 
	
	for (i=0; i < chars_to_print; i++)
	  if (text[i]=='&' || text[i]=='<') {
	    break;
	  }
	
	/* imprime el fragmento */
	num += fwrite(text,i, 1, outputf);
	text += i;
	chars_to_print -= i;
      }
      
      num+= fprintf(outputf,"%c", limite);
    }

  if (elm_list[nodo->cont.elemento.elm_id].contenttype[doctype]
      ==CONTTYPE_EMPTY) num+=fprintf(outputf,"/");
  
  num+=fprintf(outputf,gt);

  EPRINTF2(">>S...<%s> num %d\n", ELM_PTR(nodo).name, num);

  return num;
}



static int elm_output_end_tag(tree_node_t* nodo)
{
  int num= 0;

  if (elm_list[nodo->cont.elemento.elm_id].contenttype[doctype]
      !=CONTTYPE_EMPTY) {

    num+= fprintf(outputf,"%s/%s%s",lt,
		  elm_list[nodo->cont.elemento.elm_id].name, gt);
  }

  EPRINTF2(">>...<%s/> num %d\n", ELM_PTR(nodo).name, num);

  return num;
}


/*
 * El parámetro nl indica cuántos caracteres han sido
 * escritos en la línea actual.
 *
 * Devuelve: el número de caracteres escritos en la última línea
 *
 */
static int data_output(tree_node_t *nodo, int nl, int indent)
{
  int i;
  char *text;
  int num_chars= nodo->cont.chardata.data_len; 
  int chars_to_print;

  DEBUG("data_output");
  EPRINTF2(">> >> nl %d; indent %d\n", nl, indent); 


  text= (char*)tree_index_to_ptr(nodo->cont.chardata.data);

  /* busca el primer blanco */

  /* vuelca el texto */
  while (num_chars > 0) {
    int found;

    DEBUG("Nueva iteración");
    EPRINTF2("TEXT(%d)[[%s]]\n", num_chars, text); 

    found= 0;

    /* busca el próximo fin de línea */
    for (i= 0;i < (param_chars_per_line - nl) && (i<num_chars);i++)
      if ((text[i]== 0x0a) || (text[i]== 0x0d)) {
	found= 1;
	break;
      } 

    EPRINTF3(">> num_chars %d; found %d; i %d; ", num_chars, found, i); 
    EPRINTF2("nl %d; xml_space_on: %d\n", nl, xml_space_on);

    if (!xml_space_on && !found && (i<num_chars)) {
      /* busca el próximo punto donde cortar la línea */
      int i0= param_chars_per_line - nl; 
      if (i0 < 0) i0= 0;

      for (i= i0 ; 
	   (i<num_chars) && 
	     (text[i]!= 0x0a) && (text[i]!= 0x0d) && (text[i]!= 0x20);
	   i++);
      if (i<num_chars) {
	if (text[i]== 0x20) text[i]= '\n';
	chars_to_print= i + 1;
	EPRINTF1("corte %d; ", i);
      } else 
	chars_to_print= num_chars;
    } else if (found) {
      text[i]='\n';
      chars_to_print= i + 1;
    } else {
      chars_to_print= num_chars;
    }
    EPRINTF1("chars_to_print %d\n", chars_to_print);

    while (chars_to_print > 0) {

      if (text[0]=='&') {
	fprintf(outputf,"%s",amp);
	text++;
	chars_to_print--;
	num_chars--;
      } else if (text[0]=='<') {
	fprintf(outputf,"%s",lt);
	text++;
	chars_to_print--;
	num_chars--;
      } else if (text[0]=='>') {
	fprintf(outputf,"%s",gt);
	text++;
	chars_to_print--;
	num_chars--;
      } 

      for (i=0; i < chars_to_print; i++)
	if (text[i]=='&' || text[i]=='<' || text[i]=='>') {
	  break;
	}
      
      /* imprime el fragmento */
      fwrite(text,i, 1, outputf);
      text += i;
      chars_to_print -= i;
      num_chars -= i;
    }
    
    /* si quedan caracteres por imprimir, identa la nueva línea */
    if (num_chars > 0 && !xml_space_on) {
      for (i=0; i<indent; i++) fputc(' ',outputf);
      nl= indent;
      
      /* filtra los blancos sobrantes tras el corte de línea*/
      for ( ; (num_chars>0) && (text[0]==0x0a || text[0]==0x0d || text[0]==0x20); 
	    text++, num_chars--);
    }
  }

  EPRINTF1(">>...[text] num %d\n", nl+chars_to_print);


  return nl + chars_to_print;
}




static int comment_output(tree_node_t *nodo, int nl, int indent)
{
  int num;

  fprintf(outputf,"%s!--",lt);

  /* imprime el comentario como si fuese texto */
  num= data_output(nodo, nl + 4, indent);

  fprintf(outputf,"--%s",gt);
  num+= 3;

  return num;
}


static int new_line_output(int indent) 
{
  int i;

  fputc('\n',outputf);
  for (i=0; i<indent; i++)
    fputc(' ',outputf);
  
  return indent;
}
