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
 * dtd_util.c
 * 
 * (Jes�s Arias Fisteus)
 *
 * Funciones para acceder a la informaci�n codificada de los
 * DTD de XHTML en dtd.c y dtd.h
 *
 * Hay funciones de b�squeda de elementos y atributos por
 * su nombre y de comprobaci�n de contenido de elementos
 * y de valores de atributos...
 *
 *
 */

#include <stdio.h>
#include <string.h>

#include "xchar.h"
#include "mensajes.h"
#include "dtd.h"
#include "dtd_util.h"


extern char *tree_strdup(const char *str);

/* tabla hash para buscar nombres de entidades */
extern int ent_hash[];


/* funciones internas */
static int is_child_valid(int *rule_ptr, int elements[], int num);
static int search_par_close(int rule_ptr);
static int hash_value(const char *cad);
static int dtd_ref_is_valid(xchar *ref);

static int isXmlChar(xchar ch);
static int isXmlNameChar(xchar ch);
static int isXmlLetter(xchar ch);
static int makeXmlCdata(xchar *value);
static int makeXmlId(xchar *value);
static int makeXmlNmtoken(xchar *value);
static int makeXmlNames(xchar *value, int atttype);


/*
 * devuelve el �ndice (n�mero) del dtd cuya clave (key)
 * coincida con la proporcionada
 *
 */
int dtd_get_dtd_index(const char *key)
{
  int i;
  int dtd= -1;

  for (i=0; i<DTD_NUM; i++) {
    if (!strncmp(key, dtd_key[i], DTD_KEY_LEN)) {
      dtd = i;
      break;
    }
  }

  return dtd;
}


/*
 * devuelve el �ndice (n�mero) del dtd cuya clave (key)
 * coincida con la proporcionada
 *
 */
int dtd_get_dtd_index_n(const char *key, size_t key_length)
{
  int i;
  int dtd= -1;

  for (i=0; i<DTD_NUM; i++) {
    if (key_length == strlen(dtd_key[i])
        && !strncmp(key, dtd_key[i], key_length)) {
      dtd = i;
      break;
    }
  }

  return dtd;
}


/*
 * busca un elemento por nombre y devuelve
 * su identificador
 *
 */
int dtd_elm_search(const char *elm_name)
{
  int i;

  for (i=0; i<elm_data_num;i++)
    if (!strcmp(elm_list[i].name, elm_name)) return i;

  /* si no sali� antes, no se encuentra */
  return -1;
}


/*
 * devuelve un puntero a la referencia a entidad o -1 si no se encuentra
 * ent_name llega como '&nombre;'
 *
 */
int dtd_ent_search(const char *ent_name)
{
  char *ent;
  int hash;
  int indice;
  int i;
  
  /* duplica la cadena y se queda con 'nombre' */
  ent= tree_strdup(&ent_name[1]);
  for (i = 0; ent[i] != ';'; i++);
  ent[i]=0;

  /* b�squeda con tabla hash */
  hash= hash_value(ent);
  for (indice=-1, i=ent_hash[hash]; (i<ent_hash[hash+1]) && (indice==-1); i++)
    if (!strcmp(ent,ent_list[i])) indice= i;

/*   free(ent); */
  return indice;
}




/*
 * busca un atributo por nombre en la lista de atributos,
 * comenzando por el id siguiente a 'from'. Devuelve
 * el identificador del atributo o -1 si no se encuentra
 *
 */
int dtd_att_search(const char *att_name, int from)
{
  int i;

  for (i=from+1; i<att_data_num;i++)
    if (!strcmp(att_list[i].name, att_name)) return i;
  
  /* si no sali� antes, no se encuentra */
  return -1;
}




/*
 * busca un atributo con nombre 'att_name'
 * en la lista de atributos dada por 'in'
 *
 * devuelve -1 si no est�, o el �ndice del atributo
 *
 */
int dtd_att_search_list(const char *att_name, const int *in)
{
  int i;

  for (i=0; (i<ELM_ATTLIST_LEN) && (in[i]>=0); i++) {
    if (!strcmp(att_name, att_list[in[i]].name)) return in[i];
  }
  return -1;
}



