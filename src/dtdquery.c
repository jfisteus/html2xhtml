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
 * dtdquery.c
 *
 * Queries the internal dtdlib of the converter.
 *
 * Use:
 *    dtdquery -v
 *    dtdquery -e <element_name>
 *    dtdquery -a <attribute_name>
 *    dtdquery --list-dtds
 *    dtdquery --compare <dtd_key> <dtd_key>
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtd_util.h"
#include "dtd.h"

void lee_elm(char *elm_name);
void lee_att(char *att_name);
void check_content(char **argv, int argc);
void listar_dtds();
void comparar_dtds(char *dtd_key_1, char *dtd_key_2);
void error_de_parametros();
void print_version();

int comparar_elemento(int elmid, int dtd1, int dtd2);
int atributos_solo_en_uno(int *lista1, int *lista2, int* resultado);

char *dtdToString(int dtd);
char *envToString(int environment);
char *attTypeToString(int attType);
char *defDeclToString(defaultDecl_t def);

char *readAttBuffer(int buff_ptr);
char *readElmBuffer(int buff_ptr);

char *contentspecToString(unsigned char *buff, char *str, contentType_t conttype, int *len_buff);

int dtd_id(char *key);



#define ERROR(msg)        {fprintf(stderr,"ERROR (%s,%d): %s\n", \
				   __FILE__,__LINE__,msg);exit(1);} 


/* para que el módulo de mensajes no eche de menos la variable */
int parser_num_linea= -1;



void exit_on_error(char *msg)
{
  fprintf(stderr,"!!%s(%d): %s\n",__FILE__,__LINE__,msg);
  exit(1);
}



int main (int argc, char **argv)
{
  if (argc < 2) {
    error_de_parametros();
  } else {
    if (!strcmp(argv[1],"-e")) lee_elm(argv[2]);
    else if (!strcmp(argv[1],"-a")) lee_att(argv[2]);
    else if (!strcmp(argv[1],"-v")) print_version();
    else if (!strcmp(argv[1],"-c")) check_content(argv, argc);
    else if (!strcmp(argv[1],"--list-dtds")) listar_dtds();
    else if (!strcmp(argv[1],"--compare") && argc == 4) 
      comparar_dtds(argv[2], argv[3]);
    else error_de_parametros();
  }

  exit(0);
}

void print_version()
{
  fprintf(stderr, "dtdquery (part of html2xhtml version %s)\n", VERSION);
  fprintf(stderr, "DTD data based on DTDs as available on %s\n",
	  DTD_SNAPSHOT_DATE);
}

void error_de_parametros()
{
  fprintf(stderr, "dtdquery -v\n");
  fprintf(stderr, "dtdquery -e <element_name>\n");
  fprintf(stderr, "dtdquery -a <attribute_name>\n");
  fprintf(stderr, "dtdquery --list-dtds\n");
  fprintf(stderr, "dtdquery --compare <dtd_key> <dtd_key>\n");
  exit(1);
}


void lee_elm(char *elm_name)
{
  int elm_ptr;
  int dtd;
  int i;
  char content[1024];
  int tmp;


  if ((elm_ptr= dtd_elm_search(elm_name))<0) {
    fprintf(stderr,"Element '%s' not found\n",elm_name);
    return;
  }

  printf("%3d %s\n",elm_ptr, elm_list[elm_ptr].name);
  
  for (dtd=0; dtd<DTD_NUM; dtd++) {
    if (elm_list[elm_ptr].environment & DTD_MASK(dtd)) {
      printf("\n** %s **\n",dtd_name[dtd]);
      printf("   - attlist:\n");
      for (i=0; elm_list[elm_ptr].attlist[dtd][i]>=0; i++)
	printf("      %3d  %s\n",elm_list[elm_ptr].attlist[dtd][i],
	       att_list[elm_list[elm_ptr].attlist[dtd][i]].name);

      printf("   - content:\n");
      printf("        %s\n",
	     contentspecToString(dtd_elm_read_buffer
				 (elm_list[elm_ptr].contentspec[dtd]),
				 content,elm_list[elm_ptr].contenttype[dtd],&tmp));
    }
  }

}




