// GLFW-compatible shim implemented on EGL + Android. Implements the subset of
// GLFW declared in shim/GLFW/glfw3.h so the Zig game code compiles unchanged.
#include <GLFW/glfw3.h>

#include <EGL/egl.h>
#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "cubyz_android.h"

#define LOG_TAG "CctesShim"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define CLIPBOARD_CAP 4096

// ---------------------------------------------------------------- window state

struct GLFWwindow {
	int width;
	int height;
	int shouldClose;
	int cursorMode;
	int swapInterval;

	GLFWerrorfun errorCb;
	GLFWkeyfun keyCb;
	GLFWcharfun charCb;
	GLFWframebuffersizefun framebufferSizeCb;
	GLFWcursorposfun cursorPosCb;
	GLFWmousebuttonfun mouseButtonCb;
	GLFWscrollfun scrollCb;
};

// ---------------------------------------------------------------- global state

static EGLDisplay g_display = EGL_NO_DISPLAY;
static EGLConfig g_config = NULL;
static EGLContext g_context = EGL_NO_CONTEXT;
static EGLSurface g_surface = EGL_NO_SURFACE;
static struct android_app* g_app = NULL;

static GLFWwindow* g_window = NULL;
static int g_initialized = 0;

static GLFWerrorfun g_errorCb = NULL;

static char g_clipboard[CLIPBOARD_CAP] = { 0 };

// ----------------------------------------------------------------- touch state
//
// Fingers are interpreted as a virtual gamepad: the left half of the screen is
// a floating movement stick (fed into the LEFT_X/LEFT_Y axes), the right half is
// drag-to-look (RIGHT_X/RIGHT_Y), and on-screen buttons map to the face buttons
// and triggers that main.zig already binds. Because the engine polls a gamepad
// rather than raw touch, every existing binding — move, jump, break, place,
// inventory, hotbar — works with no Zig-side changes.

typedef enum {
	TOUCH_NONE = -1,
	TOUCH_MOVE = 0,
	TOUCH_LOOK,
	TOUCH_BUTTON,
} TouchRole;

typedef struct {
	int active;
	TouchRole role;
	int pointerId;
	int button;         // CUBYZ_TOUCH_BTN_* when role == TOUCH_BUTTON
	float startX, startY;
	float curX, curY;
} TouchPointer;

#define MAX_TOUCH_POINTERS 10
static TouchPointer g_touches[MAX_TOUCH_POINTERS];

// Virtual gamepad state, in GLFW's -1..1 axis convention.
static float g_padAxes[6];
static unsigned char g_padButtons[15];

// The movement stick follows the finger: where it first touched becomes the
// origin, and the offset from there is the axis value.
static float g_moveOriginX = 0, g_moveOriginY = 0;
static int g_moveActive = 0;
static float g_moveOffX = 0, g_moveOffY = 0;

// Look-drag: accumulated movement since the last poll, in pixels. Applied to the
// mouse-cursor path each frame (see touch_pump_look()).
static float g_lookDX = 0, g_lookDY = 0;

// The last cursor position handed to the engine. Window.zig diffs consecutive
// positions, so this has to track exactly what it saw — both the menu's direct
// touch positions and the in-game look-drag flow through here. Keeping one
// source of truth is what stops the camera from jumping when returning to the
// game after a menu.
static double g_cursorX = 0, g_cursorY = 0;

static int g_windowW = 0, g_windowH = 0;

// ---------------------------------------------------------------------- misc

static int app_width(void) {
	if (g_app && g_app->window)
		return ANativeWindow_getWidth(g_app->window);
	return g_window ? g_window->width : 0;
}

static int app_height(void) {
	if (g_app && g_app->window)
		return ANativeWindow_getHeight(g_app->window);
	return g_window ? g_window->height : 0;
}

void cubyz_android_set_app(struct android_app* app) {
	g_app = app;
}

// ------------------------------------------------------------- GLFW lifecycle

int glfwInit(void) {
	g_initialized = 1;
	LOGI("glfwInit");
	return GLFW_TRUE;
}

