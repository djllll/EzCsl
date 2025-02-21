#include "ezcsl.h"
#include "tcpserver.h"
#include <conio.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#if (!TCP_MODE && !CMD_MODE)
#error Please Choose a mode (CMD_MODE/TCP_MODE) !!
#endif


#define WELCOME \
    "\033[36m    ______      ______                       __\r\n\
   / ____/___  / ____/___  ____  _________  / /__\r\n\
  / __/ /_  / / /   / __ \\/ __ \\/ ___/ __ \\/ / _ \\\r\n\
 / /___  / /_/ /___/ /_/ / / / (__  ) /_/ / /  __/\r\n\
/_____/ /___/\\____/\\____/_/ /_/____/\\____/_/\\___/\033[m\r\n"


/* Command ID */
#define TEST_ADD2_ID 0
#define TEST_ADD3_ID 1

#define ECHO_NONE_ID 0
#define ECHO_ONE_ID  1
#define ECHO_MUL_ID  2
#define ECHO_TIME_ID 3
#define ECHO_PROGRESS_ID 4

#define INFO_VERSION_ID 0

/**
 * @brief test command callback
 * 
 * @param id 
 * @param para 
 */
void test_cmd_callback(uint16_t id, ez_param_t *para)
{
    switch (id) {
    case TEST_ADD2_ID:
        EZ_PRTL("result is %d", EZ_PtoI(para[0]) + EZ_PtoI(para[1]));
        break;
    case TEST_ADD3_ID:
        EZ_PRTL("result is %d", EZ_PtoI(para[0]) + EZ_PtoI(para[1]) + EZ_PtoI(para[2]));
        break;
    default:
        break;
    }
}


/**
 * @brief echo command callback
 *
 * @param id
 * @param para
 */
void echo_cmd_callback(uint16_t id, ez_param_t *para)
{
    switch (id) {
    case ECHO_NONE_ID:
        EZ_PRTL("your input is none ");
        break;
    case ECHO_ONE_ID:
        EZ_PRTL("your input :%d", EZ_PtoI(para[0]));
        break;
    case ECHO_MUL_ID:
        EZ_PRTL("your input :%s f:%f i:%d", EZ_PtoS(para[0]), EZ_PtoF(para[1]), EZ_PtoI(para[2]));
        break;
    case ECHO_TIME_ID: {
        while (!ezcsl_break_signal()) {
            ezport_delay(1000);
            time_t now_time;
            time(&now_time);
            EZ_PRTL("=> %s", ctime(&now_time));
        }
    } break;
    case ECHO_PROGRESS_ID:
        {
            uint16_t prg= 0;
            while (!ezcsl_break_signal() && prg<100) {
                prg++;
                ezport_delay(100);
                EZ_PRT_PROGRESS("Progressing Echo: ",prg);
            }
        }
        break;
    default:
        break;
    }
}


/**
 * @brief info command callback
 * 
 * @param id 
 * @param para 
 */
void info_cmd_callback(uint16_t id, ez_param_t *para)
{
    switch (id) {
    case INFO_VERSION_ID:
        EZ_PRTL("Version:"EZCSL_VERSION);
        break;
    default:
        break;
    }
}

/**
 * @brief modem receving callback
 * 
 * @param rev 
 * @param len 
 * @return modem_rev_func_t 
 */
static modem_rev_func_t modem_rev_cb(modem_file_t *file)
{
    static FILE *f;
    switch (file->frame_type) {
    case FILE_INFO_ONLY:
        f = fopen(file->filename, "w");
        fwrite("", 0, 1, f);
        fclose(f);
        f = fopen(file->filename, "ab");
        break;
    case FILE_INFO_AND_CONTENT:
        fwrite(file->content, file->contentlen, 1, f);
        break;
    case FILE_TRANS_OVER:
        fclose(f);
        break;
    default:
        break;
    }
    return M_SEND_NEXT;
}


/**
 * @brief main
 * 
 * @return int 
 */
int main(void)
{
#ifdef TCP_MODE
    tcp_server_open();
    tcp_server_start_receving_thread();
#endif

    /* EzCsl init */
    ezcsl_init(COLOR_CYAN("TEST:"), WELCOME, "123456");

    // EZ_LOGI("EzCsl","init ok");

    ezcsl_modem_set("rb", modem_rev_cb);

    /* add cmd */
    ez_cmd_unit_t *test_unit = ezcsl_cmd_unit_create("test", "add test callback", EZ_NSUDO, test_cmd_callback);
    ezcsl_cmd_register(test_unit, TEST_ADD2_ID, "add2", "add,a,b", EZ_PARAM_INT EZ_PARAM_INT);
    ezcsl_cmd_register(test_unit, TEST_ADD3_ID, "add3", "add,a,b,c", EZ_PARAM_INT EZ_PARAM_INT EZ_PARAM_INT);

    ez_cmd_unit_t *echo_unit = ezcsl_cmd_unit_create("echo", "echo your input", EZ_NSUDO, echo_cmd_callback);
    ezcsl_cmd_register(echo_unit, ECHO_NONE_ID, "none", "input ", EZ_PARAM_NONE);
    ezcsl_cmd_register(echo_unit, ECHO_ONE_ID, "one", "input 'int'", EZ_PARAM_INT);
    ezcsl_cmd_register(echo_unit, ECHO_MUL_ID, "mul", "input 'str,float,int'", EZ_PARAM_STR EZ_PARAM_FLOAT EZ_PARAM_INT);
    ezcsl_cmd_register(echo_unit, ECHO_TIME_ID, "time", "time echo", "");
    ezcsl_cmd_register(echo_unit, ECHO_PROGRESS_ID, "prg", "prg echo", "");

    ez_cmd_unit_t *info_unit = ezcsl_cmd_unit_create("info", "software info", EZ_SUDO, info_cmd_callback);
    ezcsl_cmd_register(info_unit, INFO_VERSION_ID, "version", "", EZ_PARAM_NONE);

/* input */
#ifdef CMD_MODE
    do {
        ezport_receive_a_char(getch());
    } while (!ezcsl_tick()); // quit
#else
    while (!ezcsl_tick())
        ;
    tcp_server_close();
#endif


    /* deinit */
    ezcsl_deinit();

    printf("\r\nEzCsl: Bye.\r\n");
    return 0;
}
