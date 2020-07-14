#ifndef _GENIESP32_GLOBALVARS_H_
#define _GENIESP32_GLOBALVARS_H_


typedef struct {
    enum {ON, OFF}  state;
    int16_t         ambiant_ligh;
} globals_t;

extern globals_t globals;


#endif /*_GENIESP32_GLOBALVARS_H_*/
