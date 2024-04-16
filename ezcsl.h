#ifndef _EZCSL_H_
#define _EZCSL_H_

#include "stdio.h"

#include "ezcsl_port.h"

#define CSL_BUF_LEN     40  //console buf len (include prefix)
#define HISTORY_LEN     3  //history record
#define PRINT_BUF_LEN   60
#define PARA_LEN_MAX    5
#define SPLIT_CHAR      ',' 

#define EZ_PtoS(param) ((const char*)(param))   //ez_param_t => string
#define EZ_PtoI(param) (*(int*)(param))         //ez_param_t => integer
#define EZ_PtoF(param) (*(float*)(param))       //ez_param_t => float
#define ez_param_t      void*


#define BACKSPACE_KV 0x08
#define TAB_KV 0x09
#define ENTER_KV 0x0d
#define CTRL_C_KV 0x03

typedef enum{
    EZ_OK=0,
    EZ_ERR
}ez_sta_t;

typedef struct CmdUnitObj{
    const char *title_main;
    const char *describe;
    void (*callback)(ezuint16_t ,ez_param_t*);
    struct CmdUnitObj *next;
}Ez_CmdUnit_t;

typedef struct CmdObj{
    struct CmdUnitObj *unit;
    const char *title_sub;
    const char *describe;
    ezuint16_t id;
    ezuint8_t para_num;
    const char *para_desc;
    struct CmdObj *next;
}Ez_Cmd_t;

extern void ezcsl_init(const char *prefix,const char *welcome);
extern void ezcsl_deinit(void); 
extern void ezcsl_tick(void);
extern Ez_CmdUnit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,void (*callback)(ezuint16_t,ez_param_t* ));
extern ez_sta_t ezcsl_cmd_register(Ez_CmdUnit_t *unit, ezuint16_t id, const char *title_sub, const char *describe, const char* para_desc);
extern void ezport_send_str(char *str, ezuint16_t len);
#define ezcsl_send_printf(fmt, ...)                                        \
    do {                                                                   \
        ezuint16_t _d_printed;                                               \
        char _d_dat_buf[PRINT_BUF_LEN];                                    \
        _d_printed = snprintf(_d_dat_buf, PRINT_BUF_LEN, fmt, ##__VA_ARGS__); \
        ezport_send_str(_d_dat_buf, _d_printed);                                 \
    } while (0)
#endif