/*
 * busca un atributo con id 'att_id'
 * en la lista de atributos dada por 'in'
 *
 * devuelve -1 si no est�, o el �ndice del atributo
 *
 */
int dtd_att_search_list_id(int att_id, const int *in)
{
  int i;

  for (i=0; (i<ELM_ATTLIST_LEN) && (in[i]>=0); i++) {
    if (in[i]==att_id) return in[i];
  }
  return -1;
}




/*
 * comprueba si el valor de un atributo es v�lido
 *
 * devuelve 1 si el valor cumple con las especificaciones
 * del DTD o 0 si no
 * devuelve 2 si cumple, pero necesita ser transformado a
 * min�sculas
 *
 * COMPROBACIONES:
 *   - CDATA, ID, IDREF, IDREFS, NMTOKEN, NMTOKENS: comprueba que
 *        los caracteres usados sean v�lidos para ese tipo. Si no
 *        lo son, los pisa con '_'.          
 *
 *   - enumerated: comprueba que el valor est� en la enumeraci�n de
 *        posibles valores. Si no, lo pasa a min�sculas y vuelve
 *        a comprobar.
 *     
 *
 */
int  dtd_att_is_valid(int att_id, xchar *value)
{
  att_data_t *att;

  att = &att_list[att_id];
  return dtd_att_is_valid_by_type(att->attType, att->defaultDecl,
                                  att->defaults, value);
}

int  dtd_att_is_valid_by_type(int att_type, defaultDecl_t default_decl,
                              int defaults, xchar *value)
{
  char *valores;
  char str[256];
  int valid;

  /* 
   * si contenttype>=0, tenemos una lista de posibles valores
   * y se comprueba si est� entre ellos
   *
   */
  if (att_type>=0) {
    int i,k;

    valores= dtd_att_read_buffer(att_type);

    /* valores[0] es '(' */
    i= 1;
    valid=0;
    while (valores[i]) {
      for (k=0; (valores[i]!='|') && (valores[i]!=')'); i++,k++) 
        str[k]= valores[i];
      str[k]= 0; /* termina la cadena */
      i++; /* salta el '|'o ')'*/

      /* comprueba este valor */
      if (!strcmp(str,value)) {
        valid=1;
        break;
      } else {
        xchar lowercase[128];
        xtolower(lowercase,value,128);
        if (!strcmp(str,lowercase)) {
          valid=2;
          break;
        }
      }
    } /* while */
    
    if (valid != 1) return valid;
  } /* if */
  else {
    /* se comprueba que no contenga & o < 
     * puede ser v�lido a�n conteniendo & si
     * forma parte de una referencia
     */
    if (dtd_att_val_search_errors(value)!=-1) return 0;

  } /* else */



  /* 
   * se comprueba que el valor respete la sintaxis
   * asociada a su tipo
   *
   */
  switch (att_type) {
  case ATTTYPE_CDATA:
    if (!makeXmlCdata(value)) return 0;
    break;
  case ATTTYPE_ID:
  case ATTTYPE_IDREF:
    if (!makeXmlId(value)) return 0;
    break;
  case ATTTYPE_NMTOKEN:
    if (!makeXmlNmtoken(value)) return 0;
    break;
  case ATTTYPE_IDREFS:
  case ATTTYPE_NMTOKENS:
    if (!makeXmlNames(value,att_type)) return 0;
    break;
  }


  /* 
   * ahora se comprueba si hay declaraci�n FIXED, en cuyo
   * caso tenemos que comprobar que adquiera ese valor
   *
   */
  if (default_decl==DEFDECL_FIXED) {
    if (!strcmp(value,dtd_att_read_buffer(defaults))) return 1;
    else {
      xchar lowercase[128];
      xtolower(lowercase,value,128);
      if (!strcmp(lowercase,dtd_att_read_buffer(defaults))) return 1;
      else return 0;
    }
  }

  return 1;
}





/**
 * comprueba el valor de un atributo desde el
 * punto de vista de aparici�n de caracteres ilegales
 * ('<' o '&' que no pertenezca a referencia a entidad)
 *
 * devuelve la posici�n en que se encuentre '<' o '&'
 *
 * o -1 si todo es correcto
 *
 */