void glfwTerminate(void) {
	if (g_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (g_surface != EGL_NO_SURFACE)
			eglDestroySurface(g_display, g_surface);
		if (g_context != EGL_NO_CONTEXT)
			eglDestroyContext(g_display, g_context);
		eglTerminate(g_display);
	}
	g_surface = EGL_NO_SURFACE;
	g_context = EGL_NO_CONTEXT;
	g_config = NULL;
	g_display = EGL_NO_DISPLAY;
	g_initialized = 0;
}

void glfwInitHint(int hint, int value) {
	(void)hint; (void)value;
}

void glfwWindowHint(int hint, int value) {
	(void)hint; (void)value;
}

GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share) {
	(void)title; (void)monitor; (void)share;

	if (!g_app || !g_app->window) {
		LOGE("glfwCreateWindow: android window not ready");
		if (g_errorCb) g_errorCb(0x00010008 /* GLFW_PLATFORM_ERROR */, "no ANativeWindow");
		return NULL;
	}

	g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (g_display == EGL_NO_DISPLAY) {
		LOGE("eglGetDisplay failed");
		return NULL;
	}

	EGLint major = 0, minor = 0;
	if (!eglInitialize(g_display, &major, &minor)) {
		LOGE("eglInitialize failed: 0x%x", eglGetError());
		g_display = EGL_NO_DISPLAY;
		return NULL;
	}

	const EGLint cfgAttribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint numConfigs = 0;
	if (!eglChooseConfig(g_display, cfgAttribs, &g_config, 1, &numConfigs) || numConfigs < 1) {
		LOGE("eglChooseConfig failed: 0x%x", eglGetError());
		eglTerminate(g_display);
		g_display = EGL_NO_DISPLAY;
		return NULL;
	}

	// Ask for GLES 3.x with a fallback to 2.0. Minor version hint is avoided
	// (not universally available on older EGL headers); the driver picks the
	// highest supported 3.x context.
	static const EGLint ctxAttribs3[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	static const EGLint ctxAttribs2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	g_context = eglCreateContext(g_display, g_config, EGL_NO_CONTEXT, ctxAttribs3);
	if (g_context == EGL_NO_CONTEXT)
		g_context = eglCreateContext(g_display, g_config, EGL_NO_CONTEXT, ctxAttribs2);
	if (g_context == EGL_NO_CONTEXT) {
		LOGE("eglCreateContext failed: 0x%x", eglGetError());
		eglTerminate(g_display);
		g_display = EGL_NO_DISPLAY;
		return NULL;
	}

	g_surface = eglCreateWindowSurface(g_display, g_config, (EGLNativeWindowType)g_app->window, NULL);
	if (g_surface == EGL_NO_SURFACE) {
		LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
		eglDestroyContext(g_display, g_context);
		g_context = EGL_NO_CONTEXT;
		eglTerminate(g_display);
		g_display = EGL_NO_DISPLAY;
		return NULL;
	}

	(void)share; // context sharing is not needed by the game

	GLFWwindow* w = (GLFWwindow*)calloc(1, sizeof(GLFWwindow));
	if (!w) {
		LOGE("out of memory allocating GLFWwindow");
		return NULL;
	}
	w->width = width > 0 ? width : app_width();
	w->height = height > 0 ? height : app_height();
	w->shouldClose = GLFW_FALSE;
	w->cursorMode = GLFW_CURSOR_NORMAL;
	w->swapInterval = 1;

	g_window = w;
	LOGI("created EGL window surface %dx%d (EGL %d.%d)", w->width, w->height, major, minor);
	return w;
}

void glfwDestroyWindow(GLFWwindow* window) {
	if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE) {
		eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglDestroySurface(g_display, g_surface);
		g_surface = EGL_NO_SURFACE;
	}
	if (window) {
		if (window == g_window)
			g_window = NULL;
		free(window);
	}
}

int glfwWindowShouldClose(GLFWwindow* window) {
	return window ? window->shouldClose : GLFW_TRUE;
}

void glfwSetWindowShouldClose(GLFWwindow* window, int value) {
	if (window)
		window->shouldClose = value ? GLFW_TRUE : GLFW_FALSE;
}