void lee_att(char *att_name)
{
  int att_ptr;
  int i,k,num;

  num= 0;
  att_ptr= -1;
  do {
    att_ptr= dtd_att_search(att_name, att_ptr);
    if (att_ptr>=0) {
      num++;
      printf("%3d %s\n",att_ptr,att_list[att_ptr].name);
      printf("      %s\n",envToString(att_list[att_ptr].environment));
      printf("      %s %s %s\n",
	     attTypeToString(att_list[att_ptr].attType),
	     defDeclToString(att_list[att_ptr].defaultDecl),
	     dtd_att_read_buffer(att_list[att_ptr].defaults));

      /* busca los elementos que contengan este atributo */
      for (i=0; i<elm_data_num; i++)
	for (k=0; k<DTD_NUM; k++) {
	  if (dtd_att_search_list_id(att_ptr,elm_list[i].attlist[k])>=0)
	    printf("         %3d %-14s (%s)\n",
		   i,elm_list[i].name,dtd_name[k]);
	}
    }
  }
  while (att_ptr>=0);


  if (!num) fprintf(stderr,"Attribute '%s' not found\n",att_name);
}





void check_content(char **argv, int argc) {
  int doctype;
  int elm_ptr;
  int *contenido;
  int num_elm;
  int valid;
  int i;
  char content[1024];
  int tmp;

  num_elm= argc - 4;

  if (argc < 4) {
    fprintf(stderr,"%s -c dtd element [content*]\n", argv[0]);
    exit(1);
  }

  doctype= dtd_get_dtd_index(argv[2]);
  if (doctype==-1) {
    fprintf(stderr,"%s -c dtd element [content*]\n", argv[0]);
    fprintf(stderr,"      dtd= (transitional|strict|frameset|basic|1.1|mp)\n");
    exit(1);    
  }
  
  if ((elm_ptr= dtd_elm_search(argv[3]))<0) {
    fprintf(stderr,"Element '%s' not found\n",argv[3]);
    exit(1);
  }

  if (elm_list[elm_ptr].contenttype[doctype] != CONTTYPE_CHILDREN) {
    fprintf(stderr,"Element '%s' has not 'children' content\n",argv[3]);
    exit(1);
  }
  
  contenido= (int*) malloc(sizeof(int) * num_elm);

  for (i= 0; i < num_elm; i++) {
    if ((contenido[i]= dtd_elm_search(argv[i+4])) < 0 ) {
      fprintf(stderr,"Element '%s' not found\n",argv[i+4]);
      free(contenido);
      exit(1);
    }
  }

  valid= dtd_is_child_valid(elm_list[elm_ptr].contentspec[doctype], 
			    contenido, num_elm);

  printf("%s\n",
	 contentspecToString(dtd_elm_read_buffer
			     (elm_list[elm_ptr].contentspec[doctype]),
			     content,elm_list[elm_ptr].contenttype[doctype],
			     &tmp)
	 );
  printf("valid: %d\n", valid);

  return;
}

/*
 * Compara dos DTDs elemento a elemento
 *
 */
void comparar_dtds(char *dtd_key_1, char *dtd_key_2)
{
  int dtd1, dtd2;
  int i;
  int num_listados;
  int comparar;

  /* busca id de los DTDs */
  dtd1 = dtd_get_dtd_index(dtd_key_1);
  dtd2 = dtd_get_dtd_index(dtd_key_2);

  if (dtd1 != -1 && dtd2 != -1 && dtd1 != dtd2) {
    
    /* lista los elementos sólo en dtd1*/
    printf("Elements only in %s:\n", dtd_key_1);
    num_listados = 0;
    for (i = 0; i < elm_data_num; i++) {
      if (elm_list[i].contenttype[dtd1]
	  && !elm_list[i].contenttype[dtd2]) {
	printf("%s\n", elm_list[i].name);
	num_listados++;
      }
    }
    if (!num_listados) {
      printf("--none--\n");
    }
    printf("\n");

    /* lista los elementos sólo en dtd2*/
    printf("Elements only in %s:\n", dtd_key_2); 
    num_listados = 0;
    for (i = 0; i < elm_data_num; i++) {
      if (elm_list[i].contenttype[dtd2]
	  && !elm_list[i].contenttype[dtd1]) {
	printf("%s\n", elm_list[i].name);
	num_listados++;
      }
    }
    if (!num_listados) {
      printf("--none--\n");
    }
    printf("\n");

    /* lista los elementos comunes con atributos distintos*/
    printf("Common elements with different attribute list:\n"); 
    printf("\t[%s]\n", dtd_key_1); 
    printf("\t\t\t[%s]\n", dtd_key_2); 
    num_listados = 0;
    for (i = 0; i < elm_data_num; i++) {
      comparar = comparar_elemento(i, dtd1, dtd2);
      if (comparar == 1)
	num_listados++;
    }

    if (num_listados) {
      printf("--none--\n");
    }

  } else {
    if (dtd1 == dtd2)
      fprintf(stderr, "DTDs must be different\n");
    else {
      if (dtd1 == -1)
	fprintf(stderr, "DTD %s not found\n", dtd_key_1);
      if (dtd2 == -1)
	fprintf(stderr, "DTD %s not found\n", dtd_key_2);
    }
  }
}


