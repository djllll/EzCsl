#include "ezcsl.h"
#include "stdio.h"  // vsprintf
#include "stdlib.h" // s_malloc


#if USE_EZ_MODEM == EZ_YMODEM_1K
#define XYMODEM_BUF_LEN 1030
#endif


#define IS_VISIBLE(c)   ((c) >= 0x20 && (c) <= 0x7e)
#define IS_BACKSPACE(c) ((c) == 0x08 || (c) == 0x7f)
#define IS_TAB(c)       ((c) == 0x09)
#define IS_ENTER(c)     ((c) == 0x0d)
#define IS_CTRL_C(c)    ((c) == 0x03)
#define IS_CTRL_D(c)    ((c) == 0x04)

#define TIP_MAIN_CMD_DESC_LIST COLOR_GREEN("Main Command & Description List")
#define TIP_SUB_CMD_DESC_LIST  COLOR_GREEN("Sub Command & Description List")
#define TIP_SPLIT_LINE         COLOR_GREEN("-------------------------")
#define TIP_INCORRECT_PSW      COLOR_RED("\r\nIncorrect Password! Try again.\r\n")
#define TIP_PSW_INPUT          "Password :"

#define EXPAND_DESC(c) ((c) == 's' ? "string" : ((c) == 'i' ? "integer" : ((c) == 'f' ? "float" : "unkown")))

const char *strNULL = "";
#define CHECK_NULL_STR(c) ((c) == NULL ? strNULL : (c))


static struct EzCslHandleStruct {
    /* prefix */
    const char *prefix;
    uint8_t prefix_len;

    /* buffer */
    char buf[CSL_BUF_LEN];
    uint16_t bufp;
    uint16_t bufl;

    /* history */
    char hist_buf[HISTORY_BUF_LEN];
    uint8_t cur_hist_idx;
    uint16_t hist_buf_rest;

    /* ringbuffer */
    ezrb_t *rb;

    /* log */
    ez_log_level_mask_t log_level_mask;

    /* modem */
#if USE_EZ_MODEM != 0
    uint8_t modem_start_flag;
    uint8_t modem_buf[XYMODEM_BUF_LEN];
    uint16_t modem_p;
    const char *modem_prefix;
    modem_rev_func_t (*modem_cb)(modem_file_t *); // filename,filesize,buf,buflen
#endif

    /* su */
    const char *su_psw;
    uint8_t su_checked;
    uint8_t psw_inputing;

    /* occupy */
    volatile uint8_t csl_occupied;
} ezhdl;



/* ez console port function */
#if USE_EZ_MODEM != 0
static ez_sta_t modem_start(void);
static uint16_t crc16_modem(uint8_t *data, uint16_t length);
static void modem_reply(uint8_t reply);
#endif
static void* s_malloc(uint16_t size);
static void s_free(void *mem);
static void ezcsl_reset_empty(void);
static void ezcsl_tabcomplete(void);
static void ezcsl_submit(void);
static void buf_to_history(void);
static uint8_t load_history(void);
static void last_history_to_buf(void);
static void next_history_to_buf(void);
static ez_cmd_t *cmd_head = NULL;
static ez_cmd_unit_t *cmd_unit_head = NULL;

/* ez inner cmd */
static void ezcsl_cmd_help_callback(uint16_t id, ez_param_t *para);



/**
 * @brief 
 * 
 * @param size 
 * @return void* 
 */
static void* s_malloc(uint16_t size){
    ezport_rtos_mutex_lock();
    void *p = malloc(size);
    ezport_rtos_mutex_unlock();
    return p;
}


/**
 * @brief 
 * 
 * @param mem 
 */
static void s_free(void *mem){
    ezport_rtos_mutex_lock();
    free(mem);
    ezport_rtos_mutex_unlock();
}

/**
 * @brief reset with prefix
 *
 */
void ezcsl_reset_prefix(void)
{
    ezcsl_printf(MOVE_CURSOR_ABS(0) "%s" ERASE_TO_END(), ezhdl.prefix);
    ezhdl.buf[0] = 0;
    ezhdl.bufl = 0;
    ezhdl.bufp = 0;
}

/**
 * @brief reset with empty prefix
 *
 */
static void ezcsl_reset_empty(void)
{
    ezcsl_printf(MOVE_CURSOR_ABS(0) ERASE_TO_END());
    ezhdl.buf[0] = 0;
    ezhdl.bufl = 0;
    ezhdl.bufp = 0;
}


/**
 * @param  c the char from input
 */
void ezport_receive_a_char(char c)
{
#if USE_EZ_MODEM != 0
    if (ezhdl.modem_start_flag == 0) {
#endif
        if (ezhdl.csl_occupied && IS_CTRL_C(c)) {
            ezhdl.csl_occupied = 0;
        } else {
            ezrb_push(ezhdl.rb, (uint8_t)c);
        }
#if USE_EZ_MODEM != 0
        ezhdl.modem_p = 0;
    } else {
        ezhdl.modem_buf[ezhdl.modem_p] = (uint8_t)c;
        if (ezhdl.modem_p < XYMODEM_BUF_LEN - 1)
            ezhdl.modem_p++;
    }
#endif
}


