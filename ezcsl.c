#include "ezcsl.h"
#include "string.h"
#include "stdlib.h"

/* your include begin */
#include "stdio.h"
/* your include end */


#define PRINT_BUF_LEN   60
#define PARA_LEN_MAX 5
#define STR_TO_PARA atoi

#define DBGprintf printf

const char* strNULL="";
#define CHECK_NULL_STR(c) ((c)==NULL?strNULL:(c))
typedef struct CmdHistory{
    char history[CSL_BUF_LEN];
    struct CmdHistory *next;
}cmd_history_t;


static struct EzCslHandleStruct {
    uint8_t prefix_len;
    char buf[CSL_BUF_LEN];
    uint16_t bufp;
    uint16_t bufl;
    uint8_t historyp;
} ezhdl;

/* ez console port ,user need to achieve this himself */
void ezport_receive_a_char(char c);
void ezport_send_str(char *str, uint16_t len);

void ezcsl_init(const char *prefix ,const char *welcome);
void ezcsl_send_printf(const char *fmt, ...);
static void ezcsl_submit(void);

static cmd_history_t *history_head=NULL;
static void history_add(char *buf);
static void history_load(uint8_t idx);

static Ez_Cmd_t *cmd_head=NULL;
static Ez_CmdUnit_t *cmd_unit_head=NULL;
Ez_CmdUnit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,void (*callback)(uint16_t,ez_param_t *));
ez_sta_t ezcsl_cmd_register(Ez_CmdUnit_t *unit,uint16_t id,const char *title_sub,const char *describe,uint8_t para_num);

/* ez inner cmd */
static void ezcsl_cmd_help_callback(uint16_t id,ez_param_t *para);

#define EZCSL_RST()                                   \
    do {                                              \
        ezport_send_str(ezhdl.buf, ezhdl.prefix_len); \
        ezhdl.buf[ezhdl.prefix_len] = 0;              \
        ezhdl.bufl = ezhdl.prefix_len;                \
        ezhdl.bufp = ezhdl.prefix_len;                \
    } while (0)

/**
 * use this function by `extern void ezport_receive_a_char(char c)`
 * place it in a loop
 * @param  c the char from input
 * @author Jinlin Deng
 */
void ezport_receive_a_char(char c)
{
    static uint8_t direction_flag = 0; // direction keys
    if (!direction_flag) {
        if (c >= 0x20 && c <= 0x7e && ezhdl.bufl < CSL_BUF_LEN) {
            /* visible char */
            for (uint16_t i = ezhdl.bufl; i >= ezhdl.bufp + 1; i--) {
                ezhdl.buf[i] = ezhdl.buf[i - 1];
            }
            ezhdl.buf[ezhdl.bufp] = c;
            ezcsl_send_printf("\033[s");

            ezport_send_str(ezhdl.buf + ezhdl.bufp, ezhdl.bufl - ezhdl.bufp + 1);
            ezcsl_send_printf("\033[u\033[1C");
            ezhdl.bufp++;
            ezhdl.bufl++;
        } else if (c == 0x08 && ezhdl.bufp > ezhdl.prefix_len) { // cannot delete the prefix
            /* backspace */
            ezcsl_send_printf("\033[1D\033[s");
            for (uint16_t i = ezhdl.bufp - 1; i < ezhdl.bufl; i++) {
                ezhdl.buf[i] = ezhdl.buf[i + 1];
                ezport_send_str(ezhdl.buf + i, 1);
            }
            ezhdl.bufp--;
            ezhdl.bufl--;
            ezcsl_send_printf("\033[K\033[u");
        } else if (c == 0x0d) {
            /* enter */
            ezhdl.buf[ezhdl.bufp] = 0; // cmd end
            ezcsl_submit();
        }  else if (c == 0x03) {
            /* ctrl+c */
            ezcsl_send_printf("^C\r\n");
            EZCSL_RST();
        } else if (c == 0) {
            direction_flag = 1;
        }
    } else {
        if (c == 0x4b) {
            /* left arrow */
            if (ezhdl.bufp > ezhdl.prefix_len) {
                ezcsl_send_printf("\033[1D");
                ezhdl.bufp--;
            }
        } else if (c == 0x4d) {
            /* right arrow */
            if (ezhdl.bufp < ezhdl.bufl) {
                ezcsl_send_printf("\033[1C");
                ezhdl.bufp++;
            }
        }
        else if (c == 0x48) {
            /* up arrow */
            EZCSL_RST();
        }else if (c == 0x50) {
            /* down arrow */
            EZCSL_RST();
        }
        direction_flag = 0;
    }
    // DBGprintf("input :(%x)",c);
}

