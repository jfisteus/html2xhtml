/*
 * This file has been created automatically by DTDCoder.
 * Do not edit, use DTDCoder instead.
 *
 */

#ifndef DTD_H
#define DTD_H

#define DTD_NUM 8

#define ATT_NAME_LEN 19
#define ELM_NAME_LEN 11
#define ENT_NAME_LEN 25

#define ELM_ATTLIST_LEN 39

/* files that declare types and other macros */
#include "dtd_types.h"
#include "dtd_names.h"

#define DTD_SNAPSHOT_DATE "2009-10-11 13:21:49+0200"

#define elm_data_num 97
#define att_data_num 163
#define ent_data_num 254
#define elm_buffer_num 3011
#define att_buffer_num 1050

/* variables declared in dtd.c */
extern elm_data_t elm_list[];
extern unsigned char elm_buffer[];
extern att_data_t att_list[];
extern unsigned char att_buffer[];

extern int ent_hash[257];
extern char ent_list[][25];

#define ELMID_HTML 0
#define ELMID_HEAD 1
#define ELMID_BODY 11
#define ELMID_FRAMESET 89
#define ELMID_STYLE 6
#define ELMID_SCRIPT 7
#define ELMID_META 4
#define ELMID_P 13
#define ELMID_TITLE 2
#define ELMID_PRE 30
#define ELMID_FRAME 90
#define ELMID_APPLET 64
#define ELMID_A 35
#define ELMID_FORM 68
#define ELMID_IFRAME 9
#define ELMID_IMG 65
#define ELMID_MAP 66
#define ELMID_UL 20
#define ELMID_OL 21
#define ELMID_LI 24
#define ELMID_TABLE 79
#define ELMID_TR 86
#define ELMID_TH 87
#define ELMID_TD 88
#define ELMID_THEAD 81
#define ELMID_TBODY 83
#define ELMID_OBJECT 62
#define ELMID_BIG 55
#define ELMID_SMALL 56
#define ELMID_SUB 50
#define ELMID_SUP 51
#define ELMID_INPUT 70
#define ELMID_SELECT 71
#define ELMID_TEXTAREA 74
#define ELMID_LABEL 69
#define ELMID_BUTTON 77
#define ELMID_FIELDSET 75
#define ELMID_ISINDEX 78
#define ELMID_CENTER 32
#define ELMID_U 57
#define ELMID_S 58
#define ELMID_STRIKE 59
#define ELMID_INS 33
#define ELMID_DEL 34
#define ELMID_AREA 67
#define ELMID_FONT 61
#define ELMID_BASEFONT 60
#define ELMID_DIR 23
#define ELMID_MENU 22
#define ELMID_RUBY 91
#define ELMID_RB 94
#define ELMID_RBC 92
#define ELMID_RP 96
#define ELMID_RT 95
#define ELMID_RTC 93
#define ELMID_BDO 37
#define ELMID_DIV 12
#define ELMID_SPAN 36
#define ELMID_DL 25
#define ELMID_HR 29
#define ELMID_CAPTION 80
#define ELMID_BASE 3
#define ATTID_XML_SPACE 32
#define ATTID_HTTP_EQUIV_0 8
#define ATTID_HTTP_EQUIV_1 151
#define ATTID_CONTENT 10
#define ATTID_XMLNS 4
#endif /* DTD_H */