void glfwMakeContextCurrent(GLFWwindow* window) {
	if (g_display == EGL_NO_DISPLAY || g_surface == EGL_NO_SURFACE) {
		LOGE("glfwMakeContextCurrent: EGL not ready");
		return;
	}
	if (!eglMakeCurrent(g_display, g_surface, g_surface, g_context)) {
		LOGE("eglMakeCurrent failed: 0x%x", eglGetError());
		return;
	}
	if (window)
		glfwSwapInterval(window->swapInterval);
}

void glfwSwapBuffers(GLFWwindow* window) {
	(void)window;
	if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE)
		eglSwapBuffers(g_display, g_surface);
}

void glfwSwapInterval(int interval) {
	if (g_window)
		g_window->swapInterval = interval;
	if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE)
		eglSwapInterval(g_display, interval);
}

void glfwPollEvents(void) {
	cubyz_android_pump_events();
	cubyz_touch_update();
}

GLFWglproc glfwGetProcAddress(const char* procname) {
	return (GLFWglproc)eglGetProcAddress(procname);
}

int glfwVulkanSupported(void) {
	return GLFW_FALSE;
}

const char** glfwGetRequiredInstanceExtensions(uint32_t* count) {
	(void)count;
	return NULL;
}

// ------------------------------------------------------------------ geometry

void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
	int w = 0, h = 0;
	if (g_display != EGL_NO_DISPLAY && g_surface != EGL_NO_SURFACE) {
		EGLint ew = 0, eh = 0;
		if (eglQuerySurface(g_display, g_surface, EGL_WIDTH, &ew) &&
		    eglQuerySurface(g_display, g_surface, EGL_HEIGHT, &eh)) {
			w = ew; h = eh;
		}
	}
	if (w == 0 || h == 0) {
		w = app_width();
		h = app_height();
	}
	if (w == 0 || h == 0) {
		w = window ? window->width : 0;
		h = window ? window->height : 0;
	}
	if (width) *width = w;
	if (height) *height = h;
}

void glfwGetWindowSize(GLFWwindow* window, int* width, int* height) {
	int w = app_width();
	int h = app_height();
	if (w == 0 || h == 0) {
		w = window ? window->width : 0;
		h = window ? window->height : 0;
	}
	if (width) *width = w;
	if (height) *height = h;
}

void glfwGetWindowPos(GLFWwindow* window, int* xpos, int* ypos) {
	(void)window;
	if (xpos) *xpos = 0;
	if (ypos) *ypos = 0;
}

int glfwGetWindowAttrib(GLFWwindow* window, int attrib) {
	switch (attrib) {
		case GLFW_FOCUSED: return GLFW_TRUE;
		case GLFW_VISIBLE: return GLFW_TRUE;
		case GLFW_ICONIFIED: return GLFW_FALSE;
		case GLFW_DECORATED: return GLFW_FALSE;
		case GLFW_CURSOR: return window ? window->cursorMode : GLFW_CURSOR_NORMAL;
		default: return GLFW_FALSE;
	}
}

void glfwSetWindowAttrib(GLFWwindow* window, int attrib, int value) {
	(void)window; (void)attrib; (void)value;
}

void glfwSetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate) {
	(void)window; (void)monitor; (void)xpos; (void)ypos; (void)width; (void)height; (void)refreshRate;
}

void glfwSetWindowIcon(GLFWwindow* window, int count, const GLFWimage* images) {
	(void)window; (void)count; (void)images;
}

// ------------------------------------------------------------------ monitors

GLFWmonitor* glfwGetPrimaryMonitor(void) {
	return NULL;
}

const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor) {
	(void)monitor;
	static GLFWvidmode mode = { 0, 0, 8, 8, 8, 60 };
	mode.width = app_width();
	mode.height = app_height();
	return &mode;
}

// -------------------------------------------------------------------- input

void glfwSetInputMode(GLFWwindow* window, int mode, int value) {
	if (window && mode == GLFW_CURSOR)
		window->cursorMode = value;
}

int glfwRawMouseMotionSupported(void) {
	return GLFW_FALSE;
}

const char* glfwGetKeyName(int key, int scancode) {
	(void)key; (void)scancode;
	return NULL;
}

