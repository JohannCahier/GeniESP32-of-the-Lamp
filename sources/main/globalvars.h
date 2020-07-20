#ifndef _GENIESP32_GLOBALVARS_H_
#define _GENIESP32_GLOBALVARS_H_

// for httpd_handle_t
#include <esp_http_server.h>


typedef struct {
    enum {ON, OFF}  state;
    int16_t         ambient_light;
    httpd_handle_t  http_server;
} globals_t;

extern globals_t globals;


#endif /*_GENIESP32_GLOBALVARS_H_*/
