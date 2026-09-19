#include "player/player.h"
#include "runtime/runtime.h"
#include "platform/platform.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int usage(void) {
    fprintf(stderr,
            "usage:\n"
            "  player <resource-root>            run a resource folder\n"
            "  player --script <file> [args...]  run a single script headless\n");
    return 1;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

static int is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

static int run_script_mode(const char *path, char *const *args, int arg_count) {
    efx_runtime *rt = efx_runtime_new(args, arg_count);
    if (!rt) {
        return 1;
    }
    int rc = efx_runtime_eval_file(rt, path);
    int exit_code;
    if (rc == -1) {
        exit_code = 1;
    } else if (efx_runtime_in_error(rt)) {
        exit_code = 1;
    } else if (efx_runtime_quit_requested(rt)) {
        exit_code = efx_runtime_quit_code(rt);
    } else {
        exit_code = 0;
    }
    efx_runtime_destroy(rt);
    return exit_code;
}

int efx_player_frame(void *ud) {
    efx_runtime *rt = (efx_runtime *)ud;
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return 1;
    }
    int r = efx_runtime_call_hook(rt, 1);
    if (r != EFX_HOOK_OK) {
        return 1;
    }
    if (efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt)) {
        return 1;
    }
    r = efx_runtime_call_hook(rt, 0);
    if (r != EFX_HOOK_OK) {
        return 1;
    }
    return efx_runtime_quit_requested(rt) || efx_runtime_in_error(rt);
}

static int on_frame(void *ud) {
    return efx_player_frame(ud);
}

static int run_root_mode(const char *root) {
    if (!is_dir(root)) {
        fprintf(stderr, "player: resource root is not a directory: %s\n", root);
        return 1;
    }
    char entry[4096];
    int n = snprintf(entry, sizeof(entry), "%s/main.js", root);
    if (n < 0 || n >= (int)sizeof(entry)) {
        fprintf(stderr, "player: resource root path too long: %s\n", root);
        return 1;
    }
    if (!is_file(entry)) {
        fprintf(stderr, "player: no main.js in resource root: %s\n", root);
        return 1;
    }
    efx_runtime *rt = efx_runtime_new(NULL, 0);
    if (!rt) {
        return 1;
    }
    int rc = efx_runtime_eval_file(rt, entry);
    if (rc == -1) {
        efx_runtime_destroy(rt);
        return 1;
    }
    if (efx_runtime_in_error(rt)) {
        efx_runtime_destroy(rt);
        return 1;
    }
    if (efx_runtime_quit_requested(rt)) {
        int exit_code = efx_runtime_quit_code(rt);
        efx_runtime_destroy(rt);
        return exit_code;
    }
    int has_update = 0;
    int has_render = 0;
    efx_runtime_pick_hooks(rt, &has_update, &has_render);
    efx_frame_hooks hooks;
    hooks.ud = rt;
    hooks.on_frame = on_frame;
    efx_platform_run(hooks);
    int exit_code;
    if (efx_runtime_in_error(rt)) {
        exit_code = 1;
    } else if (efx_runtime_quit_requested(rt)) {
        exit_code = efx_runtime_quit_code(rt);
    } else {
        exit_code = 0;
    }
    efx_runtime_destroy(rt);
    return exit_code;
}

int efx_player_main(int argc, char **argv) {
    if (argc < 2) {
        return usage();
    }
    if (strcmp(argv[1], "--script") == 0 || strcmp(argv[1], "-s") == 0) {
        if (argc < 3) {
            fprintf(stderr, "player: --script requires a file argument\n");
            return usage();
        }
        return run_script_mode(argv[2], argv + 3, argc - 3);
    }
    if (argv[1][0] == '-') {
        fprintf(stderr, "player: unknown option: %s\n", argv[1]);
        return usage();
    }
    return run_root_mode(argv[1]);
}
