// #ifdef __APPLE__
// #include <OpenGL/gl3.h>
// #else
// #include <GL/glew.h>
// #endif

/* Code is used as an importable module, so apps must provide their own main */
#define SOKOL_NO_ENTRY

/* Select graphics API implementation */
#ifdef __EMSCRIPTEN__
#define SOKOL_GLES3
#else
#define SOKOL_GLCORE33
#endif

#ifndef __EMSCRIPTEN__
#define SOKOL_SHADER_HEADER SOKOL_SHADER_VERSION SOKOL_SHADER_PRECISION
#define SOKOL_SHADER_VERSION "#version 330\n"
#define SOKOL_SHADER_PRECISION "precision highp float;\n"
#else
#define SOKOL_SHADER_HEADER SOKOL_SHADER_VERSION SOKOL_SHADER_PRECISION
#define SOKOL_SHADER_VERSION  "#version 300 es\n"
#define SOKOL_SHADER_PRECISION "precision highp float;\n"
#endif

// #define SOKOL_DEBUG

// #ifdef NDEBUG
// #define SOKOL_ASSERT(c)
// #endif

// #define SOKOL_ASSERT(x) ((x)?(1):(sokol_assert_failed(__FILE__,__LINE__,#x),0))

// static inline void
// sokol_assert_failed(const char* file, int line, const char* expr) {
//     printf("%s:%i: SOKOL_ASSERT(%s) failed\n", file, line, expr);
//     exit(1);
// }

#include "..\..\..\..\sokol-nim\src\sokol\c\sokol_gfx.h"
#include "..\..\..\..\sokol-nim\src\sokol\c\sokol_app.h"
#include "..\..\..\..\sokol-nim\src\sokol\c\sokol_log.h"
#include "..\..\..\..\sokol-nim\src\sokol\c\sokol_glue.h"