const char* glfwGetClipboardString(GLFWwindow* window) {
	(void)window;
	return g_clipboard;
}

void glfwSetClipboardString(GLFWwindow* window, const char* string) {
	(void)window;
	if (!string) {
		g_clipboard[0] = '\0';
		return;
	}
	size_t n = strlen(string);
	if (n >= CLIPBOARD_CAP)
		n = CLIPBOARD_CAP - 1;
	memcpy(g_clipboard, string, n);
	g_clipboard[n] = '\0';
}

// ---------------------------------------------------------------- callbacks

GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun callback) {
	GLFWerrorfun prev = g_errorCb;
	g_errorCb = callback;
	return prev;
}

GLFWkeyfun glfwSetKeyCallback(GLFWwindow* window, GLFWkeyfun callback) {
	(void)window;
	GLFWkeyfun prev = g_window ? g_window->keyCb : NULL;
	if (g_window) g_window->keyCb = callback;
	return prev;
}

GLFWcharfun glfwSetCharCallback(GLFWwindow* window, GLFWcharfun callback) {
	(void)window;
	GLFWcharfun prev = g_window ? g_window->charCb : NULL;
	if (g_window) g_window->charCb = callback;
	return prev;
}

GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow* window, GLFWframebuffersizefun callback) {
	(void)window;
	GLFWframebuffersizefun prev = g_window ? g_window->framebufferSizeCb : NULL;
	if (g_window) g_window->framebufferSizeCb = callback;
	return prev;
}

GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow* window, GLFWcursorposfun callback) {
	(void)window;
	GLFWcursorposfun prev = g_window ? g_window->cursorPosCb : NULL;
	if (g_window) g_window->cursorPosCb = callback;
	return prev;
}

GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun callback) {
	(void)window;
	GLFWmousebuttonfun prev = g_window ? g_window->mouseButtonCb : NULL;
	if (g_window) g_window->mouseButtonCb = callback;
	return prev;
}

GLFWscrollfun glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun callback) {
	(void)window;
	GLFWscrollfun prev = g_window ? g_window->scrollCb : NULL;
	if (g_window) g_window->scrollCb = callback;
	return prev;
}

// ------------------------------------------------------------------- gamepad
//
// A synthetic gamepad is always present at id 0, backed by the touch layer.
// Reporting it as connected is what makes the engine's existing gamepad
// bindings live; the touch code writes g_padAxes/g_padButtons.

int glfwJoystickPresent(int jid) {
	return jid == 0 ? GLFW_TRUE : GLFW_FALSE;
}

int glfwJoystickIsGamepad(int jid) {
	// Claim the gamepad mapping (rather than a raw joystick) so the engine does
	// not try to download SDL mappings before using our state.
	return jid == 0 ? GLFW_TRUE : GLFW_FALSE;
}

int glfwGetGamepadState(int jid, GLFWgamepadstate* state) {
	if (jid != 0 || !state)
		return GLFW_FALSE;
	memcpy(state->buttons, g_padButtons, sizeof(state->buttons));
	memcpy(state->axes, g_padAxes, sizeof(state->axes));
	return GLFW_TRUE;
}

int glfwUpdateGamepadMappings(const char* sdlMappings) {
	(void)sdlMappings;
	return GLFW_TRUE;
}

// ------------------------------------------------------- injection (glue API)

void cubyz_shim_key(int key, int scancode, int action, int mods) {
	if (g_window && g_window->keyCb)
		g_window->keyCb(g_window, key, scancode, action, mods);
}

void cubyz_shim_char(unsigned int codepoint) {
	if (g_window && g_window->charCb)
		g_window->charCb(g_window, codepoint);
}

void cubyz_shim_mouse_button(int button, int action, int mods) {
	if (g_window && g_window->mouseButtonCb)
		g_window->mouseButtonCb(g_window, button, action, mods);
}

void cubyz_shim_cursor_pos(double x, double y) {
	if (g_window && g_window->cursorPosCb)
		g_window->cursorPosCb(g_window, x, y);
}

