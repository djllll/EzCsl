#ifndef _EZCSL_H_
#define _EZCSL_H_

#include "stdarg.h"
#include "ezcsl_port.h"

#ifdef __cplusplus 
extern "C" { 
#endif 

#define EZCSL_VERSION "v1.3.0"


/* Ez Console */
#define EZ_PtoS(param)   ((const char *)(param)) // ez_param_t => string
#define EZ_PtoI(param)   (*(int *)(param))       // ez_param_t => integer
#define EZ_PtoF(param)   (*(float *)(param))     // ez_param_t => float
#define EZ_PARAM_NONE          ""
#define EZ_PARAM_INT           "i"
#define EZ_PARAM_FLOAT         "f"
#define EZ_PARAM_STR           "s"
#define ez_param_t       void *

typedef uint8_t ez_log_level_mask_t;

typedef enum{
    EZ_OK=0,
    EZ_ERR
}ez_sta_t;


typedef struct CmdUnitObj{
    const char *title_main;
    const char *describe;
    void (*callback)(uint16_t ,ez_param_t*);
    struct CmdUnitObj *next;
    uint8_t need_sudo;
}ez_cmd_unit_t;

typedef struct CmdObj{
    struct CmdUnitObj *unit;
    const char *title_sub;
    const char *describe;
    uint16_t id;
    uint8_t para_num;
    const char *para_desc;
    struct CmdObj *next;
}ez_cmd_t;




#ifdef USE_EZ_MODEM
typedef enum{
    M_SEND_NEXT = 0,
    M_SEND_REPEAT,
    M_SEND_ABORT
}modem_rev_func_t;

typedef struct{
    enum{
        FILE_INFO_ONLY = 0,
        FILE_INFO_AND_CONTENT,
        FILE_TRANS_OVER
    }frame_type;
    const char* filename;
    uint32_t filesize;
    uint32_t filesize_received;
    char *content;
    uint16_t contentlen;
}modem_file_t;

void ezcsl_modem_set(const char *modem_prefix,modem_rev_func_t (*cb_func)(modem_file_t *));
#endif

void ezcsl_init(const char *prefix ,const char *welcome,const char *sudo_psw);
void ezcsl_deinit(void); 
void ezcsl_reset_prefix(void);
void ezport_send_str(char *str, uint16_t len);
void ezcsl_printf(const char *fmt, ...);
uint8_t ezcsl_tick(void);
uint8_t ezcsl_break_signal(void);
ez_cmd_unit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,uint8_t need_sudo, void (*callback)(uint16_t,ez_param_t*));
ez_sta_t ezcsl_cmd_register(ez_cmd_unit_t *unit, uint16_t id, const char *title_sub, const char *describe, const char* para_desc);

#define MOVE_CURSOR_ABS(n)      "\033["#n"G"
#define ERASE_TO_END()          "\033[K"
#define SAVE_CURSOR_POS()       "\033[s"
#define RESTORE_CURSOR_POS()    "\033[u"
#define CURSOR_FORWARD(n)       "\033["#n"C"
#define CURSOR_BACK(n)          "\033["#n"D"

#define COLOR_BLACK(s)	 	    "\033[0;30m"s"\033[0m"
#define COLOR_L_BLACK(s)	 	"\033[1;30m"s"\033[0m"
#define COLOR_RED(s)      	    "\033[0;31m"s"\033[0m"
#define COLOR_L_RED(s)    	    "\033[1;31m"s"\033[0m"
#define COLOR_GREEN(s)    	    "\033[0;32m"s"\033[0m"
#define COLOR_L_GREEN(s)  	    "\033[1;32m"s"\033[0m"
#define COLOR_YELLOW(s)   	    "\033[0;33m"s"\033[0m"
#define COLOR_L_YELLOW(s) 	    "\033[1;33m"s"\033[0m"
#define COLOR_BLUE(s)     	    "\033[0;34m"s"\033[0m"
#define COLOR_L_BLUE(s)	 	    "\033[1;34m"s"\033[0m"
#define COLOR_PURPLE(s)   	    "\033[0;35m"s"\033[0m"
#define COLOR_L_PURPLE(s)	    "\033[1;35m"s"\033[0m"
#define COLOR_CYAN(s)     	    "\033[0;36m"s"\033[0m"
#define COLOR_L_CYAN(s)   	    "\033[1;36m"s"\033[0m"
#define COLOR_WHITE(s)    	    "\033[0;37m"s"\033[0m"
#define COLOR_L_WHITE(s)    	"\033[1;37m"s"\033[0m"


