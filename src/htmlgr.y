%start html

%{
#include <stdio.h>
#include <mensajes.h>
#include <string.h>
#include <xchar.h>

#include <procesador.h>

extern char *tree_strdup(const char *str);
extern void *tree_malloc();
  
/* lista de atributos del elemento actual */
#define MAX_ELEMENT_ATTRIBUTES  255
static void setAttributeData(char *data);
/* static void freeAttributeData(void); */

static char *element_attributes[MAX_ELEMENT_ATTRIBUTES];
static int num_element_attributes= 0;

/* para establecer modo script en el lexer */
void begin_script(char *nombre);

/* control de <PRE> */
extern int pre_state;


#ifdef DEBUG_MEM
#define free(x)      fprintf(stderr,"==%p  [%s]\n",x,x); free(x)  
#endif

%}

%union {
  int  ent;
  char *cad;
}


%token <cad> TOK_DOCTYPE TOK_COMMENT TOK_BAD_COMMENT TOK_STAG_INI TOK_ETAG TOK_CDATA
%token <cad> TOK_ATT_NAME TOK_ATT_NAMECHAR TOK_ATT_VALUE TOK_EREF TOK_CREF
%token <ent> TOK_STAG_END TOK_ATT_EQ

 
%%

html: 
| html doctype 
| html comment
| html bad_comment 
| html stag
| html etag
| html cdata
| html ref
;

doctype: TOK_DOCTYPE {
  //fprintf(stderr,"TOK_DOCTYPE: %s\n",$1);
  saxDoctype($1);
}
;

comment: TOK_COMMENT {
  //fprintf(stderr,"TOK_COMMENT: %s\n",$1);
  saxComment($1);
}
;

bad_comment: TOK_BAD_COMMENT {
  //fprintf(stderr,"TOK_BAD_COMMENT: %s\n",$1);
  INFORM("bad comment");
}
;

stag: TOK_STAG_INI attributes TOK_STAG_END {   
  //fprintf(stderr,"STAG-: %s\n",$1);
  setAttributeData(NULL);
  saxStartElement($1,(xchar**)element_attributes);
/*   freeAttributeData(); */
  num_element_attributes = 0;

  /* modo script del lexer (para SCRIPT y STYLE) */
  if ((!strcasecmp($1,"script")) ||(!strcasecmp($1,"style"))) 
    begin_script($1);

  if (!strcasecmp($1,"pre")) {DEBUG("inicio de modo PRE");pre_state++;}

/*   free($1); */
}
;

etag: TOK_ETAG {
  //fprintf(stderr,"ETAG-: %s\n",$1);
  saxEndElement($1);
  if (!strcasecmp($1,"pre")) {DEBUG("fin de modo PRE");pre_state--;}

/*   free($1); */
}
;

cdata: TOK_CDATA {
  //fprintf(stderr,"CDATA: <%s>\n",$1);
  saxCharacters($1,strlen($1));
}
;

ref: TOK_EREF {
  //fprintf(stderr,"EREF: %s\n",$1);
  saxReference($1);
/*   free($1); */
}
| TOK_CREF {
  //fprintf(stderr,"CREF: %s\n",$1);
  saxReference($1);
}
;

attributes: 
| attributes attribute 
;

attribute: TOK_ATT_NAME TOK_ATT_EQ TOK_ATT_VALUE {
                     setAttributeData($1);
                     setAttributeData($3);
                     }
| TOK_ATT_NAME {
                     setAttributeData($1);
                     setAttributeData($1);
}
| TOK_ATT_NAME TOK_ATT_EQ {
                     char *cad= (char*) tree_malloc(1);
                     cad[0]= 0;
                     setAttributeData($1);
                     setAttributeData(cad);                     
}
;

%%




/*
 * setAttributeData
 *
 * inserta 'data' en la lista de atributos de elementos
 * (sólo se mete el puntero, los datos deben seguir en memoria
 * hasta que no sean necesarios)
 *
 */
static void setAttributeData(char *data)
{
  char *ptr;
  
  /* si data==NULL, se cierra la lista de atributos */ 
  if (!data) {
    element_attributes[num_element_attributes]= NULL;
    return;
  }

  if (num_element_attributes == MAX_ELEMENT_ATTRIBUTES-1) {
    EXIT("máximo de atributos de elemento alcanzado\n");
  }

  element_attributes[num_element_attributes++]= data;  
}



#if 0
/*
 * libera la memoria dinámica asignada al array
 * de datos de atributos
 *
 * PROBLEMA: en uno de los casos hay dos seguidos que referencian a
 *     la misma posición: hay que liberar sólo uno de ellos para que
 *     no casque el programa. Para solucionarlo, se tiene en cuenta
 *     siempre el anterior puntero, y se libera el actual sólo si no 
 *     coincide con el anterior.
 *
 */
static void freeAttributeData(void)
{
  int i;

  /* se libera el primero */
  if ((num_element_attributes > 0) && (element_attributes[0])) {
    free(element_attributes[0]);
  }

  /* se liberan el resto */
  for (i=1 ; i<num_element_attributes; i++)
    if (element_attributes[i] && (element_attributes[i]!=element_attributes[i-1])) {
      free(element_attributes[i]);
    }

  num_element_attributes= 0;
}
#endif