int dtd_att_val_search_errors(const xchar *value)
{
  int i;
  xchar *v= tree_strdup(value);
  
  for (i=0; v[i]; i++) {
    if (v[i]=='<') {/*free(v);*/return i;}
    else if (v[i]=='&') {
      int k;
      for (k=i+1; v[k]; k++)
        if (v[k]==';') {
          xchar tmp= v[k+1];
          v[k+1]= 0;
          if (!dtd_ref_is_valid(&v[i])) {/*free(v);*/return i;}
          else {
            v[k+1]= tmp;
            break;
          }
        }
      if (!v[k]) {/*free(v);*/return i;}
    }
  } /* for */
  
/*   free(v); */
  return -1;
}





/*
 * lee una cadena de caracteres del buffer de elementos
 *
 */
char *dtd_elm_read_buffer(int buff_ptr)
{
  if (buff_ptr < 0) return NULL;
  if (buff_ptr > elm_buffer_num) return NULL;
  
  return (char*) &elm_buffer[buff_ptr];
}



/*
 * lee una cadena de caracteres del buffer de atributos
 *
 */
char *dtd_att_read_buffer(int buff_ptr)
{
  if (buff_ptr < 0) return NULL;
  if (buff_ptr > att_buffer_num) return NULL;
  
  return (char*) &att_buffer[buff_ptr];
}






/*
 * comprueba si la secuencia identificadores de elemento
 * concuerda con la regla de tipo 'child' dada por el
 * puntero.
 *
 * la secuencia 'elements' es un array de identificadores ordenada
 * el n�mero de elementos del array lo indica 'num'
 * el puntero a la regla es 'rule_ptr'
 *
 * DEVUELVE: 1 si lo es o 0 si no lo es (-1 si lo podr�a ser con
 *                m�s elementos)
 *
 */
int dtd_is_child_valid(int rule_ptr, int elements[], int num)
{
  int rule;
  int valid;

  DEBUG("dtd_is_child_valid()");

  rule= rule_ptr;
  valid=is_child_valid(&rule, elements, num);

  EPRINTF2("   c�digo retornado: %d [regla %d]\n",valid,rule);

  if (valid==num) return 1;
  else if (valid==-2) return -1; /* no v�lido, pero por falta de elm */

  /* no v�lido */
  else return 0;
}



/*
 * comprueba si el elemento child_id puede ser hijo de father_id
 *
 * devuelve 1 si lo puede ser o 0 si no
 *
 * para tipo children, s�lo comprueba que dicho elemento
 * est� dentro de su cadena
 *
 */ 
int dtd_can_be_child(int child, int father, int dtd_num)
{
  int i;

  if (elm_list[father].contenttype[dtd_num]==CONTTYPE_ANY)
    return 1;

  if (elm_list[father].contenttype[dtd_num]==CONTTYPE_EMPTY)
    return 0;

  /* es children o mixed */
  for (i= elm_list[father].contentspec[dtd_num];elm_buffer[i]; i++)
    if ((elm_buffer[i] & CSPEC_ELM_MASK)&&
        ((elm_buffer[i]& ~CSPEC_ELM_MASK)==child))
      return 1;

  return 0;
}



/*
 * comprueba si el elemento elm es de tipo bloque
 *
 * devuelve 1 es de bloque; 0 si es inline
 *
 */ 
int dtd_elm_is_block(int elm)
{

  /* es de bloque si no puede aparecer dentro de un p�rrafo */  
  return ! dtd_can_be_child(elm, ELMID_P, XHTML_TRANSITIONAL);
}







/*
 * ==================================================================
 * FUNCIONES INTERNAS
 * ==================================================================
 *
 */



/*
 *
 * funci�n recursiva en la que se basa el funcionamiento de 
 * dtd_is_child_valid()
 *
 * devuelve: <0:  no es v�lido
 *           num: es v�lido con 'num' elementos coincidentes 
 *
 */
