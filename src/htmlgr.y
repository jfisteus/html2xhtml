%start input

%{
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "xchar.h"
#include "mensajes.h"
#include "procesador.h"
#include "tree.h"

/* define to 1 and set variable yydebug to 1 to get debug messages */
#define YYDEBUG 0

/* list of attributes of the current element */
#define MAX_ELEMENT_ATTRIBUTES  255
static void setAttributeData(char *data);
/* static void freeAttributeData(void); */

static char *element_attributes[MAX_ELEMENT_ATTRIBUTES];
static int num_element_attributes= 0;

/* set the lexer in the script mode */
void lexer_begin_script(char *nombre);

/* control <pre> elements */
extern int pre_state;

extern int yylex (void);
 extern int yyerror(char *e);

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
%token <cad> TOK_CDATA_SEC TOK_XMLPI_INI
%token <ent> TOK_STAG_END TOK_EMPTYTAG_END TOK_ATT_EQ TOK_XMLPI_END
%token <ent> TOK_WHITESPACE

%%

input: html
;

html:
| html doctype
| html comment
| html bad_comment
| html stag
| html etag
| html cdata
| html cdata_sec
| html whitespace
| html ref
| html xmldecl
| html error
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

  /* set the lexer in script mode (for SCRIPT and STYLE) */
  if ((!strcasecmp($1,"script")) ||(!strcasecmp($1,"style")))
    lexer_begin_script($1);

  if (!strcasecmp($1,"pre")) {DEBUG("enter PRE mode");pre_state++;}

/*   free($1); */
}
;

etag: TOK_ETAG {
  //fprintf(stderr,"ETAG-: %s\n",$1);
  saxEndElement($1);
  if (!strcasecmp($1,"pre")) {DEBUG("leaving PRE mode");pre_state--;}

/*   free($1); */
}
;

stag: TOK_STAG_INI attributes TOK_EMPTYTAG_END {
  //fprintf(stderr,"EMTYTAG-: %s\n",$1);
  setAttributeData(NULL);
  saxStartElement($1,(xchar**)element_attributes);
/*   freeAttributeData(); */
  num_element_attributes = 0;
/*   free($1); */
  saxEndElement($1);
}
;

cdata: TOK_CDATA {
  //fprintf(stderr,"CDATA: <%s>\n",$1);
  saxCharacters($1,strlen($1));
}
;

cdata_sec: TOK_CDATA_SEC {
  //fprintf(stderr,"CDATA_SEC: <%s>\n",$1);
  saxCDataSection($1,strlen($1));
}
;

whitespace: TOK_WHITESPACE {
  saxWhiteSpace();
}

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

xmlpi_end: TOK_XMLPI_END
| TOK_STAG_END
| TOK_EMPTYTAG_END
;

xmldecl: TOK_XMLPI_INI attributes xmlpi_end {
  /*fprintf(stderr,"XMLDECL-: %s\n",$1);*/
  setAttributeData(NULL);
  saxXmlProcessingInstruction($1,(xchar**)element_attributes);
  num_element_attributes = 0;
}
;


attributes:
| attributes attribute
| attributes error
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
 * inserts 'data' into the attribute list of the current element.
 * (only the pointer is inserted; therefore, the data must be kept
 * in memory by the caller until it is no longer needed)
 *
 */
static void setAttributeData(char *data)
{
  /* if data == NULL, close the attribute list */
  if (!data) {
    element_attributes[num_element_attributes]= NULL;
    return;
  }

  if (num_element_attributes == MAX_ELEMENT_ATTRIBUTES-1) {
    EXIT("maximum number of attributes for one element reached\n");
  }

  element_attributes[num_element_attributes++]= data;
}
