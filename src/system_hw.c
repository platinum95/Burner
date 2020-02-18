#include "system_hw.h"
#include <stdlib.h>
#include <stdio.h>

int get_info( int some_param, system_hw_context * sys_ctx ){
    sys_ctx = ( system_hw_context*) malloc( sizeof( system_hw_context ) );
    printf( "Func called %i\n", some_param );
    sys_ctx->test = 0;
    return 1;
}