void cubyz_shim_scroll(double dx, double dy) {
	if (g_window && g_window->scrollCb)
		g_window->scrollCb(g_window, dx, dy);
}

void cubyz_shim_framebuffer_size(int w, int h) {
	if (!g_window) return;
	g_window->width = w;
	g_window->height = h;
	if (g_window->framebufferSizeCb)
		g_window->framebufferSizeCb(g_window, w, h);
}

void cubyz_shim_request_close(void) {
	if (g_window)
		g_window->shouldClose = GLFW_TRUE;
}

// -------------------------------------------------------------- touch controls

// Layout, in window pixels. Radii are fractions of the window height so the
// controls keep their physical size across screen densities. Buttons are placed
// in the lower corners, leaving the middle clear for looking around.
static const float BTN_RADIUS_FRAC = 0.075f;
static const float PAD_RADIUS_FRAC = 0.13f;

typedef struct {
	const char* label;
	float xFrac, yFrac; // centre, as a fraction of width/height
	int keycode;
} ButtonSpec;

// Order MUST match the CUBYZ_TOUCH_BTN_* enum in cubyz_android.h.
static const ButtonSpec g_buttonSpecs[CUBYZ_TOUCH_BTN_COUNT] = {
	[CUBYZ_TOUCH_BTN_JUMP] = {"^", 0.86f, 0.74f, 0},        // A
	[CUBYZ_TOUCH_BTN_BREAK] = {"X", 0.94f, 0.52f, 0},       // right trigger
	[CUBYZ_TOUCH_BTN_PLACE] = {"O", 0.75f, 0.57f, 0},       // left trigger
	[CUBYZ_TOUCH_BTN_PICK] = {"P", 0.75f, 0.78f, 0},        // dpad-left (pick block)
	[CUBYZ_TOUCH_BTN_INVENTORY] = {"I", 0.16f, 0.12f, 0},   // X
	[CUBYZ_TOUCH_BTN_MENU] = {"=", 0.27f, 0.12f, 0},        // B
	[CUBYZ_TOUCH_BTN_FLY] = {"F", 0.05f, 0.12f, 0},         // dpad-down (fly toggle)
	[CUBYZ_TOUCH_BTN_SPRINT] = {"S", 0.94f, 0.74f, 0},      // left thumb
	[CUBYZ_TOUCH_BTN_SNEAK] = {"C", 0.86f, 0.92f, 0},       // right thumb (crouch)
	[CUBYZ_TOUCH_BTN_HOTBAR_PREV] = {"<", 0.38f, 0.12f, 0}, // left bumper
	[CUBYZ_TOUCH_BTN_HOTBAR_NEXT] = {">", 0.49f, 0.12f, 0}, // right bumper
};

// Which GLFW face button / trigger each touch button drives.
static void button_to_gamepad(int btn, int* outButton, int* outAxis) {
	*outButton = -1;
	*outAxis = -1;
	switch (btn) {
		case CUBYZ_TOUCH_BTN_JUMP: *outButton = GLFW_GAMEPAD_BUTTON_A; break;
		case CUBYZ_TOUCH_BTN_BREAK: *outAxis = GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER; break;
		case CUBYZ_TOUCH_BTN_PLACE: *outAxis = GLFW_GAMEPAD_AXIS_LEFT_TRIGGER; break;
		case CUBYZ_TOUCH_BTN_PICK: *outButton = GLFW_GAMEPAD_BUTTON_DPAD_LEFT; break;
		case CUBYZ_TOUCH_BTN_INVENTORY: *outButton = GLFW_GAMEPAD_BUTTON_X; break;
		case CUBYZ_TOUCH_BTN_MENU: *outButton = GLFW_GAMEPAD_BUTTON_B; break;
		case CUBYZ_TOUCH_BTN_FLY: *outButton = GLFW_GAMEPAD_BUTTON_DPAD_DOWN; break;
		case CUBYZ_TOUCH_BTN_SPRINT: *outButton = GLFW_GAMEPAD_BUTTON_LEFT_THUMB; break;
		case CUBYZ_TOUCH_BTN_SNEAK: *outButton = GLFW_GAMEPAD_BUTTON_RIGHT_THUMB; break;
		case CUBYZ_TOUCH_BTN_HOTBAR_PREV: *outButton = GLFW_GAMEPAD_BUTTON_LEFT_BUMPER; break;
		case CUBYZ_TOUCH_BTN_HOTBAR_NEXT: *outButton = GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER; break;
		default: break;
	}
}