/**
 * use this function to send
 * @param str str need to send
 * @param len the length of the str
 * @author Jinlin Deng
 */
void ezport_send_str(char *str, uint16_t len)
{
    /**
     * Write your code here
     */
    for (int i = 0; i < len; i++) {
        putchar(str[i]);
    }
}


/**
 * init
 * @param
 * @author Jinlin Deng
 */
void ezcsl_init(const char *prefix,const char *welcome)
{
    ezhdl.prefix_len = strlen(prefix);
    uint16_t i, j;
    for (i = 0; i < CSL_BUF_LEN; i++) {
        ezhdl.buf[i] = i < ezhdl.prefix_len ? prefix[i] : 0;
    }

    ezhdl.bufp = ezhdl.prefix_len;
    ezhdl.bufl = ezhdl.prefix_len;
    ezhdl.historyp = 0;
    Ez_CmdUnit_t *unit = ezcsl_cmd_unit_create("?","help",ezcsl_cmd_help_callback);
    ezcsl_cmd_register(unit,0,NULL,NULL,0);
    ezport_send_str((char*)welcome,strlen(welcome));
    ezcsl_send_printf("you can input '?' for help\r\n");
    ezport_send_str(ezhdl.buf, ezhdl.prefix_len);
}


void ezcsl_send_printf(const char *fmt, ...)
{
    uint16_t printed;
    va_list args;
    char dat_buf[PRINT_BUF_LEN];
    va_start(args, fmt);
    printed = vsprintf(dat_buf, fmt, args);
    va_end(args);
    ezport_send_str(dat_buf, printed);
}


static void ezcsl_submit(void)
{
    uint8_t paranum=0;
    ez_param_t para[PARA_LEN_MAX];
    char *cmd=ezhdl.buf+ezhdl.prefix_len;
    const char *subtitle=strNULL;
    const char *maintitle=strNULL;
    
    char *a_split;
    uint8_t split_cnt=0;

    ezhdl.buf[ezhdl.bufl]=','; // add a ',' to the end for strtok 
    ezhdl.buf[ezhdl.bufl+1]=0; // add a ',' to the end for strtok 
    while (1) {
        a_split = strtok((char *)cmd, ",");
        if (a_split != NULL) {
            switch (split_cnt) {
            case 0:
                maintitle=a_split;
                break;
            case 1:
                subtitle=a_split;
                break;
            default:
            if(paranum<PARA_LEN_MAX){
                para[paranum]=(ez_param_t)STR_TO_PARA(a_split);
                paranum++;
            }
                break;
            }
        } else {
            break;
        }
        split_cnt++;
        cmd=NULL; //for strtok continue
    };
    

    ezcsl_send_printf("\r\n");
    // Cmd Match 
    Ez_Cmd_t *cmd_p = cmd_head;
    uint8_t match_ok_flag=0;
    while (cmd_p!= NULL) {
        if (strcmp(cmd_p->unit->title_main, maintitle) == 0) {
            match_ok_flag = 1;
            if (strcmp(cmd_p->title_sub, subtitle) == 0) {
                match_ok_flag = 2;
                if (cmd_p->para_num == paranum) {
                    cmd_p->unit->callback(cmd_p->id,para); // user can use `ezcsl_send_printf` in callback
                } else {
                    ezcsl_send_printf("\033[31mFormat Error:\033[m %s,%s,<...>,<N=%d>\r\n", maintitle, subtitle, cmd_p->para_num);
                }
                break;
            }
        }
        cmd_p = cmd_p->next;
    }

    switch (match_ok_flag)
    {
    case 0:
        ezcsl_send_printf("\033[31mUnknown Command\033[m %s\r\n",maintitle);
        break;
    case 1:
        cmd_p = cmd_head;
        ezcsl_send_printf("\033[32mSub Command & Description List\033[m \r\n");
        ezcsl_send_printf("\033[32m=========================\033[m \r\n");
        while (cmd_p != NULL) {
            if (strcmp(cmd_p->unit->title_main, maintitle) == 0) {
                ezcsl_send_printf("\033[6m%s,%s:\033[m  %s\r\n", cmd_p->unit->title_main, cmd_p->title_sub, cmd_p->describe);
            }
            cmd_p = cmd_p->next;
        }
        ezcsl_send_printf("\033[32m=========================\033[m \r\n");
        break;
    default:
        break;
    }
        
    
    EZCSL_RST();
}

