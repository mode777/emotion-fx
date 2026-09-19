#include "player/player.h"
#include "runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_eval(int argc, char **argv) {
    efx_runtime *rt = efx_runtime_new(argv + 2, argc - 2);
    if (!rt) {
        return 2;
    }
    int rc = efx_runtime_eval_file(rt, argv[1]);
    fprintf(stderr, "harness: eval=%d quit=%d code=%d error=%d\n", rc,
            efx_runtime_quit_requested(rt), efx_runtime_quit_code(rt),
            efx_runtime_in_error(rt));
    efx_runtime_destroy(rt);
    return 0;
}

static int run_frames(int argc, char **argv) {
    int frames = atoi(argv[2]);
    efx_runtime *rt = efx_runtime_new(argv + 4, argc - 4);
    if (!rt) {
        return 2;
    }
    int rc = efx_runtime_eval_file(rt, argv[3]);
    if (rc == 0 && !efx_runtime_quit_requested(rt) && !efx_runtime_in_error(rt)) {
        int has_update = 0;
        int has_render = 0;
        efx_runtime_pick_hooks(rt, &has_update, &has_render);
        fprintf(stderr, "harness: hooks update=%d render=%d\n", has_update, has_render);
        for (int i = 0; i < frames; i++) {
            if (efx_player_frame(rt)) {
                break;
            }
        }
    }
    fprintf(stderr, "harness: frames=%d quit=%d code=%d error=%d\n", frames,
            efx_runtime_quit_requested(rt), efx_runtime_quit_code(rt),
            efx_runtime_in_error(rt));
    efx_runtime_destroy(rt);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: efx_dev_harness <file> [args...]\n"
                        "       efx_dev_harness --frames <n> <file> [args...]\n");
        return 2;
    }
    if (argc >= 4 && strcmp(argv[1], "--frames") == 0) {
        return run_frames(argc, argv);
    }
    return run_eval(argc, argv);
}
