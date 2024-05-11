#ifndef _EZCSL_PORT_H_
#define _EZCSL_PORT_H_

#ifdef __cplusplus 
extern "C" { 
#endif 

#include "ezcsl_macro.h"

typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

extern void ezport_send_str(char *str, uint16_t len);
extern void ezport_receive_a_char(char c);

#define CSL_BUF_LEN     40  //console buf len (include prefix)
#define HISTORY_LEN     3  //history record
#define PRINT_BUF_LEN   150
#define PARA_LEN_MAX    5
#define SPLIT_CHAR      ',' 


#define LOG_DEFINE     LOG_LEVEL_ALL  //log level define 

#define LOCK_WAIT_DELAY()

// #define USE_EZ_XMODEM

#ifdef __cplusplus 
}
#endif 

#endif