static void ezcsl_tabfix(void)
{
    // ezcsl_send_printf("\r\n");

    // ezcsl_send_printf("\r\n");
}


/**
 * create a cmd unit
 * @param title_main main title ,cannot null or '' ,length < 10
 * @author Jinlin Deng
 */
Ez_CmdUnit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,void (*callback)(uint16_t,ez_param_t *)){
    if (strlen(title_main)==0 || strlen(title_main)>=10 || callback==NULL){
        return NULL;
    }
    
        Ez_CmdUnit_t *p = cmd_unit_head;
        while (p != NULL) { //duplicate
            if(strcmp(p->title_main,title_main)==0){
                return NULL;
            }
            p = p->next;
        }
        
        Ez_CmdUnit_t *p_add = (Ez_CmdUnit_t *)malloc(sizeof(Ez_CmdUnit_t));
        p_add->describe=CHECK_NULL_STR(describe);
        p_add->next = NULL;
        p_add->title_main = title_main;
        p_add->callback=callback;

        if(cmd_unit_head==NULL){
            cmd_unit_head=p_add;
        }else{
            p = cmd_unit_head;
            while (p->next != NULL) {
                p = p->next;
            }
            p->next = p_add;
        }
        
        return p_add;
}


/**
 * create a cmd unit
 * @param title_sub sub title ,can set null 
 * @param describe describe your cmd
 * @param param_num the number of parameters your cmd need
 * @author Jinlin Deng
 * @return register result
 */
ez_sta_t ezcsl_cmd_register(Ez_CmdUnit_t *unit, uint16_t id, const char *title_sub, const char *describe, uint8_t para_num)
{
    if (para_num > PARA_LEN_MAX) {
        return EZ_ERR;
    }
    Ez_Cmd_t *p = cmd_head;
    while (p != NULL) { // duplicate
        if (strcmp(p->unit->title_main, unit->title_main) == 0 && (strcmp(p->title_sub, title_sub) == 0 || p->id == id)) {
            return EZ_ERR;
        }
        p = p->next;
    }

    Ez_Cmd_t *p_add = (Ez_Cmd_t *)malloc(sizeof(Ez_Cmd_t));
    p_add->describe = CHECK_NULL_STR(describe);
    p_add->next = NULL;
    p_add->title_sub = CHECK_NULL_STR(title_sub);
    p_add->para_num = para_num;
    p_add->unit = unit;
    p_add->id = id;

    if (cmd_head == NULL) {
        cmd_head = p_add;
    } else {
        p = cmd_head;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = p_add;
    }

    return EZ_OK;
}


static void ezcsl_cmd_help_callback(uint16_t id,ez_param_t *para)
{
    Ez_CmdUnit_t *p = cmd_unit_head;
    ezcsl_send_printf("\033[32mMain Command & Description List\033[m \r\n");
    ezcsl_send_printf("\033[32m=========================\033[m \r\n");
    while (p!= NULL) {
        ezcsl_send_printf("%-10s %s\r\n", p->title_main,  p->describe);
        p = p->next;
    }
    ezcsl_send_printf("\033[32m=========================\033[m \r\n");
}