#include "web/web.h"

#if defined(EFX_WEB_GOLDEN)
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    efx_web_set_golden_mode();
    return efx_web_main(0, 0);
}
#else
int main(int argc, char **argv) {
    return efx_web_main(argc, argv);
}
#endif
