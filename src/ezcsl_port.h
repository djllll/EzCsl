#ifndef _EZCSL_PORT_H_
#define _EZCSL_PORT_H_

#ifdef __cplusplus 
extern "C" { 
#endif 

/* Ez Macro ,Donnot Modify */
#define LOG_LEVEL_PRT  0x01
#define LOG_LEVEL_V    0x02
#define LOG_LEVEL_E    0x04
#define LOG_LEVEL_I    0x08
#define LOG_LEVEL_D    0x10
#define LOG_LEVEL_ALL  0xff
#define LOG_LEVEL_NONE 0x00
#define EZ_NO_MODEM    0x00
#define EZ_YMODEM_1K   0x01


/* Ez Type ,Porting ******************/
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;


/* Ez Function ,Donnot Modify */
void ezport_send_str(char *str, uint16_t len);
void ezport_delay(uint16_t ms);
void ezport_receive_a_char(char c);
void ezport_rtos_mutex_lock(void);
void ezport_rtos_mutex_unlock(void);
void ezport_custom_init(void);
void ezport_custom_deinit(void);


/* Ez Configuration , Porting ******************/
#define CSL_BUF_LEN     50 // console buf len (include prefix)
#define HISTORY_BUF_LEN 40 // history record
#define PRINT_BUF_LEN   150
#define PARA_LEN_MAX    5
#define SPLIT_CHAR      ','
#define LOG_DEFINE      LOG_LEVEL_ALL // log level define
#define USE_EZ_MODEM    EZ_YMODEM_1K

#ifdef __cplusplus 
}
#endif 

#endif
