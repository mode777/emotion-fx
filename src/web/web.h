#ifndef EFX_WEB_H
#define EFX_WEB_H

/*
 * Native-JS web runtime (f2b): on Emscripten the page's own JS engine is
 * the script runtime — no quickjs ships in the wasm (ADR 0022). bridge.c
 * exposes the `efx` C surface to host-engine JS; entry.js (post-js glue)
 * loads main.js, wires hooks and mirrors the desktop error/exit contract.
 */

int efx_web_main(int argc, char *const *argv);
const char *efx_web_root(void); /* resource root chosen for this run */
void efx_web_start_loop(void);  /* called by entry.js after main.js loaded */
void efx_web_set_golden_mode(void); /* golden capture build: query-driven root */


#endif