/*
 * Vuelca un listado de los DTDs integrados y sus claves
 *
 */
void listar_dtds()
{
  int i;

  for (i = 0; i < DTD_NUM; i++) {
    printf("%s (%s)\n", dtd_name[i], dtd_key[i]);
  }
}


/*
 * Compara los atributos de un elemento en dos DTD distintos
 *
 * Devuelve:
 *   0: iguales
 *   1: atributos distintos
 *  -1: elemento no definido en alguno de los dos DTD
 */
int comparar_elemento(int elmid, int dtd1, int dtd2)
{
  int solo1[ELM_ATTLIST_LEN];
  int solo2[ELM_ATTLIST_LEN];
  int i, k;

  /* ¿está en ambos DTD? */
  if (!elm_list[elmid].contenttype[dtd1]
      || !elm_list[elmid].contenttype[dtd2])
    return -1;

  /* compara atributos */
  atributos_solo_en_uno(elm_list[elmid].attlist[dtd1],
			elm_list[elmid].attlist[dtd2],
			solo1);
  atributos_solo_en_uno(elm_list[elmid].attlist[dtd2],
			elm_list[elmid].attlist[dtd1],
			solo2);

  if (solo1[0] != -1 || solo2[0] != -1) {
    printf("%s\n", elm_list[elmid].name);
    for(i = 0; solo1[i] != -1; i++) {
      printf("\t%d %s\n", solo1[i], att_list[solo1[i]].name);
      for (k = 0; solo2[k] != -1; k++) {
	if (!strcmp(att_list[solo1[i]].name, att_list[solo2[k]].name)) {
	  printf("\t\t\t%d %s\n", solo2[k], att_list[solo2[k]].name);
	  solo2[k] = -2;
	}
      }
    }
    for(i = 0; solo2[i] != -1; i++) {
      if (solo2[i] >= 0) 
	printf("\t\t\t%d %s\n", solo2[i], att_list[solo2[i]].name);
    }
  }

  return (solo1[0] == -1 && solo2[0] == -1);
}

int atributos_solo_en_uno(int *lista1, int *lista2, int* resultado)
{
  int i, j, n;
  int encontrado;

  for (i = 0, n = 0; lista1[i] != -1; i++) {
    encontrado = 0;
    for (j = 0; lista2[j] != -1 && !encontrado; j++) {
      if (lista1[i] == lista2[j])
	encontrado = 1;
    }

    if (!encontrado)
      resultado[n++] = lista1[i];
  }
  resultado[n] = -1;

  return n > 0;
}

char *readAttBuffer(int buff_ptr)
{
  if (buff_ptr < 0) return NULL;
  if (buff_ptr > att_buffer_num) return NULL;
  
  return &att_buffer[buff_ptr];
}

char *readElmBuffer(int buff_ptr)
{
  if (buff_ptr < 0) return NULL;
  if (buff_ptr > elm_buffer_num) return NULL;
  
  return &elm_buffer[buff_ptr];
}







/*
 * ==================================================================
 * útiles para pasar datos a cadenas
 * ==================================================================
 *
 */
char *envToString(int environment)
{
  int i;
  static char str[256];

  str[0]=0;

  for (i=0; i<DTD_NUM; i++)
    if (environment & DTD_MASK(i)) {
      strcat(str,dtd_name[i]);
      strcat(str," ");
    }

  return str;
}