// Button centres and radii for the current window size.
static void button_rect(int idx, float* cx, float* cy, float* r) {
	float w = (float)(g_windowW ? g_windowW : app_width());
	float h = (float)(g_windowH ? g_windowH : app_height());
	const ButtonSpec* s = &g_buttonSpecs[idx];
	*cx = s->xFrac*w;
	*cy = s->yFrac*h;
	*r = BTN_RADIUS_FRAC*h;
}

// Hit-test a touch against the buttons. Returns CUBYZ_TOUCH_BTN_* or -1.
// Square, matching the rectangles the overlay draws (the engine has no circle
// primitive) — the layout reports `radius` as the half-extent.
static int hit_button(float x, float y) {
	for (int i = 0; i < CUBYZ_TOUCH_BTN_COUNT; i++) {
		float cx, cy, r;
		button_rect(i, &cx, &cy, &r);
		if (fabsf(x - cx) <= r && fabsf(y - cy) <= r)
			return i;
	}
	return -1;
}

static TouchPointer* find_touch(int pointerId) {
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (g_touches[i].active && g_touches[i].pointerId == pointerId)
			return &g_touches[i];
	}
	return NULL;
}

static TouchPointer* alloc_touch(int pointerId) {
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (!g_touches[i].active) {
			memset(&g_touches[i], 0, sizeof(g_touches[i]));
			g_touches[i].active = 1;
			g_touches[i].pointerId = pointerId;
			g_touches[i].role = TOUCH_NONE;
			g_touches[i].button = -1;
			return &g_touches[i];
		}
	}
	return NULL;
}

static void release_touch(TouchPointer* t) {
	if (!t) return;
	if (t->role == TOUCH_BUTTON && t->button >= 0) {
		int gpBtn, gpAxis;
		button_to_gamepad(t->button, &gpBtn, &gpAxis);
		if (gpBtn >= 0) g_padButtons[gpBtn] = 0;
		if (gpAxis >= 0) g_padAxes[gpAxis] = 0.0f;
	}
	if (t->role == TOUCH_MOVE) {
		g_moveActive = 0;
		g_moveOffX = g_moveOffY = 0.0f;
		g_padAxes[GLFW_GAMEPAD_AXIS_LEFT_X] = 0.0f;
		g_padAxes[GLFW_GAMEPAD_AXIS_LEFT_Y] = 0.0f;
	} else if (t->role == TOUCH_LOOK) {
		// Drop any unapplied drag so it cannot leak into the next look gesture
		// (or into a menu, where it would be interpreted as cursor motion).
		g_lookDX = g_lookDY = 0.0f;
	}
	t->active = 0;
}

static void handle_touch_down(int pointerId, float x, float y) {
	TouchPointer* t = alloc_touch(pointerId);
	if (!t) return;
	t->startX = x;
	t->startY = y;
	t->curX = x;
	t->curY = y;

	const int btn = hit_button(x, y);
	if (btn >= 0) {
		t->role = TOUCH_BUTTON;
		t->button = btn;
		int gpBtn, gpAxis;
		button_to_gamepad(btn, &gpBtn, &gpAxis);
		if (gpBtn >= 0) g_padButtons[gpBtn] = 1;
		if (gpAxis >= 0) g_padAxes[gpAxis] = 1.0f;
		return;
	}

	// Left half drives movement, right half looks around. The movement stick is
	// floating: it starts wherever the finger lands.
	if (x < (float)(g_windowW ? g_windowW : app_width())*0.5f) {
		t->role = TOUCH_MOVE;
		g_moveActive = 1;
		g_moveOriginX = x;
		g_moveOriginY = y;
		g_moveOffX = g_moveOffY = 0.0f;
	} else {
		t->role = TOUCH_LOOK;
		g_lookDX = g_lookDY = 0.0f;
	}
}

