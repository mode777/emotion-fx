#ifndef EFX_RUNTIME_INTERNAL_H
#define EFX_RUNTIME_INTERNAL_H

#include "quickjs.h"

struct efx_host_state {
    int quit_requested;
    int quit_code;
    JSValue quit_sentinel;
    char **args;
    int arg_count;
};

#endif