/**
 * @brief init
 *
 * @param prefix prefix of shell
 * @param welcome
 * @param su_psw password of su, NULL = no su
 */
void ezcsl_init(const char *prefix, const char *welcome, const char *su_psw)
{
    ezport_custom_init();

#if USE_EZ_MODEM != 0
    ezhdl.modem_start_flag = 0;
    ezhdl.modem_prefix = NULL;
    ezhdl.modem_p = 0;
#endif
    ezhdl.prefix_len = estrlen_s(prefix, CSL_BUF_LEN);
    ezhdl.prefix = prefix;
    ezhdl.bufp = 0;
    ezhdl.bufl = 0;

    ezhdl.cur_hist_idx = 0;
    ezhdl.hist_buf_rest = HISTORY_BUF_LEN;
    for (uint16_t i = 0; i < HISTORY_BUF_LEN; i++) {
        ezhdl.hist_buf[i] = 0;
    }

    ezhdl.su_psw = su_psw;
    ezhdl.psw_inputing = 0;
    ezhdl.su_checked = 0;

    ezhdl.rb = ezrb_create(CSL_BUF_LEN / 2);

    ez_cmd_unit_t *unit = ezcsl_cmd_unit_create("?", "help", 0, ezcsl_cmd_help_callback);
    ezcsl_cmd_register(unit, 0, NULL, NULL, "");
    ezport_send_str((char *)welcome, estrlen(welcome));
    ezcsl_printf("you can input '?' for help ("EZCSL_VERSION")\r\n");
    ezcsl_reset_prefix();
}





/**
 * @brief deinit
 *
 */
void ezcsl_deinit(void)
{
    ez_cmd_t *p1 = cmd_head;
    while (p1 != NULL) {
        ez_cmd_t *p_del = p1;
        p1 = p1->next;
        s_free(p_del);
    }
    ez_cmd_unit_t *p2 = cmd_unit_head;
    while (p2 != NULL) {
        ez_cmd_unit_t *p_del = p2;
        p2 = p2->next;
        s_free(p_del);
    }
    ezrb_destroy(ezhdl.rb);
    ezport_custom_deinit();
}


#define DIR_KEY_DETECT(c, up, down, left, right) \
    do {                                         \
        if (c == left) {                         \
            if (ezhdl.bufp > 0) {                \
                ezcsl_printf(CURSOR_BACK(1));    \
                ezhdl.bufp--;                    \
            };                                   \
        } else if (c == right) {                 \
            if (ezhdl.bufp < ezhdl.bufl) {       \
                ezcsl_printf(CURSOR_FORWARD(1)); \
                ezhdl.bufp++;                    \
            }                                    \
        } else if (c == up) {                    \
            last_history_to_buf();               \
        } else if (c == down) {                  \
            next_history_to_buf();               \
        }                                        \
    } while (0)

#define DELETE_KEY_DETECT(c, delete)                             \
    do {                                                         \
        if (c == delete &&ezhdl.bufp < ezhdl.bufl) {             \
            ezcsl_printf(SAVE_CURSOR_POS());                     \
            for (uint16_t i = ezhdl.bufp; i < ezhdl.bufl; i++) { \
                ezhdl.buf[i] = ezhdl.buf[i + 1];                 \
                ezport_send_str(ezhdl.buf + i, 1);               \
            }                                                    \
            ezhdl.bufl--;                                        \
            ezcsl_printf(ERASE_TO_END() RESTORE_CURSOR_POS());   \
        }                                                        \
    } while (0)


#define IS_POWERSHELL_PREFIX(c) (c == 0x00)
#define IS_BASH_PREFIX(c)       (c == 0x1b)
#define IS_BASH_1_PREFIX(c)     (c == '[')
#define IS_BASH_2_PREFIX(c)     (c == '3')

#define MATCH_MODE_DEFAULT    0
#define MATCH_MODE_POWERSHELL 1
#define MATCH_MODE_BASH       2
#define MATCH_MODE_BASH_1     3
#define MATCH_MODE_BASH_2     4

/**
 * @brief call it in a loop
 * 
 * @return uint8_t can be ignored if you donnot want to exit
 */
