#ifndef _EZRB_H_
#define _EZRB_H_

#include "stdint.h"

#ifdef __cplusplus 
extern "C" { 
#endif 

#define RB_BUF_LEN 32
#define MOD_BUFLEN(x) ((x)&31)
#define RB_DATA_T unsigned char

typedef enum{
    RB_OK,
    RB_ERR,
    RB_FULL,
    RB_EMPTY
}rb_sta_t;

typedef struct {  
    RB_DATA_T buffer[RB_BUF_LEN];  
    int head;    
    int tail;    
} ezrb_t;  

extern ezrb_t *ezrb_create(void);
extern rb_sta_t ezrb_push(ezrb_t *cb,RB_DATA_T dat);
extern rb_sta_t ezrb_pop(ezrb_t *cb,RB_DATA_T *dat);
extern void ezrb_destroy(ezrb_t *cb);

#ifdef __cplusplus 
}
#endif 

#endif