static int is_child_valid(int *rule_ptr, int elements[], int num)
{
  unsigned char data;
  int is_choice;
  int repeat;
  int rule;
  int elm;
  int valid = -1;
  int elm_matched;
  int error_code;
  int coincide_choice;

#ifdef CHILD_DEBUG
  char str[1024];
  int len_buff= 0;
  int i;
#endif

  DEBUG("is_child_valid()");

#ifdef CHILD_DEBUG
  EPRINTF2("    rule: %d; num: %d;\n", *rule_ptr, num);
  EPRINTF1("    %s\n", contentspecToString(&elm_buffer[*rule_ptr], str, 
                                         CONTTYPE_CHILDREN, &len_buff));
  EPRINTF("    :: ");  
  for (i=0; i<num; i++)
    EPRINTF1("%s ", elm_list[elements[i]].name);
  EPRINTF("\n");
#endif

  coincide_choice= 0;
  error_code= -1;
  rule= *rule_ptr;
  data= elm_buffer[rule];

  if (!CSPEC_ISPAR(data) || (!(data & CSPEC_PAR_O)))
    EXIT("dtd_is_child_valid: the rule must begin with '('");

  is_choice= CSPEC_ISCHOICE(data);
  repeat= CSPEC_NUM(data);

#ifdef CHILD_DEBUG
  if (is_choice) EPRINTF("    | ");
  else EPRINTF("    , ");
  switch (repeat) {
  case CSPEC_AST:
    EPRINTF("*\n");
    break;
  case CSPEC_MAS:
    EPRINTF("+\n");
    break;
  case CSPEC_INT:
    EPRINTF("?\n");
    break;
  default:
    EPRINTF("1\n");
    break;
  }
#endif

  elm= 0;
  elm_matched= 0;
  data= elm_buffer[++rule];
  for ( ; ; data= elm_buffer[++rule]) {

    /* mira si es v�lido el elemento actual */
    if (CSPEC_ISELM(data)) {
      /* es un elemento suelto */
      if ((elm<num) && elements[elm]==CSPEC_ELM(data)) valid= 1;
      else if (elm>=num) valid=-2; /* faltan elementos */
      else valid= -1;
#ifdef CHILD_DEBUG
      EPRINTF3("    .    [%d:%d] %s\n",rule,valid,elm_list[CSPEC_ELM(data)].name);
#endif

    } else if (data & CSPEC_PAR_O) {
      /* es una regla compuesta: comienza con '(' */
#ifdef CHILD_DEBUG
      int rule0= rule;
      EPRINTF1("    >>   [%d]\n",rule);
#endif
      valid= is_child_valid(&rule, elements+elm, num-elm);
#ifdef CHILD_DEBUG
      EPRINTF3("    <<   [%d:%d] %s\n",rule,valid,
               contentspecToString(&elm_buffer[rule0], str, 
                                   CONTTYPE_CHILDREN, &len_buff));
      EPRINTF1("         -> %s\n",
               contentspecToString(&elm_buffer[rule+1], str, 
                                   CONTTYPE_CHILDREN, &len_buff));
#endif
   
    } else if (data & CSPEC_PAR_C) {
      /* fin de esta regla */

#ifdef CHILD_DEBUG
      EPRINTF2("    ))   coincide_choice:%d; elm_matched:%d\n",
               coincide_choice, elm_matched);
#endif

      if (is_choice) {
        /* error si no hubo coincidencias en iteraciones anteriores
         * y el tipo no permite 0 coincidencias
         */
        *rule_ptr= rule;
        if ((coincide_choice)||(elm_matched>0)
            ||(repeat==CSPEC_INT)||(repeat==CSPEC_AST)) 
          return elm_matched;
        else return error_code; 
      } else {
        /* si no es choice, se cumple */
        elm_matched= elm;
        
        /* retorna si s�lo permite una coincidenecia */
        if ((repeat==CSPEC_INT)||(repeat==0)) {
          *rule_ptr= rule;
          return elm_matched;
        } else {
          /* vuelve a verificar */
          rule= *rule_ptr;
#ifdef CHILD_DEBUG
      EPRINTF("    --R--\n");
#endif
          continue;
        }
      }
    } else EXIT("is_child_valid: incorrect rule");



    /* comprueba el resultado de este elemento */
    if (valid>=0) {

#ifdef CHILD_DEBUG
      EPRINTF1("    --V-- %d\n", valid);
#endif

      /* OK, se mira el siguiente elemento (si no es CHOICE) o se finaliza */
      elm+= valid;

      /* si es choice, busca el fin y retorna */
      if (is_choice) {
        elm_matched= elm;
        coincide_choice= 1;

        /* si no se emparej� ninguno, 
         * aunque sea v�lido, se contin�a con el siguiente */
        if (elm_matched) {
          /* si es '*' o '+', o contin�a el bucle */
          if ((!repeat)||(repeat==CSPEC_INT)) {
            rule= search_par_close(rule+1);
            *rule_ptr= rule;
            return elm;
          }
          else rule= *rule_ptr;
        }
      }
      
      /* no es choice,
       * o es choice v�lido sin emparejar ning�n elemento
       * ==> se mira si cumple el siguiente elemento 
       */

    } else {
      /* no v�lido */
      /* si no es choice retorna con error */

#ifdef CHILD_DEBUG
      EPRINTF1("    --NV-- %d\n", valid);
#endif

      if (!is_choice) {
        rule= search_par_close(rule+1);
        *rule_ptr= rule;

        /* error si no hubo coincidencias en iteraciones anteriores
         * y el tipo no permite 0 coincidencias
         */
        if ((elm_matched>0)||(repeat==CSPEC_INT)||(repeat==CSPEC_AST)) 
          return elm_matched;    /* no hay error */
        else return valid;       /* hay error */
      }

      /* es choice, se sigue probando */
      if (valid==-2) error_code= -2;
    }
  } /* for */

}