uint8_t ezcsl_tick(void)
{
    static uint8_t match_mode = MATCH_MODE_DEFAULT;
    uint8_t c;
    while (ezrb_pop(ezhdl.rb, &c) == RB_OK) {
        switch (match_mode) {
        case MATCH_MODE_POWERSHELL:
            DIR_KEY_DETECT(c, 'H', 'P', 'K', 'M');
            DELETE_KEY_DETECT(c, 'S');
            match_mode = MATCH_MODE_DEFAULT;
            break;
        case MATCH_MODE_BASH:
            if (IS_BASH_1_PREFIX(c)) {
                match_mode = MATCH_MODE_BASH_1;
                break;
            }
            match_mode = MATCH_MODE_DEFAULT;
            break;
        case MATCH_MODE_BASH_1:
            if (IS_BASH_2_PREFIX(c)) {
                match_mode = MATCH_MODE_BASH_2;
                break;
            }
            DIR_KEY_DETECT(c, 'A', 'B', 'D', 'C');
            match_mode = MATCH_MODE_DEFAULT;
            break;
        case MATCH_MODE_BASH_2:
            DELETE_KEY_DETECT(c, '~');
            match_mode = MATCH_MODE_DEFAULT;
            break;
            break;
        case MATCH_MODE_DEFAULT:
        default:
            if (IS_VISIBLE(c) && ezhdl.bufl < CSL_BUF_LEN - 1) {
                /* visible char */
                for (uint16_t i = ezhdl.bufl; i >= ezhdl.bufp + 1; i--) {
                    ezhdl.buf[i] = ezhdl.buf[i - 1];
                }
                ezhdl.buf[ezhdl.bufp] = c;
                if (!ezhdl.psw_inputing) {
                    ezcsl_printf(SAVE_CURSOR_POS());
                    ezport_send_str(ezhdl.buf + ezhdl.bufp, ezhdl.bufl - ezhdl.bufp + 1);
                    ezcsl_printf(RESTORE_CURSOR_POS() CURSOR_FORWARD(1));
                }
                ezhdl.bufp++;
                ezhdl.bufl++;
            } else if (IS_BACKSPACE(c) && ezhdl.bufp > 0) {
                /* backspace */
                if (!ezhdl.psw_inputing) {
                    ezcsl_printf(CURSOR_BACK(1) SAVE_CURSOR_POS());
                }
                for (uint16_t i = ezhdl.bufp - 1; i < ezhdl.bufl - 1; i++) {
                    ezhdl.buf[i] = ezhdl.buf[i + 1];
                    if (!ezhdl.psw_inputing) {
                        ezport_send_str(ezhdl.buf + i, 1);
                    }
                }
                ezhdl.bufp--;
                ezhdl.bufl--;
                if (!ezhdl.psw_inputing) {
                    ezcsl_printf(ERASE_TO_END() RESTORE_CURSOR_POS());
                }
            } else if (IS_ENTER(c)) {
                /* enter */
                ezhdl.buf[ezhdl.bufl] = '\0'; // cmd end
                if (!ezhdl.psw_inputing) {
                    // trip end spaces
                    while(ezhdl.bufl>1){
                        if(ezhdl.buf[ezhdl.bufl-1]==' '){
                            ezhdl.buf[ezhdl.bufl - 1] = '\0';
                            ezhdl.bufl--;
                        } else {
                            break;
                        }
                    }
                    buf_to_history();
                    ezcsl_submit();
                } else {
                    if (estrcmp(ezhdl.su_psw,ezhdl.buf) == 0) {
                        /* password success */
                        ezhdl.su_checked = 1;
                        ezhdl.psw_inputing = 0;
                        /* submit again, TODO it looks not beautiful */
                        last_history_to_buf(); 
                        ezcsl_submit();
                    } else {
                        ezcsl_printf(TIP_INCORRECT_PSW);
                        ezcsl_reset_empty();
                        ezcsl_printf(TIP_PSW_INPUT);
                    }
                }
            } else if (IS_CTRL_C(c)) {
                /* ctrl+c */
                ezcsl_printf("^C\r\n");
                ezcsl_reset_prefix();
                if (ezhdl.psw_inputing) {
                    ezhdl.psw_inputing = 0;
                }
            } else if (IS_CTRL_D(c)) {
                /* ctrl+D */
                ezcsl_printf("^D\r\n");
                ezcsl_reset_prefix();
                if (ezhdl.psw_inputing) {
                    ezhdl.psw_inputing = 0;
                }
                return 1;
            } else if (IS_TAB(c) && !ezhdl.psw_inputing) {
                /* tab */
                ezhdl.buf[ezhdl.bufp] = 0; // cmd end
                ezcsl_tabcomplete();
            } else if (IS_POWERSHELL_PREFIX(c)) {
                match_mode = MATCH_MODE_POWERSHELL;
            } else if (IS_BASH_PREFIX(c)) {
                match_mode = MATCH_MODE_BASH;
            }
            break;
        }
    }
    return 0;
}


/**
 * @brief printf
 *
 * @param fmt
 * @param ...
 */
void ezcsl_printf(const char *fmt, ...)
{
    ezport_rtos_mutex_lock();
    uint16_t printed;
    va_list args;
    char dat_buf[PRINT_BUF_LEN];
    va_start(args, fmt);
    printed = vsnprintf(dat_buf, PRINT_BUF_LEN, fmt, args);
    ezport_send_str(dat_buf, printed);
    va_end(args);
    ezport_rtos_mutex_unlock();
}


