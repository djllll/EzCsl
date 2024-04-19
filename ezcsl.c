#include "ezcsl.h"
#include "stdlib.h"
#include "ezrb.h"
#include "ezstring.h"





#define KEY_IS_VISIBLE(c) ((c) >= 0x20 && (c) <= 0x7e)

#define EXPAND_DESC(c) ((c)=='s'?"string":((c)=='i'?"integer":((c)=='f'?"float":"unkown")))
#define DBGprintf ezcsl_send_printf

const char* strNULL="";
#define CHECK_NULL_STR(c) ((c)==NULL?strNULL:(c))

typedef struct CmdHistory{
    char history[CSL_BUF_LEN];
    struct CmdHistory *next;
}cmd_history_t;


static struct EzCslHandleStruct {
    ezuint8_t prefix_len;
    char buf[CSL_BUF_LEN];
    ezuint16_t bufp;
    ezuint16_t bufl;
    ezuint8_t historyp;
    ezrb_t *rb;
} ezhdl;

/* ez console port function */
void ezport_receive_a_char(char c);

void ezcsl_init(const char *prefix ,const char *welcome);
void ezcsl_deinit(void);
void ezcsl_tick(void);

static void ezcsl_tabcomplete(void);
static void ezcsl_submit(void);
static cmd_history_t *history_head=NULL;
static cmd_history_t *cur_history=NULL;
static void buf_to_history(void);
static void load_history(void);
static void last_history_to_buf(void);
static void next_history_to_buf(void);

static Ez_Cmd_t *cmd_head=NULL;
static Ez_CmdUnit_t *cmd_unit_head=NULL;
Ez_CmdUnit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,void (*callback)(ezuint16_t,ez_param_t*));
ez_sta_t ezcsl_cmd_register(Ez_CmdUnit_t *unit,ezuint16_t id,const char *title_sub,const char *describe,const char* para_desc);

/* ez inner cmd */
static void ezcsl_cmd_help_callback(ezuint16_t id,ez_param_t* para);

#define EZCSL_RST()                                   \
    do {                                              \
        ezport_send_str(ezhdl.buf, ezhdl.prefix_len); \
        ezhdl.buf[ezhdl.prefix_len] = 0;              \
        ezhdl.bufl = ezhdl.prefix_len;                \
        ezhdl.bufp = ezhdl.prefix_len;                \
    } while (0)



/**
 * @param  c the char from input
 * @author Jinlin Deng
 */
void ezport_receive_a_char(char c)
{
    ezrb_push(ezhdl.rb,(ezuint8_t)c);
    // DBGprintf("input :(%02x,%c)",c,c);
}



/**
 * init
 * @param
 * @author Jinlin Deng
 */
void ezcsl_init(const char *prefix,const char *welcome)
{
    ezhdl.prefix_len = estrlen_s(prefix,CSL_BUF_LEN);
    ezuint16_t i, j;
    for (i = 0; i < CSL_BUF_LEN; i++) {
        ezhdl.buf[i] = i < ezhdl.prefix_len ? prefix[i] : 0;
    }

    ezhdl.bufp = ezhdl.prefix_len;
    ezhdl.bufl = ezhdl.prefix_len;
    ezhdl.historyp = 0;
    ezhdl.rb = ezrb_create();
    Ez_CmdUnit_t *unit = ezcsl_cmd_unit_create("?","help",ezcsl_cmd_help_callback);
    ezcsl_cmd_register(unit,0,NULL,NULL,"");
    ezport_send_str((char*)welcome,estrlen(welcome)); 
    ezcsl_send_printf("you can input '?' for help\r\n");
    ezport_send_str(ezhdl.buf, ezhdl.prefix_len);
}


void ezcsl_deinit(void){
    Ez_Cmd_t *p1=cmd_head;
    while(p1!=NULL){
        Ez_Cmd_t *p_del=p1;
        p1=p1->next;
        free(p_del);
    }
    Ez_CmdUnit_t *p2=cmd_unit_head;
    while(p2!=NULL){
        Ez_CmdUnit_t *p_del=p2;
        p2=p2->next;
        free(p_del);
    }
    cmd_history_t *p3 =history_head;
    while(p3!=NULL){
        cmd_history_t *p_del=p3;
        p3=p3->next;
        free(p_del);
    }
    ezrb_destroy(ezhdl.rb);
}




#define DIR_KEY_DETECT(c, up, down, left, right) \
    do {                                         \
        if (c == left) {                         \
            if (ezhdl.bufp > ezhdl.prefix_len) { \
                ezcsl_send_printf("\033[1D");    \
                ezhdl.bufp--;                    \
            };                                   \
        } else if (c == right) {                 \
            if (ezhdl.bufp < ezhdl.bufl) {       \
                ezcsl_send_printf("\033[1C");    \
                ezhdl.bufp++;                    \
            }                                    \
        } else if (c == up) {                    \
            last_history_to_buf();               \
        } else if (c == down) {                  \
            next_history_to_buf();               \
        }                                        \
    } while (0)

