#include "ezcsl_port.h"

/* your include begin */
#include "main.h"
/* your include end */

/**
 * use this to send
 * @param str str need to send
 * @param len the length of the str
 */
void ezport_send_str(char *str, uint16_t len)
{
    /** Write your code here ↓↓↓↓ */
    HAL_UART_Transmit(&hlpuart1,(const uint8_t*)str,len,1000);
    /** Write your code here ↑↑↑↑ */
}



/**
 * @brief write your delay here
 * 
 * @param ms 
 */
void ezport_delay(uint16_t ms)
{
    /** Write your code here ↓↓↓↓ */
    HAL_Delay(ms);
    /** Write your code here ↑↑↑↑ */
}


/**
 * @brief custom init (Optional)
 * 
 */
void ezport_custom_init(void)
{

}


/**
 * @brief custom deinit (Optional)
 * 
 */
void ezport_custom_deinit(void)
{
    
}



/**
 * @brief mutex lock (Necessary in RTOS)
 * 
 */
void ezport_rtos_mutex_lock(void)
{
    /** Write your code here ↓↓↓↓ */
    
    /** Write your code here ↑↑↑↑ */
}


/**
 * @brief mutex unlock (Necessary in RTOS)
 * 
 */
void ezport_rtos_mutex_unlock(void)
{
    /** Write your code here ↓↓↓↓ */
 
    /** Write your code here ↑↑↑↑ */
}