/**
 * @brief submit input
 *
 */
static void ezcsl_submit(void)
{
#if USE_EZ_MODEM != 0
    if (ezhdl.modem_prefix != NULL && ezhdl.modem_cb != NULL) {
        if (estrncmp(ezhdl.modem_prefix, ezhdl.buf, estrlen(ezhdl.modem_prefix)) == 0) {
            ezhdl.modem_start_flag = 1;
            if (modem_start() == EZ_ERR) {
                EZ_LOGE("MODEM","Timeout!");
            } else {
                EZ_LOGI("MODEM","Success!");
            }
            ezcsl_reset_prefix();
            ezhdl.modem_start_flag = 0;
            return;
        }
    }
#endif
    uint8_t paranum = 0;
    float paraF[PARA_LEN_MAX];
    int paraI[PARA_LEN_MAX];
    ez_param_t para[PARA_LEN_MAX];
    char *cmd = ezhdl.buf;
    const char *subtitle = strNULL;
    const char *maintitle = strNULL;

    char *a_split;
    uint8_t split_cnt = 0;


    if (ezhdl.bufl > CSL_BUF_LEN - 1) {
        ezhdl.bufl = CSL_BUF_LEN - 1;
    }
    ezhdl.buf[ezhdl.bufl] = SPLIT_CHAR; // add a SPLIT_CHR to the end for estrtokc
    // ezhdl.buf[ezhdl.bufl+1]=0; // add a SPLIT_CHR to the end for estrtokc
    while (1) {
        a_split = estrtokc((char *)cmd, SPLIT_CHAR);
        if (a_split != NULL) {
            switch (split_cnt) {
            case 0:
                maintitle = a_split;
                break;
            case 1:
                subtitle = a_split;
                break;
            default:
                if (paranum < PARA_LEN_MAX && estrlen(a_split) > 0) {
                    para[paranum] = (ez_param_t *)a_split;
                    paranum++;
                }
                break;
            }
        } else {
            break;
        }
        split_cnt++;
        cmd = NULL; // for estrtokc continue
    };


    ezcsl_printf("\r\n");
    // Cmd Match
    ez_cmd_t *cmd_p = cmd_head;
    uint8_t match_ok_flag = 0; // 0 match fail ,1 main match ok ,2 main and sub  match  ok
    while (cmd_p != NULL) {
        if (estrcmp(cmd_p->unit->title_main, maintitle) == 0) {
            match_ok_flag = 1;
            if (cmd_p->unit->need_su && !ezhdl.su_checked && ezhdl.su_psw != NULL) {
                /* query su password */
                ezcsl_reset_empty();
                ezcsl_printf(TIP_PSW_INPUT);
                ezhdl.psw_inputing = 1;
                return;
            }
            if (estrcmp(cmd_p->title_sub, subtitle) == 0) {
                match_ok_flag = 2;
                if (cmd_p->para_num == paranum) {
                    /* match ok ,ready to exec cmd */
                    for (uint8_t i = 0; i < paranum; i++) {
                        switch (cmd_p->para_desc[i]) {
                        case 's': {
                            para[i] = (void *)para[i];
                        } break;
                        case 'i': {
                            paraI[i] = (int)atoi((const char *)para[i]);
                            para[i] = (void *)&paraI[i];
                        } break;
                        case 'f': {
                            paraF[i] = (float)atof((const char *)para[i]);
                            para[i] = (void *)&paraF[i];
                        } break;
                        default:
                            break;
                        }
                    }

                    /**** execute start ****/
                    ezhdl.csl_occupied = 1;
                    cmd_p->unit->callback(cmd_p->id, para);
                    ezhdl.csl_occupied = 0;
                    /**** execute end ****/

                } else {
                    ezcsl_printf("\033[31mIncorrect format!\033[m %s,%s", maintitle, subtitle);
                    for (uint8_t i = 0; i < cmd_p->para_num; i++) {
                        ezcsl_printf(",<%s>", EXPAND_DESC(cmd_p->para_desc[i]));
                    }
                    ezcsl_printf(" : %s\r\n", cmd_p->describe);
                }
                break;
            }
        }
        cmd_p = cmd_p->next;
    }

    switch (match_ok_flag) {
    case 0:
        ezcsl_printf(COLOR_RED("Unknown Command") " %s\r\n", maintitle);
        break;
    case 1:
        cmd_p = cmd_head;
        ezcsl_printf(TIP_SUB_CMD_DESC_LIST "\r\n");
        ezcsl_printf(TIP_SPLIT_LINE "\r\n");
        while (cmd_p != NULL) {
            if (estrcmp(cmd_p->unit->title_main, maintitle) == 0) {
                ezcsl_printf("%s,%s:  %s\r\n", cmd_p->unit->title_main, cmd_p->title_sub, cmd_p->describe);
            }
            cmd_p = cmd_p->next;
        }
        ezcsl_printf(TIP_SPLIT_LINE "\r\n");
        break;
    default:
        break;
    }


    ezcsl_reset_prefix();
}