static int search_par_close(int rule_ptr)
{
  int num_par;
  unsigned char data;
  
  data= elm_buffer[rule_ptr];
  for( num_par=0; !CSPEC_ISPAR(data) || (num_par>0) || !(data & CSPEC_PAR_C) ; 
       data=elm_buffer[++rule_ptr]) {
    if (CSPEC_ISPAR(data) && (data & CSPEC_PAR_C)) num_par--;
    else if (CSPEC_ISPAR(data) && (data & CSPEC_PAR_O)) num_par++;
  }

  return rule_ptr;
}




static int hash_value(const char *cad)
{
  int i;
  int hash=0;

  for (i=0; cad[i]; i++)
    hash= (hash*31 + cad[i]) & 0xff;  /* %256 */

  return hash;
}




/**
 * comprueba que una referencia sea v�lida
 * llega como '&aacute;' o '&#333;'
 *
 * devuelve 1 si es v�lida o 0 si no
 *
 */
static int dtd_ref_is_valid(xchar *ref)
{
  if ((ref[0]!='&')||(ref[strlen(ref)-1]!=';')) return 0;

  if (ref[1]=='#') {
    /* referencia a car�cter */
    int hexa;
    int i;

    if (ref[2]=='x') hexa= 1;
    else hexa= 0;

    for (i= hexa+2; ref[i]!=';'; i++) {
      if (((ref[i]<'0')||(ref[i]>'9')) && 
          (!hexa || (
                     ((ref[i]<'a')||(ref[i]>'f'))
                     && ((ref[i]<'A')||(ref[i]>'F')))))
        return 0;
    }
  } else /* referencia a entidad */
    if (dtd_ent_search(ref) == -1) return 0;

  return 1;
}



/*
 * comprueba que el valor sea CDATA de XML
 *
 * los caracteres que no lo cumplan son pisados con '_'
 *
 */
static int makeXmlCdata(xchar *value)
{
  int i;
  
  /* verifica si cumple: Char* */
  for (i=0; value[i];i++) 
     if (!isXmlChar(value[i])) value[i]='_';
  
  return 1;
}




/*
 * comprueba que el valor sea un ID o IDREF de XML
 *
 * los caracteres que no lo cumplan son pisados con '_'
 *
 */
static int makeXmlId(xchar *value)
{
  /* ID e IDREF: (Name)= (Letter | '_' | ':') (NameChar)* */
  int i;
  
  if (!value[0]) return 0;

  /*
   * da problemas cambiar el valor del identificador, porque
   * los enlaces se rompen. Se prefiere que �ste sea eliminado
   *
   */
#if 0
  /* primer car�cter: Letter | '_' | ':' */
  if ((value[0]!='_')&&(value[0]!=':')&&(!isXmlLetter(value[0])))
    value[0]= '_';
  
  /* el resto: NameChar* */
  for (i=1; value[i];i++) 
    if (!isXmlNameChar(value[i])) value[i]='_';
#endif  

  /* primer car�cter: Letter | '_' | ':' */
  if ((value[0]!='_')&&(value[0]!=':')&&(!isXmlLetter(value[0])))
    return 0;
  
  /* el resto: NameChar* */
  for (i=1; value[i];i++) 
    if (!isXmlNameChar(value[i])) return 0;



  return 1;
}