#define DELETE_KEY_DETECT(c, delete)                               \
    do {                                                           \
        if (c == delete &&ezhdl.bufp < ezhdl.bufl) {               \
            ezcsl_send_printf("\033[s");                           \
            for (ezuint16_t i = ezhdl.bufp; i < ezhdl.bufl; i++) { \
                ezhdl.buf[i] = ezhdl.buf[i + 1];                   \
                ezport_send_str(ezhdl.buf + i, 1);                 \
            }                                                      \
            ezhdl.bufp;                                            \
            ezhdl.bufl--;                                          \
            ezcsl_send_printf("\033[K\033[u");                     \
        }                                                          \
    } while (0)
                                                     \

#define IS_POWERSHELL_PREFIX(c) (c==0x00)
#define IS_BASH_PREFIX(c)       (c==0x1b)
#define IS_BASH_1_PREFIX(c)     (c=='[')
#define IS_BASH_2_PREFIX(c)     (c=='3')

#define MATCH_MODE_DEFAULT      0
#define MATCH_MODE_POWERSHELL   1
#define MATCH_MODE_BASH         2
#define MATCH_MODE_BASH_1       3
#define MATCH_MODE_BASH_2       4

void ezcsl_tick(void) {
    static ezuint8_t match_mode = MATCH_MODE_DEFAULT; // direction keys
    ezuint8_t c;
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
            break;
        case MATCH_MODE_DEFAULT:
        default:
            if (KEY_IS_VISIBLE(c) && ezhdl.bufl < CSL_BUF_LEN - 1) {
                /* visible char */
                for (ezuint16_t i = ezhdl.bufl; i >= ezhdl.bufp + 1; i--) {
                    ezhdl.buf[i] = ezhdl.buf[i - 1];
                }
                ezhdl.buf[ezhdl.bufp] = c;
                ezcsl_send_printf("\033[s");
                ezport_send_str(ezhdl.buf + ezhdl.bufp, ezhdl.bufl - ezhdl.bufp + 1);
                ezcsl_send_printf("\033[u\033[1C");
                ezhdl.bufp++;
                ezhdl.bufl++;
            } else if (c == BACKSPACE_KV && ezhdl.bufp > ezhdl.prefix_len) { // cannot delete the prefix
                /* backspace */
                ezcsl_send_printf("\033[1D\033[s");
                for (ezuint16_t i = ezhdl.bufp - 1; i < ezhdl.bufl; i++) {
                    ezhdl.buf[i] = ezhdl.buf[i + 1];
                    ezport_send_str(ezhdl.buf + i, 1);
                }
                ezhdl.bufp--;
                ezhdl.bufl--;
                ezcsl_send_printf("\033[K\033[u");
            } else if (c == ENTER_KV) {
                /* enter */
                ezhdl.buf[ezhdl.bufl] = 0; // cmd end
                ezcsl_submit();
            } else if (c == CTRL_C_KV) {
                /* ctrl+c */
                ezcsl_send_printf("^C\r\n");
                EZCSL_RST();
            } else if (c == TAB_KV) {
                /* tab */
                ezhdl.buf[ezhdl.bufp] = 0; // cmd end
                ezcsl_tabcomplete();
            } else if (IS_POWERSHELL_PREFIX(c)) {
                match_mode = MATCH_MODE_POWERSHELL;
            } else if (c == 0x1b) {
                match_mode = MATCH_MODE_BASH;
            }
            break;
        }
        // DBGprintf("your input is %x\r\n",c);
    }
}