/**
 * @brief auto complete
 *
 */
static void ezcsl_tabcomplete(void)
{
    char existed_cmdbuf[CSL_BUF_LEN] = {0};
    uint8_t match_ok_cnt = 0;
    if (estrlen_s(ezhdl.buf, CSL_BUF_LEN) == 0) {
        return;
    }
    ez_cmd_t *p;
    p = cmd_head;
    while (p != NULL) {
        existed_cmdbuf[0] = 0;
        estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->unit->title_main);
        estrcatc_s(existed_cmdbuf, CSL_BUF_LEN, SPLIT_CHAR);
        estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->title_sub);
        if (estrncmp(ezhdl.buf, existed_cmdbuf, estrlen_s(ezhdl.buf, CSL_BUF_LEN)) == 0) {
            match_ok_cnt++;
        }
        p = p->next;
    }
    if (match_ok_cnt == 1) {
        p = cmd_head;
        while (p != NULL) {
            existed_cmdbuf[0] = 0;
            estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->unit->title_main);
            estrcatc_s(existed_cmdbuf, CSL_BUF_LEN, SPLIT_CHAR);
            estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->title_sub);
            if (estrncmp(ezhdl.buf, existed_cmdbuf, estrlen_s(ezhdl.buf, CSL_BUF_LEN)) == 0) {
                estrcpy_s(ezhdl.buf, CSL_BUF_LEN, existed_cmdbuf);
                ezhdl.bufp = ezhdl.bufl = estrlen_s(ezhdl.buf, CSL_BUF_LEN);
                ezcsl_printf(MOVE_CURSOR_ABS(0) "%s%s" ERASE_TO_END(), ezhdl.prefix, ezhdl.buf);
                break;
            }
            p = p->next;
        }
    } else if (match_ok_cnt > 1) {
        char autocomplete[CSL_BUF_LEN] = {0};
        ezport_send_str((char *)"\r\n", 2);
        p = cmd_head;
        while (p != NULL) {
            existed_cmdbuf[0] = 0;
            estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->unit->title_main);
            estrcatc_s(existed_cmdbuf, CSL_BUF_LEN, SPLIT_CHAR);
            estrcat_s(existed_cmdbuf, CSL_BUF_LEN, p->title_sub);
            if (estrncmp(ezhdl.buf, existed_cmdbuf, estrlen_s(ezhdl.buf, CSL_BUF_LEN)) == 0) {
                ezcsl_printf("%s\t", existed_cmdbuf);
                if (autocomplete[0] == 0) {
                    estrcpy_s(autocomplete, CSL_BUF_LEN, existed_cmdbuf);
                } else {
                    for (uint16_t i = 0; i < estrlen_s(autocomplete, CSL_BUF_LEN); i++) {
                        if (autocomplete[i] != existed_cmdbuf[i]) {
                            autocomplete[i] = 0;
                            break;
                        }
                    }
                }
            }
            p = p->next;
        }
        estrcpy_s(ezhdl.buf, CSL_BUF_LEN, autocomplete);
        ezhdl.bufp = ezhdl.bufl = estrlen_s(ezhdl.buf, CSL_BUF_LEN);
        ezport_send_str((char *)"\r\n", 2);
        ezport_send_str((char *)ezhdl.prefix, ezhdl.prefix_len);
        ezport_send_str(ezhdl.buf, ezhdl.bufl);
    }
}


/**
 * @brief create cmd unit
 * 
 * @param title_main 
 * @param describe 
 * @param need_su EZ_NSUDO or EZ_SUDO
 * @param callback 
 * @return ez_cmd_unit_t* 
 */