char *attTypeToString(int attType)
{
  static char str[256];

  switch (attType) {
  case ATTTYPE_CDATA:
    strcpy(str, "CDATA");
    break;
  case ATTTYPE_ID:
    strcpy(str, "ID");
    break;
  case ATTTYPE_IDREF:
    strcpy(str, "IDREF");
    break;
  case ATTTYPE_IDREFS:
    strcpy(str, "IDREFS");
    break;
  case ATTTYPE_ENTITY:
    strcpy(str, "ENTITY");
    break;
  case ATTTYPE_ENTITIES:
    strcpy(str, "ENTITIES");
    break;
  case ATTTYPE_NMTOKEN:
    strcpy(str, "NMTOKEN");
    break;
  case ATTTYPE_NMTOKENS:
    strcpy(str, "NMTOKENS");
    break;
  case ATTTYPE_ENUMERATED:
    strcpy(str, "ENUMERATED");
    break;
  case ATTTYPE_NOTATION:
    strcpy(str, "NOTATION");
    break;

  default:
    strncpy(str,dtd_att_read_buffer(attType),255);
  }

  return str;
}




char *defDeclToString(defaultDecl_t def)
{
  static char str[32];

  switch (def) {
  case DEFDECL_DEFAULT:
    strcpy(str,"#DEFAULT");
    break;
  case DEFDECL_REQUIRED:
    strcpy(str,"#REQUIRED");
    break;
  case DEFDECL_IMPLIED:
    strcpy(str,"#IMPLIED");
    break;
  case DEFDECL_FIXED:
    strcpy(str,"#FIXED");
    break;
  }

  return str;
}


/* movida a dtd_util.c */
#if 0
/* función recursiva que pasa a la cadena el contentspec del buffer */
char *contentspecToString(unsigned char *buff, char *str, contentType_t conttype,int *len_buff)
{
  unsigned char v;
  int i;

  switch (conttype) {
  case CONTTYPE_NONE:
    strcpy(str,"Undefined");
    *len_buff= 0;
    break;
  case CONTTYPE_EMPTY:
    strcpy(str,"EMPTY");
    *len_buff= 0;
    break;
  case CONTTYPE_ANY:
    strcpy(str,"ANY");
    *len_buff= 0;
    break;
  case CONTTYPE_MIXED:
    {
      strcpy(str,"(#PCDATA");
      for (i=0; buff[i]; i++) {
	strcat(str,"|");
	strcat(str,elm_list[buff[i]&(~CSPEC_ELM_MASK)].name);
      }

      strcat(str,")");
    }
    *len_buff= i;
    break;

  case CONTTYPE_CHILDREN:
    {
      char separador[2];
      int num_items, avance;

      i= 0;
      num_items= 0;

      v= buff[i++];
      if (!(v&CSPEC_ELM_MASK)&&((v&CSPEC_PAR_MASK)!=CSPEC_PAR_O)) {
	strcpy(str,"<error>");
	break;
      }

      if (v&CSPEC_CHOICE) strcpy(separador,"|"); /* choice */
      else strcpy(separador,",");                /* enumerate */
      
      strcpy(str,"(");

      while ((((v=buff[i++])&CSPEC_PAR_MASK)!=CSPEC_PAR_C)
	     || (v&CSPEC_ELM_MASK)) {
	if (num_items) strcat(str,separador);
	num_items++;

	if (!(v&CSPEC_ELM_MASK)&&((v&CSPEC_PAR_MASK)==CSPEC_PAR_O)) {
	  /* recursion */
	  contentspecToString(&buff[i-1],str+strlen(str),conttype,&avance);
	  /* busca donde acaba */
	  /*
	  for ( ; ((buff[i]&CSPEC_PAR_MASK)!=CSPEC_PAR_C)
		  ||(buff[i]&CSPEC_ELM_MASK); i++);
	  i++;
	  */
	  i+= avance-1;
	} else {
	  /* elemento aislado */
	  if (!(v&CSPEC_ELM_MASK)) {
	    strcat(str,"<error>");
	    break;
	  }
	  strcat(str,elm_list[v&(~CSPEC_ELM_MASK)].name);
	}
      }
      
      strcat(str,")");
      switch (buff[0]& CSPEC_NUM_MASK) {
      case CSPEC_AST:
	strcat(str,"*");
	break;
      case CSPEC_INT:
	strcat(str,"?");
	break;
      case CSPEC_MAS:
	strcat(str,"+");
	break;
      }
      
      *len_buff= i;
    }
  } /* switch (conttype) */

  return str;
}

#endif