static void handle_touch_move(int pointerId, float x, float y) {
	TouchPointer* t = find_touch(pointerId);
	if (!t) return;
	if (t->role == TOUCH_MOVE) {
		const float padR = PAD_RADIUS_FRAC*(float)(g_windowH ? g_windowH : app_height());
		float dx = x - g_moveOriginX;
		float dy = y - g_moveOriginY;
		float len = sqrtf(dx*dx + dy*dy);
		if (len > padR && len > 0.0f) {
			// Drag the origin along so the stick never gets stuck at full tilt.
			g_moveOriginX += dx*(1.0f - padR/len);
			g_moveOriginY += dy*(1.0f - padR/len);
			dx *= padR/len;
			dy *= padR/len;
		}
		g_moveOffX = dx;
		g_moveOffY = dy;
		g_padAxes[GLFW_GAMEPAD_AXIS_LEFT_X] = dx/padR;
		// Screen +y is down; GLFW's left-stick Y is positive upwards.
		g_padAxes[GLFW_GAMEPAD_AXIS_LEFT_Y] = -dy/padR;
	} else if (t->role == TOUCH_LOOK) {
		g_lookDX += x - t->curX;
		g_lookDY += y - t->curY;
	} else if (t->role == TOUCH_BUTTON) {
		// Allow sliding off a button to release it, and onto one to press it.
		const int btn = hit_button(x, y);
		if (btn != t->button) {
			int gpBtn, gpAxis;
			button_to_gamepad(t->button, &gpBtn, &gpAxis);
			if (gpBtn >= 0) g_padButtons[gpBtn] = 0;
			if (gpAxis >= 0) g_padAxes[gpAxis] = 0.0f;
			t->button = btn;
			if (btn >= 0) {
				button_to_gamepad(btn, &gpBtn, &gpAxis);
				if (gpBtn >= 0) g_padButtons[gpBtn] = 1;
				if (gpAxis >= 0) g_padAxes[gpAxis] = 1.0f;
			}
		}
	}
	t->curX = x;
	t->curY = y;
}

// The look-drag feeds the right stick, but only through glfwGetGamepadState —
// and the engine applies a deadzone that would swallow a small frame-to-frame
// drag. So instead the accumulated pixels are applied directly to the mouse
// cursor path, which is exactly how mouselook already works.
// The only place that feeds the engine's cursor-position callback. Both paths
// (menu touch, in-game look-drag) go through here so g_cursorX/Y always match
// what Window.zig last saw.
static void feed_cursor_pos(double x, double y) {
	g_cursorX = x;
	g_cursorY = y;
	if (g_window && g_window->cursorPosCb)
		g_window->cursorPosCb(g_window, x, y);
}

static void touch_pump_look(void) {
	if (g_lookDX == 0.0f && g_lookDY == 0.0f)
		return;
	// 1:1 — the world turns exactly as far as the finger travels (times the
	// user's mouse-sensitivity setting, applied by the engine), which is what
	// touch players expect from a drag-to-look control.
	feed_cursor_pos(g_cursorX + g_lookDX, g_cursorY + g_lookDY);
	g_lookDX = g_lookDY = 0.0f;
}

// Called from glfwPollEvents so touch-derived input is refreshed each frame.
void cubyz_touch_update(void) {
	g_windowW = app_width();
	g_windowH = app_height();
	touch_pump_look();
}

// ------------------------------------------------------------------ glue API

int cubyz_touch_button(int index, CubyzTouchLayout* out) {
	if (!out || index < 0 || index >= CUBYZ_TOUCH_BTN_COUNT)
		return 0;
	float cx, cy, r;
	button_rect(index, &cx, &cy, &r);
	out->x = cx;
	out->y = cy;
	out->radius = r;
	out->label = g_buttonSpecs[index].label;
	return 1;
}

int cubyz_touch_move_pad(float* outOriginX, float* outOriginY, float* outOffsetX, float* outOffsetY) {
	if (outOriginX) *outOriginX = g_moveOriginX;
	if (outOriginY) *outOriginY = g_moveOriginY;
	if (outOffsetX) *outOffsetX = g_moveOffX;
	if (outOffsetY) *outOffsetY = g_moveOffY;
	return g_moveActive;
}

