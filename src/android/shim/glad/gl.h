// glad-compatible GL header backed by OpenGL ES 3.2, for the Android build.
//
// Desktop Cubyz uses glad's function-pointer dispatch (every glFoo is a macro
// for glad_glFoo, filled in by gladLoadGL at runtime). On Android that is
// unnecessary: every entry point the game uses — other than the seven shimmed
// in gles_compat.h — is exported by libGLESv3.so and resolves at link time.
//
// So this header keeps glad's *surface* (so the engine compiles unchanged:
// GLADloadfunc, gladLoadGL, and the GL_* names) but binds directly to GLES.
#ifndef CUBYZ_GLAD_GL_SHIM_H
#define CUBYZ_GLAD_GL_SHIM_H

#include <GLES3/gl32.h>
#include "../../gles_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

// glad dispatch typedefs, kept for API compatibility.
typedef void (*GLADapiproc)(void);
typedef GLADapiproc (*GLADloadfunc)(const char* name);
typedef GLADapiproc (*GLADuserptrloadfunc)(void* userptr, const char* name);

// GLES entry points are linked statically, so loading is a no-op that always
// succeeds. The loader argument is ignored.
int gladLoadGL(GLADloadfunc load);
int gladLoadGLUserPtr(GLADuserptrloadfunc load, void* userptr);

#ifdef __cplusplus
}
#endif

#endif
