/*
 * types.h
 *
 */

#ifndef TYPES_H 
#define TYPES_H



#define DTD_MASK(x)            (1<<(x))




/* defaultDecl */
typedef enum {
  DEFDECL_DEFAULT,
  DEFDECL_REQUIRED,
  DEFDECL_IMPLIED,
  DEFDECL_FIXED
} defaultDecl_t;


/* attType */
#define ATTTYPE_CDATA         -2
#define ATTTYPE_ID            -3
#define ATTTYPE_IDREF         -4
#define ATTTYPE_IDREFS        -5
#define ATTTYPE_ENTITY        -6
#define ATTTYPE_ENTITIES      -7
#define ATTTYPE_NMTOKEN       -8
#define ATTTYPE_NMTOKENS      -9 
#define ATTTYPE_ENUMERATED    -10
#define ATTTYPE_NOTATION      -11


/* contentspec type */
typedef enum {
  CONTTYPE_NONE,    /* no válido: elemento no definido en el DTD */
  CONTTYPE_EMPTY,
  CONTTYPE_ANY,
  CONTTYPE_MIXED,
  CONTTYPE_CHILDREN
} contentType_t;



struct _att_data {
  char           name[ATT_NAME_LEN];
  int            attType;
  defaultDecl_t  defaultDecl;
  int            defaults;
  int            environment;
};

typedef struct _att_data att_data_t;




struct _elm_data {
  char           name[ELM_NAME_LEN]; 
  contentType_t  contenttype[DTD_NUM];
  int            contentspec[DTD_NUM];
  int            attlist[DTD_NUM][ELM_ATTLIST_LEN];
  int            environment;
};

typedef struct _elm_data elm_data_t;








/* para codificar contentspec en el buffer */
#define CSPEC_PAR_O    0x01 /* 0000 00 01 '(' */
#define CSPEC_PAR_C    0x02 /* 0000 00 10 ')' */

#define CSPEC_AST      0x04 /* 0000 01 00 '*' */
#define CSPEC_MAS      0x08 /* 0000 10 00 '+' */
#define CSPEC_INT      0x0c /* 0000 11 00 '?' */

#define CSPEC_CHOICE   0x10 /* 0001 00 00 '|' */

#define CSPEC_ELM_MASK 0x80 /* 1000 00 00 */
#define CSPEC_PAR_MASK 0x03 /* 0000 00 11 */
#define CSPEC_NUM_MASK 0x0C /* 0000 11 00 */

#define CSPEC_ISPAR(x)     (!((x) & CSPEC_ELM_MASK) && ((x) & CSPEC_PAR_MASK))
#define CSPEC_PAR(x)       ((x) & CSPEC_PAR_MASK)
#define CSPEC_ISCHOICE(x)  ((x) & CSPEC_CHOICE)
#define CSPEC_NUM(x)       (((x) & CSPEC_NUM_MASK))
#define CSPEC_ISELM(x)    ((x) & CSPEC_ELM_MASK)
#define CSPEC_ELM(x)      ((x) & ~CSPEC_ELM_MASK)

#endif