static void ezcsl_submit(void)
{
    ezuint8_t paranum=0;
    static float paraF[PARA_LEN_MAX];
    static int paraI[PARA_LEN_MAX];
    ez_param_t para[PARA_LEN_MAX];
    char *cmd=ezhdl.buf+ezhdl.prefix_len;
    const char *subtitle=strNULL;
    const char *maintitle=strNULL;
    
    char *a_split;
    ezuint8_t split_cnt=0;

    buf_to_history();
    cur_history=NULL;

    ezhdl.buf[ezhdl.bufl]=SPLIT_CHAR; // add a SPLIT_CHR to the end for estrtokc 
    // ezhdl.buf[ezhdl.bufl+1]=0; // add a SPLIT_CHR to the end for estrtokc 
    while (1) {
        a_split = estrtokc((char *)cmd, SPLIT_CHAR);
        if (a_split != NULL) {
            switch (split_cnt) {
            case 0:
                maintitle=a_split;
                break;
            case 1:
                subtitle=a_split;
                break;
            default:
                if(paranum<PARA_LEN_MAX && estrlen(a_split)>0){
                    para[paranum]=(ez_param_t*)a_split;
                    paranum++;
                }
                break;
            }
        } else {
            break;
        }
        split_cnt++;
        cmd=NULL; //for estrtokc continue
    };
    

    ezcsl_send_printf("\r\n");
    // Cmd Match 
    Ez_Cmd_t *cmd_p = cmd_head;
    ezuint8_t match_ok_flag=0;  //0 match fail ,1 main match ok ,2 main and sub  match  ok 
    while (cmd_p!= NULL) {
        if (estrcmp(cmd_p->unit->title_main, maintitle) == 0) {
            match_ok_flag = 1;
            if (estrcmp(cmd_p->title_sub, subtitle) == 0) {
                match_ok_flag = 2;
                if (cmd_p->para_num == paranum) {
                    for (ezuint8_t i = 0; i < paranum; i++) {
                        switch (cmd_p->para_desc[i]) {
                        case 's': {
                            para[i] = (void *)para[i];
                        } break;
                        case 'i': {
                            paraI[i] = (int)atoi((const char *)para[i]);
                            para[i] = (void*)&paraI[i];
                        } break;
                        case 'f': {
                            paraF[i] = (float)atof((const char *)para[i]);
                            para[i] = (void*)&paraF[i];
                        } break;
                        default:
                            break;
                        }
                    }
                    cmd_p->unit->callback(cmd_p->id,para); // user can use `ezcsl_send_printf` in callback
                } else {
                    ezcsl_send_printf("\033[31mCmd Error!\033[m %s,%s", maintitle, subtitle);
                    for(ezuint8_t i=0;i<cmd_p->para_num;i++){
                    ezcsl_send_printf(",<%s>",EXPAND_DESC(cmd_p->para_desc[i]));
                    }
                    ezcsl_send_printf(" : %s\r\n",cmd_p->describe);
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
            if (estrcmp(cmd_p->unit->title_main, maintitle) == 0) {
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

static void ezcsl_tabcomplete(void)
{
    char existed_cmdbuf[CSL_BUF_LEN] = {0};
    ezuint8_t match_ok_cnt = 0;
    if(estrlen_s(ezhdl.buf + ezhdl.prefix_len,CSL_BUF_LEN)==0){
        return;
    }
    Ez_Cmd_t *p;
    p = cmd_head;
    while (p != NULL) {
        existed_cmdbuf[0] = 0;
        estrcat_s(existed_cmdbuf,CSL_BUF_LEN,p->unit->title_main);
        estrcatc_s(existed_cmdbuf,CSL_BUF_LEN, SPLIT_CHAR);
        estrcat_s(existed_cmdbuf,CSL_BUF_LEN, p->title_sub);
        if (estrncmp(ezhdl.buf + ezhdl.prefix_len, existed_cmdbuf, estrlen_s(ezhdl.buf + ezhdl.prefix_len,CSL_BUF_LEN)) == 0) {
            match_ok_cnt++;
        }
        p = p->next;
    }
    if (match_ok_cnt == 1) {
        p = cmd_head;
        while (p != NULL) {
            existed_cmdbuf[0] = 0;
            estrcat_s(existed_cmdbuf,CSL_BUF_LEN, p->unit->title_main);
            estrcatc_s(existed_cmdbuf,CSL_BUF_LEN, SPLIT_CHAR);
            estrcat_s(existed_cmdbuf,CSL_BUF_LEN, p->title_sub);
            if (estrncmp(ezhdl.buf + ezhdl.prefix_len, existed_cmdbuf, estrlen_s(ezhdl.buf + ezhdl.prefix_len,CSL_BUF_LEN)) == 0) {
                estrcpy_s(ezhdl.buf + ezhdl.prefix_len,CSL_BUF_LEN-ezhdl.prefix_len, existed_cmdbuf);
                ezhdl.bufp = ezhdl.bufl = estrlen_s(ezhdl.buf,CSL_BUF_LEN);
                ezcsl_send_printf("\033[0G%s\033[K", ezhdl.buf);
                break;
            }
            p = p->next;
        }
    } else if (match_ok_cnt > 1) {
        char autocomplete[CSL_BUF_LEN]={0};
        ezcsl_send_printf("\r\n");
        p = cmd_head;
        while (p != NULL) {
            existed_cmdbuf[0] = 0;
            estrcat_s(existed_cmdbuf,CSL_BUF_LEN, p->unit->title_main);
            estrcatc_s(existed_cmdbuf,CSL_BUF_LEN, SPLIT_CHAR);
            estrcat_s(existed_cmdbuf,CSL_BUF_LEN, p->title_sub);
            if (estrncmp(ezhdl.buf + ezhdl.prefix_len, existed_cmdbuf, estrlen_s(ezhdl.buf + ezhdl.prefix_len,CSL_BUF_LEN)) == 0) {
                ezcsl_send_printf("%s\t", existed_cmdbuf);
                if (autocomplete[0] == 0) {
                    estrcpy_s(autocomplete,CSL_BUF_LEN,existed_cmdbuf);
                }else{
                    for(ezuint16_t i=0;i<estrlen_s(autocomplete,CSL_BUF_LEN);i++){
                        if(autocomplete[i]!=existed_cmdbuf[i]){
                            autocomplete[i]=0;
                            break;
                        }
                    }
                }
            }
            p = p->next;
        }
        estrcpy_s(ezhdl.buf+ezhdl.prefix_len,CSL_BUF_LEN-ezhdl.prefix_len,autocomplete);
        ezhdl.bufp=ezhdl.bufl=estrlen_s(ezhdl.buf,CSL_BUF_LEN);
        ezcsl_send_printf("\r\n");
        ezport_send_str(ezhdl.buf, ezhdl.bufl);
    }
}


/**
 * create a cmd unit
 * @param title_main main title ,cannot null or '' ,length < 10
 * @author Jinlin Deng
 */
Ez_CmdUnit_t *ezcsl_cmd_unit_create(const char *title_main,const char *describe ,void (*callback)(ezuint16_t,ez_param_t* )){
    if (estrlen(title_main)==0 || estrlen(title_main)>=10 || callback==NULL){
        return NULL;
    }
    
        Ez_CmdUnit_t *p = cmd_unit_head;
        while (p != NULL) { //duplicate
            if(estrcmp(p->title_main,title_main)==0){
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
 * @param para_desc the description of parameters your cmd need,s->string,i->int,f->float
 * @author Jinlin Deng
 * @return register result
 */
ez_sta_t ezcsl_cmd_register(Ez_CmdUnit_t *unit, ezuint16_t id, const char *title_sub, const char *describe, const char *para_desc)
{
    if (estrlen(para_desc) > PARA_LEN_MAX) {
        return EZ_ERR;
    }
    Ez_Cmd_t *p = cmd_head;
    while (p != NULL) { // duplicate
        if (estrcmp(p->unit->title_main, unit->title_main) == 0 && (estrcmp(p->title_sub, title_sub) == 0 || p->id == id)) {
            return EZ_ERR;
        }
        p = p->next;
    }

    Ez_Cmd_t *p_add = (Ez_Cmd_t *)malloc(sizeof(Ez_Cmd_t));
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


static void ezcsl_cmd_help_callback(ezuint16_t id,ez_param_t* para)
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

/**
 * move the buf to history
 * @param  
 * @author Jinlin Deng
 */
static void buf_to_history(void)
{
    static ezuint8_t cur_history_len = 0;
    if (HISTORY_LEN <= 0) {
        return;
    }
    if (history_head == NULL) {
        cmd_history_t *p_add = (cmd_history_t *)malloc(sizeof(cmd_history_t));
        estrcpy_s(p_add->history,CSL_BUF_LEN, ezhdl.buf);
        p_add->next=NULL;
        history_head = p_add;
        cur_history_len++;
    } else {
        if (cur_history_len < HISTORY_LEN) { // first insert
            cmd_history_t *p_add = (cmd_history_t *)malloc(sizeof(cmd_history_t));
            estrcpy_s(p_add->history,CSL_BUF_LEN, ezhdl.buf);
            p_add->next = history_head;
            history_head = p_add;
            cur_history_len++;
        } else { // move last to first
            cmd_history_t *p_last = history_head;
            cmd_history_t *p_last_parent = history_head;
            while (p_last != NULL) {
                if (p_last->next == NULL) {
                    break;
                }
                p_last_parent = p_last;
                p_last = p_last->next;
            }
            p_last_parent->next = NULL;
            estrcpy_s(p_last->history,CSL_BUF_LEN, ezhdl.buf);
            p_last->next = history_head;
            history_head = p_last;
        }
    }
}

/**
 * move history to buf  
 * @author Jinlin Deng
 */
static void load_history(void){
    estrcpy_s(ezhdl.buf,CSL_BUF_LEN,cur_history->history);
    ezhdl.bufl=ezhdl.bufp=estrlen_s(ezhdl.buf,CSL_BUF_LEN);
    ezcsl_send_printf("\033[0G%s\033[K",ezhdl.buf);
}
static void last_history_to_buf(void){
    if(cur_history==NULL){
        if(history_head!=NULL){
            cur_history=history_head;
            load_history();
        }
    }else{
        if(cur_history->next!=NULL){
            cur_history=cur_history->next;
            load_history();
        }
    }
}
static void next_history_to_buf(void){
    if(cur_history==NULL){
        return;
    }else{
        cmd_history_t *p=history_head;
        while(p!=NULL){
            if(p->next==cur_history){
                cur_history=p;
                load_history();
                break;
            }
            p=p->next;
        }
    }
}