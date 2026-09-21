#ifndef EFX_BACKEND_H
#define EFX_BACKEND_H

/* sokol backend selection, shared by every TU that includes sokol */

#if defined(_WIN32)
#define SOKOL_D3D11
#elif defined(__APPLE__)
#define SOKOL_METAL
#elif defined(__EMSCRIPTEN__)
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE
#endif

#endif