ez_cmd_unit_t *ezcsl_cmd_unit_create(const char *title_main, const char *describe, uint8_t need_su, void (*callback)(uint16_t, ez_param_t *))
{
    if (estrlen(title_main) == 0 || estrlen(title_main) >= 10 || callback == NULL) {
        return NULL;
    }

    ez_cmd_unit_t *p = cmd_unit_head;
    while (p != NULL) { // duplicate
        if (estrcmp(p->title_main, title_main) == 0) {
            return NULL;
        }
        p = p->next;
    }

    ez_cmd_unit_t *p_add = (ez_cmd_unit_t *)s_malloc(sizeof(ez_cmd_unit_t));
    p_add->describe = CHECK_NULL_STR(describe);
    p_add->next = NULL;
    p_add->title_main = title_main;
    p_add->callback = callback;
    p_add->need_su = need_su;

    if (cmd_unit_head == NULL) {
        cmd_unit_head = p_add;
    } else {
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
 * @param para_desc the description of parameters your cmd need,s->string,i->int,f->float
 * @return register result
 */
ez_sta_t ezcsl_cmd_register(ez_cmd_unit_t *unit, uint16_t id, const char *title_sub, const char *describe, const char *para_desc)
{
    if (estrlen(para_desc) > PARA_LEN_MAX) {
        return EZ_ERR;
    }
    ez_cmd_t *p = cmd_head;
    while (p != NULL) { // duplicate
        if (estrcmp(p->unit->title_main, unit->title_main) == 0 && (estrcmp(p->title_sub, title_sub) == 0 || p->id == id)) {
            return EZ_ERR;
        }
        p = p->next;
    }

    ez_cmd_t *p_add = (ez_cmd_t *)s_malloc(sizeof(ez_cmd_t));
    p_add->describe = CHECK_NULL_STR(describe);
    p_add->next = NULL;
    p_add->title_sub = CHECK_NULL_STR(title_sub);
    p_add->para_num = estrlen(para_desc);
    p_add->para_desc = para_desc;
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

/**
 * @brief break signal query
 * 
 * @return uint8_t 
 */
uint8_t ezcsl_break_signal(void){
    return !ezhdl.csl_occupied;
}


/**
 * @brief 
 * 
 */
void ezcsl_is_su(void){
    return ezhdl.su_checked;
}

/**
 * @brief help cmd callback
 *
 * @param id cmd id
 * @param para param
 */
static void ezcsl_cmd_help_callback(uint16_t id, ez_param_t *para)
{
    ez_cmd_unit_t *p = cmd_unit_head;
    ezcsl_printf(TIP_MAIN_CMD_DESC_LIST "\r\n");
    ezcsl_printf(TIP_SPLIT_LINE "\r\n");
    while (p != NULL) {
        ezcsl_printf("%-10s %s\r\n", p->title_main, p->describe);
        p = p->next;
    }
    ezcsl_printf(TIP_SPLIT_LINE "\r\n");
}


static uint8_t last_load_hist = 0; // history direction,its a ugly flag...
/**
 * move the buf to history
 * @param
 */
static void buf_to_history(void)
{
    for (uint16_t i = 0; i < HISTORY_BUF_LEN; i++) {
        uint16_t idx = HISTORY_BUF_LEN - 1 - i;
        if (idx > ezhdl.bufl) {
            ezhdl.hist_buf[idx] = ezhdl.hist_buf[idx - ezhdl.bufl - 1];
        } else if (idx < ezhdl.bufl) {
            ezhdl.hist_buf[idx] = ezhdl.buf[idx];
        } else {
            ezhdl.hist_buf[idx] = 0;
        }
    }
    ezhdl.cur_hist_idx = 0;
    last_load_hist = 0;
}


/**
 * move history to buf
 */
static uint8_t load_history(void)
{
    uint8_t hist_num = 0;
    char *hist_start = ezhdl.hist_buf;
    for (uint16_t i = 0; i < HISTORY_BUF_LEN; i++) {
        if (ezhdl.hist_buf[i] == 0) {
            if (hist_num == ezhdl.cur_hist_idx) {
                estrcpy_s(ezhdl.buf, CSL_BUF_LEN, hist_start);
                ezhdl.bufl = ezhdl.bufp = estrlen_s(ezhdl.buf, CSL_BUF_LEN);
                ezcsl_printf(MOVE_CURSOR_ABS(0) "%s%s" ERASE_TO_END(), ezhdl.prefix, ezhdl.buf);
                return 1;
            } else {
                if (ezhdl.hist_buf[i + 1] == 0) {
                    return 0;
                }
                hist_start = &ezhdl.hist_buf[i + 1];
                hist_num++;
            }
        }
    }
    return 0;
}


/**
 * @brief load last record
 *
 */
static void last_history_to_buf(void)
{
    if (last_load_hist == 2) {
        ezhdl.cur_hist_idx++;
    }
    if (load_history()) {
        last_load_hist = 1;
        ezhdl.cur_hist_idx++;
    }
}

/**
 * @brief load next record
 *
 */
static void next_history_to_buf(void)
{

    if (last_load_hist == 1 && ezhdl.cur_hist_idx > 0) {
        ezhdl.cur_hist_idx--;
    }
    if (ezhdl.cur_hist_idx > 0) {
        ezhdl.cur_hist_idx--;
        load_history();
        last_load_hist = 2;
    }
}


/************************** Ymodem ************************************/
#if USE_EZ_MODEM == EZ_YMODEM_1K

#define XYM_SOH 0x01
#define XYM_STX 0x02
#define XYM_EOT 0x04
#define XYM_ACK 0x06
#define XYM_NAK 0x15
#define XYM_CAN 0x18
#define XYM_C   0x43


/**
 * @brief modem init (optional),if you need modem,call this after `ezcsl_init`
 * 
 * @param modem_prefix 
 * @param cb_func callback from modem,
 *      @param char* buffer pointer
 *      @param uint16_t buffer length
 */
void ezcsl_modem_set(const char *modem_prefix, modem_rev_func_t (*cb_func)(modem_file_t *))
{
    ezhdl.modem_prefix = modem_prefix;
    ezhdl.modem_cb = cb_func;
}


/**
 * @brief modem crc16
 * 
 * @param data 
 * @param length 
 * @return uint16_t 
 */
static uint16_t crc16_modem(uint8_t *data, uint16_t length)
{
    uint16_t crc = 0;
    for (; length > 0; length--) {
        crc = crc ^ (*data++ << 8);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
        crc &= 0xFFFF;
    }
    return crc;
}


/**
 * @brief
 *
 * @param reply
 */
static void modem_reply(uint8_t reply)
{
    uint8_t sendbuf;
    ezport_delay(10);
    sendbuf = reply;
    ezport_send_str((char *)&sendbuf, 1);
}


/**
 * @brief
 *
 */
static ez_sta_t modem_start(void)
{
    uint16_t frame_size = 0;
    uint16_t timeout = 0;
    uint16_t bufp_last_time_out = 0; // timeout
    uint8_t last_packet_num = 0;
    uint8_t rev_file_info = 0;
    uint8_t eot_confirm = 0;

    /* set receive buffer */
    ezport_delay(3000);
    modem_reply(XYM_C);

    /* start receiving */
    ez_sta_t ret = EZ_ERR;
    modem_file_t file={
        .frame_type = FILE_TRANS_OVER,
        .content = NULL,
        .contentlen = 0,
        .filename = NULL,
        .filesize = 0,
        .filesize_received = 0,
    };
    while (1) {
        if (bufp_last_time_out == ezhdl.modem_p) {
            ezport_delay(1);
            timeout++;
            if (timeout > 3000) {
                ret = EZ_ERR;
                break;
            }
        } else {
            bufp_last_time_out = ezhdl.modem_p;

            if (ezhdl.modem_buf[0] == XYM_SOH) {
                timeout = 0;
                frame_size = 128; // 128+5
            } else if (ezhdl.modem_buf[0] == XYM_STX) {
                timeout = 0;
                frame_size = 1024; // 1024+5
            } else if (ezhdl.modem_p == 1 && ezhdl.modem_buf[0] == XYM_EOT) {
                timeout = 0;
                ezhdl.modem_p = 0;
                if (eot_confirm == 0) {
                    ezport_delay(1000);
                    modem_reply(XYM_NAK);
                    eot_confirm = 1;
                } else {
                    ezport_delay(1000);
                    modem_reply(XYM_ACK);
                    modem_reply(XYM_C);
                    file.frame_type = FILE_TRANS_OVER;
                    ezhdl.modem_cb(&file);
                    ret = EZ_OK;
                    break;
                }
            } else {
                ezport_delay(1);
                timeout++;
                if (timeout > 3000) {
                    ret = EZ_ERR;
                    break;
                }
            }
            if (ezhdl.modem_p == frame_size + 5 && frame_size != 0) {
                ezhdl.modem_p = 0;
                if (ezhdl.modem_buf[1] == (uint8_t)(last_packet_num) && ezhdl.modem_buf[1] == (uint8_t)(~ezhdl.modem_buf[2])) {
                    uint8_t *rev_without_head = ezhdl.modem_buf + 3;
                    if (crc16_modem(rev_without_head, frame_size) == ((uint16_t)(ezhdl.modem_buf[frame_size + 3] << 8) | ezhdl.modem_buf[frame_size + 4])) {
                        if(file.filename==NULL){
                            file.frame_type = FILE_INFO_ONLY;
                            file.filesize_received = 0;
                            file.filename = (const char*)rev_without_head;
                            file.filesize = (uint32_t)atoi((const char*)rev_without_head + estrlen_s((const char *)rev_without_head, frame_size) + 1);
                           }else{
                            file.frame_type = FILE_INFO_AND_CONTENT;
                            file.content = (char *)rev_without_head;
                            file.filesize_received += frame_size;
                            file.contentlen = file.filesize_received > file.filesize ? frame_size - (file.filesize_received - file.filesize) : frame_size;
                        }
                        switch (ezhdl.modem_cb(&file)) {
                        case M_SEND_NEXT:
                            last_packet_num++;
                            if (rev_file_info == 0) {
                                rev_file_info = 1;
                                modem_reply(XYM_ACK);
                                modem_reply(XYM_C);
                            } else {
                                modem_reply(XYM_ACK);
                            }
                            break;
                        case M_SEND_REPEAT:
                            modem_reply(XYM_NAK);
                            break;
                        case M_SEND_ABORT:
                        default:
                            modem_reply(XYM_CAN);
                            break;
                        };
                    } else {
                        /* repeat when crc wrong */
                        modem_reply(XYM_NAK);
                    }
                } else {
                    /* abort when frame goes wrong */
                    modem_reply(XYM_CAN);
                    ret = EZ_ERR;
                    break;
                }
            }
        }
    }
    ezport_delay(1000); //
    return ret;
}




#endif /* USE_EZ_MODEM */


/**
 * create a ringbuffer
 */
ezrb_t *ezrb_create(uint8_t len)
{
    if (len < 1) {
        return NULL;
    }
    ezrb_t *rb = (ezrb_t *)s_malloc(sizeof(ezrb_t));
    if (rb == NULL) {
        return NULL;
    }
    rb->head = 0;
    rb->tail = 0;
    rb->len = len;
    rb->buffer = (RB_DATA_T *)s_malloc(sizeof(RB_DATA_T) * len);
    if (rb->buffer == NULL) {
        ezrb_destroy(rb);
        return NULL;
    }
    for (uint16_t i = 0; i < len; i++) {
        rb->buffer[i] = 0;
    }
    return rb;
}

/**
 * push the data to buffer
 * @param  data :the data
 */
rb_sta_t ezrb_push(ezrb_t *rb, RB_DATA_T data)
{
    if (rb == NULL) {
        return RB_ERR;
    }
    // buffer is full?
    if (rb->tail + 1 == rb->head || (rb->tail + 1 == rb->len && rb->head == 0)) {
        return RB_FULL;
    }
    // write
    rb->buffer[rb->tail] = data;
    rb->tail = rb->tail + 1 == rb->len ? 0 : rb->tail + 1;
    return RB_OK;
}

/**
 * get the data from buffer
 * @param  rev :the data received
 */
rb_sta_t ezrb_pop(ezrb_t *rb, RB_DATA_T *rev)
{
    if (rb == NULL) {
        return RB_ERR;
    }
    // buffer is empty ?
    if (rb->head == rb->tail) {
        return RB_EMPTY;
    }
    // read
    RB_DATA_T data = rb->buffer[rb->head];
    rb->head = rb->head + 1 == rb->len ? 0 : rb->head + 1;
    *rev = data;
    return RB_OK;
}

void ezrb_destroy(ezrb_t *rb)
{
    if (rb->buffer != NULL) {
        s_free(rb->buffer);
        rb->buffer = NULL;
    }
    if (rb != NULL) {
        s_free(rb);
        rb = NULL;
    }
}


/* *********** ezstring ************* */


#define EZSTR_OVERFLOW(s, lmt) \
    if (++s >= lmt)            \
        return EZSTR_ERR;


ezstr_ret_t estrcat_s(char *_Dst, ezstr_size_t _DstSize, const char *_Src)
{
    ezstr_size_t s = 0;
    char *tmp = _Dst;
    while (*tmp != 0) {
        EZSTR_OVERFLOW(s, _DstSize);
        (void)*tmp++;
    }
    while (*_Src != 0) {
        EZSTR_OVERFLOW(s, _DstSize);
        *tmp++ = *_Src++;
    }
    *tmp = 0;
    return EZSTR_OK;
}
ezstr_ret_t estrcatc_s(char *_Dst, ezstr_size_t _DstSize, char _Src)
{
    ezstr_size_t s = 0;
    char *tmp = _Dst;
    while (*tmp != 0) {
        EZSTR_OVERFLOW(s, _DstSize);
        (void)*tmp++;
    }
    if (s + 1 >= _DstSize) {
        return EZSTR_ERR;
    }
    *tmp = _Src;
    *(tmp + 1) = 0;
    return EZSTR_OK;
}

ezstr_ret_t estrcpy_s(char *_Dst, ezstr_size_t _DstSize, const char *_Src)
{
    ezstr_size_t s = 0;
    while (*_Src != 0) {
        EZSTR_OVERFLOW(s, _DstSize);
        *_Dst++ = *_Src++;
    }
    *_Dst = 0;
    return EZSTR_OK;
}

ezstr_ret_t estrlen_s(const char *_Str, ezstr_size_t _Size)
{
    ezstr_size_t s = 0;
    while (*_Str != 0) {
        if (++s >= _Size)
            return _Size;
        _Str++;
    }
    return s;
}


ezstr_size_t estrlen(const char *_Str)
{
    ezstr_size_t s = 0;
    while (*_Str != 0) {
        s++;
        _Str++;
    }
    return s;
}


ezstr_ret_t estrcmp(const char *_Str1, const char *_Str2)
{
    while (*_Str1 == *_Str2) {
        if (*_Str1 == '\0') {
            return EZSTR_OK;
        }
        _Str1++;
        _Str2++;
    }
    return EZSTR_ERR;
}

ezstr_ret_t estrncmp(const char *_Str1, const char *_Str2, ezstr_size_t _Size)
{
    ezstr_size_t s = 0;
    while (*_Str1 == *_Str2) {
        if (++s >= _Size) {
            return EZSTR_OK;
        }
        _Str1++;
        _Str2++;
    }
    return EZSTR_ERR;
}


char *estrtokc(char *_Str, char _Deli)
{
    static char *tmp = NULL;
    if (_Str == NULL) {
        _Str = tmp;
    }
    char *search = _Str;
    while (*search != _Deli) {
        if (*search == 0) {
            return NULL;
        }
        search++;
    }
    *search = 0;
    tmp = search + 1;
    return _Str;
}