int cubyz_touch_in_game(void) {
	return (g_window && g_window->cursorMode == GLFW_CURSOR_DISABLED) ? 1 : 0;
}

void cubyz_touch_window_size(int* width, int* height) {
	if (width) *width = g_windowW ? g_windowW : app_width();
	if (height) *height = g_windowH ? g_windowH : app_height();
}

// The engine's framebufferSize callback reports the surface size it actually
// uses. That callback is the single source of truth for the layout: it also
// covers the resize events the game only learns about through its own poll loop
// (APP_CMD_WINDOW_RESIZED is not always delivered), so the overlay can never
// drift away from what the engine renders into.
void cubyz_touch_sync_window_size(void) {
	if (g_app && g_app->window) {
		const int w = ANativeWindow_getWidth(g_app->window);
		const int h = ANativeWindow_getHeight(g_app->window);
		if (w > 0 && h > 0) {
			g_windowW = w;
			g_windowH = h;
		}
	}
	if (g_windowW <= 0) g_windowW = app_width();
	if (g_windowH <= 0) g_windowH = app_height();
}

// Entry point used by android_main.c to forward motion events. Returns 1 if the
// event was consumed by the touch controls.
int cubyz_touch_handle_motion(AInputEvent* event) {
	if (!event)
		return 0;
	const int32_t action = AMotionEvent_getAction(event);
	const int masked = action & AMOTION_EVENT_ACTION_MASK;
	const size_t count = AMotionEvent_getPointerCount(event);

	g_windowW = app_width();
	g_windowH = app_height();

	// A real mouse (or trackpad) must behave exactly as on the desktop, in menus
	// and in-game alike — it is not a finger, so it never drives the virtual
	// gamepad. Only touchscreen events go through the touch state machine.
	const int32_t source = AInputEvent_getSource(event);
	const int isTouch = (source & AINPUT_SOURCE_TOUCHSCREEN) != 0;

	// In menus the pointer is free, so touches act as ordinary mouse clicks —
	// that keeps every existing GUI directly touchable. Only while the game owns
	// the pointer do fingers become a gamepad.
	const int inGame = cubyz_touch_in_game();
	if (!isTouch || !inGame) {
		if (count > 0) {
			const float x = AMotionEvent_getX(event, 0);
			const float y = AMotionEvent_getY(event, 0);
			feed_cursor_pos(x, y);
			if (masked == AMOTION_EVENT_ACTION_DOWN)
				cubyz_shim_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
			else if (masked == AMOTION_EVENT_ACTION_UP)
				cubyz_shim_mouse_button(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
		}
		return 1;
	}

	switch (masked) {
		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_POINTER_DOWN: {
			const size_t idx = (masked == AMOTION_EVENT_ACTION_POINTER_DOWN)
			                      ? (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT)
			                      : 0;
			if (idx < count) {
				const int id = AMotionEvent_getPointerId(event, idx);
				handle_touch_down(id, AMotionEvent_getX(event, idx), AMotionEvent_getY(event, idx));
			}
			break;
		}
		case AMOTION_EVENT_ACTION_MOVE: {
			for (size_t i = 0; i < count; i++)
				handle_touch_move(AMotionEvent_getPointerId(event, i), AMotionEvent_getX(event, i), AMotionEvent_getY(event, i));
			break;
		}
		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP: {
			const size_t idx = (masked == AMOTION_EVENT_ACTION_POINTER_UP)
			                      ? (size_t)((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT)
			                      : 0;
			if (idx < count)
				release_touch(find_touch(AMotionEvent_getPointerId(event, idx)));
			break;
		}
		case AMOTION_EVENT_ACTION_CANCEL:
			for (int i = 0; i < MAX_TOUCH_POINTERS; i++)
				release_touch(&g_touches[i]);
			break;
		default:
			break;
	}
	return 1;
}

// Called by android_main.c on window resize so the layout tracks the surface.
void cubyz_touch_set_window_size(int w, int h) {
	g_windowW = w;
	g_windowH = h;
}
