// glad loader shim for Android.
//
// Desktop Cubyz resolves every GL entry point at runtime through glad
// (gladLoadGL fills a table of function pointers). Android links them straight
// out of libGLESv3.so, so there is nothing to load and the loader must simply
// report success. See src/android/shim/glad/gl.h for the matching declarations.
#include <glad/gl.h>

int gladLoadGL(GLADloadfunc load) {
	(void)load;
	return 1;
}

int gladLoadGLUserPtr(GLADuserptrloadfunc load, void* userptr) {
	(void)load;
	(void)userptr;
	return 1;
}