/*
 * comprueba que el valor sea un NMTOKEN de XML
 *
 * los caracteres que no lo cumplan son pisados con '_'
 *
 */
static int makeXmlNmtoken(xchar *value)
{
  /* NMTOKEN: Nmtoken= (NameChar)* */
  int i;
  
  if (!value[0]) return 0;

  /* NameChar* */
  for (i=0; value[i];i++) 
    if (!isXmlNameChar(value[i])) value[i]='_';
  
  return 1;
}


/*
 * comprueba el valor de un atributo de tipo
 * IDREFS y NMTOKENS
 *
 */
static int makeXmlNames(xchar *value, int atttype)
{
  xchar *ini,*fin;
  xchar tmp;

  ini= value;
  fin= value;

  while (1) {
    for ( ;
         (*fin) && (*fin!=0x20) && (*fin!=0x09) 
           && (*fin!=0x0a) && (*fin!=0x0d);
         fin++);
    tmp= *fin;
    *fin= 0;

    switch (atttype) {
    case ATTTYPE_IDREFS:
      if (!makeXmlId(ini)) {*fin= tmp; return 0;}
      break;
    case ATTTYPE_NMTOKENS:
      if (!makeXmlNmtoken(ini)) {*fin= tmp; return 0;}
      break;
    default:
      *fin= tmp;
      return 0;
    }
    
    *fin= tmp;

    for ( ;
         (*fin) && ((*fin==0x20) || (*fin==0x09) 
           || (*fin==0x0a) || (*fin==0x0d));
         fin++);
    ini= fin;
    if (!*fin) break;
  }

  return 1;
}








/*
 * comprueba si el car�cter es Char seg�n XML
 *
 * devuelve 1 si lo es o 0 si no
 *
 */
static int isXmlChar(xchar ch)
{
  if ((ch>=0x20) || (ch=0x09) || (ch=0x0a) || (ch=0x0d))
    return 1;
  else return 0;
}


/*
 * comprueba si el car�cter es Letter seg�n XML
 *
 * devuelve 1 si lo es o 0 si no
 *
 */
static int isXmlLetter(xchar ch)
{
  if (((ch>=0x41)&&(ch<=0x5A)) ||
      ((ch>=0x61)&&(ch<=0x7A)) ||
      ((ch>=(char)0xC0)&&(ch<=(char)0xD6)) ||
      ((ch>=(char)0xD8)&&(ch<=(char)0xF6)) ||
      ((ch>=(char)0xF8) /*&&(ch<=0xFF)*/ )) 
                    /* comentado porque con char es siempre cierto */
    return 1;
  else return 0;
}


/*
 * comprueba si el car�cter es NameChar seg�n XML
 *
 * devuelve 1 si lo es o 0 si no
 *
 */
static int isXmlNameChar(xchar ch)
{
  if (((ch>=0x41)&&(ch<=0x5A)) || /* Letter */
      ((ch>=0x61)&&(ch<=0x7A)) ||
      ((ch>=(char)0xC0)&&(ch<=(char)0xD6)) ||
      ((ch>=(char)0xD8)&&(ch<=(char)0xF6)) ||
      ((ch>=(char)0xF8)/*&&(ch<=0xFF)*/) ||
      ((ch>=(char)0xF8)/*&&(ch<=0xFF)*/) ||
      ((ch>=0x30)&&(ch<=0x39)) || /* Digit */
      (ch==(char)0xB7)               || /* Extender */
      (ch=='.') || (ch=='-')   ||
      (ch==':') || (ch=='_'))
    return 1;
  else return 0;
}





/* funci�n recursiva que pasa a la cadena el contentspec del buffer */
char *contentspecToString(char *buff, char *str, contentType_t conttype,int *len_buff)
{
  unsigned char v;
  int i;

  switch (conttype) {
  case CONTTYPE_NONE:
    strcpy(str,"No definido");
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