#if (LOG_DEFINE & LOG_LEVEL_E)
#define EZ_LOGE(TAG, format, ...)                                                                \
    do {                                                                                         \
        ezcsl_printf(MOVE_CURSOR_ABS(0) COLOR_L_RED("E[" TAG "] " format "\r\n"), ##__VA_ARGS__); \
        ezcsl_reset_prefix();                                                                    \
    } while (0)
#else
#define EZ_LOGE(TAG, format, ...) \
    {                             \
        ;                         \
    }
#endif


#if (LOG_DEFINE & LOG_LEVEL_I)
#define EZ_LOGI(TAG, format, ...)                                                                  \
    do {                                                                                           \
        ezcsl_printf(MOVE_CURSOR_ABS(0) COLOR_L_GREEN("I[" TAG "] " format "\r\n"), ##__VA_ARGS__); \
        ezcsl_reset_prefix();                                                                      \
    } while (0)
#else
#define EZ_LOGI(TAG, format, ...) \
    {                             \
        ;                         \
    }
#endif


#if (LOG_DEFINE & LOG_LEVEL_D)
#define EZ_LOGD(TAG, format, ...)                                                                 \
    do {                                                                                          \
        ezcsl_printf(MOVE_CURSOR_ABS(0) COLOR_L_BLUE("D[" TAG "] " format "\r\n"), ##__VA_ARGS__); \
        ezcsl_reset_prefix();                                                                     \
    } while (0)
#else
#define EZ_LOGD(TAG, format, ...) \
    {                             \
        ;                         \
    }
#endif

#if (LOG_DEFINE & LOG_LEVEL_V)
#define EZ_LOGV(TAG, format, ...)                                                   \
    do {                                                                            \
        ezcsl_printf(MOVE_CURSOR_ABS(0) "V[" TAG "] " format "\r\n", ##__VA_ARGS__); \
        ezcsl_reset_prefix();                                                       \
    } while (0)
#else
#define EZ_LOGV(TAG, format, ...) \
    {                             \
        ;                         \
    }
#endif

#if (LOG_DEFINE & LOG_LEVEL_PRT)
#define EZ_PRTL(format, ...)                  \
    do {                                     \
        ezcsl_printf(MOVE_CURSOR_ABS(0)format"\r\n", ##__VA_ARGS__); \
    } while (0)
#else
#define EZ_PRTL(format, ...) \
    {                       \
        ;                   \
    }
#endif

#if (LOG_DEFINE & LOG_LEVEL_PRT)
#define EZ_PRT(format, ...)                  \
    do {                                     \
        ezcsl_printf(format, ##__VA_ARGS__); \
    } while (0)
#else
#define EZ_PRT(format, ...) \
    {                       \
        ;                   \
    }
#endif

#define EZ_NSUDO 0
#define EZ_SUDO  1


/* Ez Ringbuffer */
#define RB_DATA_T unsigned char

typedef enum{
    RB_OK,
    RB_ERR,
    RB_FULL,
    RB_EMPTY
}rb_sta_t;

typedef struct {  
    RB_DATA_T *buffer;  
    uint8_t len;
    uint8_t head;    
    uint8_t tail;    
} ezrb_t;  

ezrb_t *ezrb_create(uint8_t len);
rb_sta_t ezrb_push(ezrb_t *cb,RB_DATA_T dat);
rb_sta_t ezrb_pop(ezrb_t *cb,RB_DATA_T *dat);
void ezrb_destroy(ezrb_t *cb);

/* Ez String */
#define EZSTR_OK 0
#define EZSTR_ERR 1

#define ezstr_ret_t char
#define ezstr_size_t int

ezstr_ret_t estrcat_s(char *_Dst, ezstr_size_t _DstSize, const char *_Src);
ezstr_ret_t estrcatc_s(char *_Dst, ezstr_size_t _DstSize, char _Src);
ezstr_ret_t estrcpy_s(char *_Dst, ezstr_size_t _DstSize, const char *_Src);
ezstr_ret_t estrlen_s(const char *_Str,ezstr_size_t _Size);
ezstr_size_t estrlen(const char *_Str);
ezstr_ret_t estrcmp(const char* _Str1,const char* _Str2);
ezstr_ret_t estrncmp(const char *_Str1, const char *_Str2, ezstr_size_t _Size);
char* estrtokc(char *_Str, char _Deli);



#ifdef __cplusplus 
}
#endif 

#endif