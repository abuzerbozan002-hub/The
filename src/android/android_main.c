// Android entry point: drives the native_app_glue lifecycle, waits for the
// window, then hands control to the Zig game loop. Translates AInputEvent into
// the shim's GLFW-style callbacks.
#include <android_native_app_glue.h>

#include <android/log.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <jni.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <GLFW/glfw3.h>

#include "cubyz_android.h"

#define LOG_TAG "CctesMain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static struct android_app* g_app = NULL;
static int g_quit = 0;

// --------------------------------------------------------------- app commands

static void handle_cmd(struct android_app* app, int32_t cmd) {
	switch (cmd) {
		case APP_CMD_INIT_WINDOW:
			if (app->window) {
				LOGI("APP_CMD_INIT_WINDOW %dx%d",
				     ANativeWindow_getWidth(app->window),
				     ANativeWindow_getHeight(app->window));
			}
			break;
		case APP_CMD_WINDOW_RESIZED:
			if (app->window) {
				int w = ANativeWindow_getWidth(app->window);
				int h = ANativeWindow_getHeight(app->window);
				LOGI("APP_CMD_WINDOW_RESIZED %dx%d", w, h);
				cubyz_shim_framebuffer_size(w, h);
				cubyz_touch_set_window_size(w, h);
			}
			break;
		case APP_CMD_TERM_WINDOW:
			LOGI("APP_CMD_TERM_WINDOW");
			cubyz_shim_request_close();
			break;
		case APP_CMD_GAINED_FOCUS:
		case APP_CMD_LOST_FOCUS:
			break;
		case APP_CMD_DESTROY:
			LOGI("APP_CMD_DESTROY");
			g_quit = 1;
			break;
		default:
			break;
	}
}

// --------------------------------------------------------------------- input

static int map_keycode(int32_t kc) {
	switch (kc) {
		case AKEYCODE_BACK: return GLFW_KEY_ESCAPE;
		case AKEYCODE_ENTER:
		case AKEYCODE_NUMPAD_ENTER: return GLFW_KEY_ENTER;
		case AKEYCODE_TAB: return GLFW_KEY_TAB;
		case AKEYCODE_DEL: return GLFW_KEY_BACKSPACE;
		case AKEYCODE_FORWARD_DEL: return GLFW_KEY_DELETE;
		case AKEYCODE_DPAD_LEFT: return GLFW_KEY_LEFT;
		case AKEYCODE_DPAD_RIGHT: return GLFW_KEY_RIGHT;
		case AKEYCODE_DPAD_UP: return GLFW_KEY_UP;
		case AKEYCODE_DPAD_DOWN: return GLFW_KEY_DOWN;
		case AKEYCODE_MOVE_HOME: return GLFW_KEY_HOME;
		case AKEYCODE_MOVE_END: return GLFW_KEY_END;
		case AKEYCODE_PAGE_UP: return GLFW_KEY_PAGE_UP;
		case AKEYCODE_PAGE_DOWN: return GLFW_KEY_PAGE_DOWN;
		case AKEYCODE_INSERT: return GLFW_KEY_INSERT;
		case AKEYCODE_ESCAPE: return GLFW_KEY_ESCAPE;
		case AKEYCODE_SPACE: return GLFW_KEY_SPACE;
		case AKEYCODE_MINUS: return GLFW_KEY_MINUS;
		case AKEYCODE_EQUALS: return GLFW_KEY_EQUAL;
		case AKEYCODE_SLASH: return GLFW_KEY_SLASH;
		case AKEYCODE_GRAVE: return GLFW_KEY_GRAVE_ACCENT;
		case AKEYCODE_SHIFT_LEFT: return GLFW_KEY_LEFT_SHIFT;
		case AKEYCODE_SHIFT_RIGHT: return GLFW_KEY_RIGHT_SHIFT;
		case AKEYCODE_CTRL_LEFT: return GLFW_KEY_LEFT_CONTROL;
		case AKEYCODE_CTRL_RIGHT: return GLFW_KEY_RIGHT_CONTROL;
		case AKEYCODE_ALT_LEFT: return GLFW_KEY_LEFT_ALT;
		case AKEYCODE_ALT_RIGHT: return GLFW_KEY_RIGHT_ALT;
		default:
			break;
	}
	if (kc >= AKEYCODE_0 && kc <= AKEYCODE_9)
		return GLFW_KEY_0 + (kc - AKEYCODE_0);
	if (kc >= AKEYCODE_A && kc <= AKEYCODE_Z)
		return GLFW_KEY_A + (kc - AKEYCODE_A);
	if (kc >= AKEYCODE_F1 && kc <= AKEYCODE_F12)
		return GLFW_KEY_F1 + (kc - AKEYCODE_F1);
	return GLFW_KEY_UNKNOWN;
}

// The public NDK header exposes no unicode accessor for key events, so derive
// the character from the keycode + modifier state.
static int keycode_to_char(int32_t kc, int32_t meta) {
	int shift = (meta & AMETA_SHIFT_ON) != 0;
	if (kc >= AKEYCODE_A && kc <= AKEYCODE_Z)
		return (shift ? 'A' : 'a') + (kc - AKEYCODE_A);
	if (kc >= AKEYCODE_0 && kc <= AKEYCODE_9) {
		static const char shifted[] = ")!@#$%^&*(";
		if (shift)
			return (unsigned char)shifted[kc - AKEYCODE_0];
		return '0' + (kc - AKEYCODE_0);
	}
	switch (kc) {
		case AKEYCODE_SPACE: return ' ';
		case AKEYCODE_MINUS: return shift ? '_' : '-';
		case AKEYCODE_EQUALS: return shift ? '+' : '=';
		case AKEYCODE_SLASH: return shift ? '?' : '/';
		case AKEYCODE_BACKSLASH: return shift ? '|' : '\\';
		case AKEYCODE_COMMA: return shift ? '<' : ',';
		case AKEYCODE_PERIOD: return shift ? '>' : '.';
		case AKEYCODE_SEMICOLON: return shift ? ':' : ';';
		case AKEYCODE_APOSTROPHE: return shift ? '"' : '\'';
		case AKEYCODE_GRAVE: return shift ? '~' : '`';
		case AKEYCODE_LEFT_BRACKET: return shift ? '{' : '[';
		case AKEYCODE_RIGHT_BRACKET: return shift ? '}' : ']';
		default: return 0;
	}
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
	(void)app;
	int32_t type = AInputEvent_getType(event);

	if (type == AINPUT_EVENT_TYPE_MOTION) {
		// The touch layer decides between mouse emulation (menus) and the
		// virtual gamepad (in-game); see glfw_shim.c.
		return cubyz_touch_handle_motion(event);
	}

	if (type == AINPUT_EVENT_TYPE_KEY) {
		int32_t aaction = AKeyEvent_getAction(event);
		if (aaction == AKEY_EVENT_ACTION_DOWN || aaction == AKEY_EVENT_ACTION_UP) {
			int32_t kc = AKeyEvent_getKeyCode(event);
			int action = (aaction == AKEY_EVENT_ACTION_DOWN)
			                 ? (AKeyEvent_getRepeatCount(event) > 0 ? GLFW_REPEAT : GLFW_PRESS)
			                 : GLFW_RELEASE;
			int key = map_keycode(kc);
			int32_t meta = AKeyEvent_getMetaState(event);
			int mods = 0;
			if (meta & AMETA_SHIFT_ON) mods |= 0x0001; // GLFW_MOD_SHIFT
			if (meta & AMETA_CTRL_ON) mods |= 0x0002;  // GLFW_MOD_CONTROL
			if (meta & AMETA_ALT_ON) mods |= 0x0004;   // GLFW_MOD_ALT
			cubyz_shim_key(key, kc, action, mods);

			if (action != GLFW_RELEASE && !(meta & AMETA_CTRL_ON)) {
				// The public NDK header exposes no Unicode accessor for key
				// events, so the character is reconstructed from keycode+shift.
				// BACK is never text: the engine maps it to Escape, which is how
				// the on-screen keyboard is dismissed.
				int uc = keycode_to_char(kc, meta);
				if (uc > 0 && kc != AKEYCODE_BACK)
					cubyz_shim_char((unsigned int)uc);
			}
		}
		return 1;
	}

	return 0;
}

// ------------------------------------------------------------------ soft keyboard
//
// A NativeActivity has no EditText, so the system never raises the IME by
// itself: the game must ask whenever a text field gains focus. Forwards to
// MainActivity.setSoftKeyboard, which drives the InputMethodManager on the UI
// thread. Every JNI handle is released immediately — they are only needed for
// the duration of the call.
void cubyz_android_set_soft_keyboard(int show) {
	if (!g_app || !g_app->activity || !g_app->activity->clazz)
		return;
	JavaVM* vm = g_app->activity->vm;
	if (!vm)
		return;
	JNIEnv* env = NULL;
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		// The game loop runs on the glue's own thread, which the JVM does not
		// know about yet; attach it so GetEnv has something to return.
		if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
			return;
	}
	jclass cls = (*env)->GetObjectClass(env, g_app->activity->clazz);
	if (!cls)
		return;
	jmethodID mid = (*env)->GetMethodID(env, cls, "setSoftKeyboard", "(Z)V");
	if (mid)
		(*env)->CallVoidMethod(env, g_app->activity->clazz, mid, show ? JNI_TRUE : JNI_FALSE);
	if ((*env)->ExceptionCheck(env))
		(*env)->ExceptionClear(env);
	(*env)->DeleteLocalRef(env, cls);
}

// ------------------------------------------------------------------- pumping

void cubyz_android_pump_events(void) {
	if (!g_app) return;
	int events;
	struct android_poll_source* source;
	while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
		if (source)
			source->process(g_app, source);
		if (g_app->destroyRequested) {
			cubyz_shim_request_close();
			break;
		}
	}
}

// ---------------------------------------------------------------------- main

// Private per-app directory (writable, matches the package's data dir). Cubyz
// stores its userdata under $HOME/.cubyz, so this becomes HOME.
static char g_home[PATH_MAX];
static char g_cwd[PATH_MAX];

const char* cubyz_android_home_path(void) {
	return g_home;
}

const char* cubyz_android_cwd_path(void) {
	return g_cwd;
}

// Suppress the engine's "Downloading controller mappings..." step. The shim
// reports a synthetic gamepad at id 0, so `isControllerConnected()` is always
// true and `downloadControllerMappings()` would otherwise fetch SDL mappings on
// first launch — pointless here (our pad is not a real HID device, and the shim
// never reports any other joystick), and its progress window is not closeable.
// Writing a fresh timestamp makes the engine treat the mappings as already
// current: `Window.zig` reads ./gamecontrollerdb.stamp as hex nanoseconds of
// CLOCK_REALTIME (std.Io.Clock.`real`) and skips the download when it is < 7
// days old.
static void prevent_controller_mapping_download(void) {
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
		return;
	long long nanos = (long long)ts.tv_sec*1000000000LL + (long long)ts.tv_nsec;
	FILE* f = fopen("gamecontrollerdb.stamp", "wb");
	if (!f)
		return;
	fprintf(f, "%llx", nanos);
	fclose(f);
}

void android_main(struct android_app* app) {
	g_app = app;
	app->onAppCmd = handle_cmd;
	app->onInputEvent = handle_input;

	cubyz_android_set_app(app);

	// internalDataPath is already getFilesDir() (i.e. .../<pkg>/files), which is
	// exactly where AssetExtractor unpacked the assets and where Cubyz should
	// keep its userdata. Use it as both HOME and the working directory so the
	// game's relative paths ("assets/cubyz/...") resolve.
	const char* data = app->activity ? app->activity->internalDataPath : NULL;
	if (data) {
		snprintf(g_home, sizeof(g_home), "%s", data);
	} else {
		snprintf(g_home, sizeof(g_home), "/data/local/tmp");
	}
	snprintf(g_cwd, sizeof(g_cwd), "%s", g_home);
	if (chdir(g_cwd) != 0) {
		LOGE("chdir(%s) failed", g_cwd);
	}
	prevent_controller_mapping_download();

	// Wait until the window exists, pumping events so nothing is lost.
	while (app->window == NULL && !g_quit && !app->destroyRequested) {
		int events;
		struct android_poll_source* source;
		int timeout = (!app->destroyRequested) ? -1 : 0;
		while (ALooper_pollOnce(timeout, NULL, &events, (void**)&source) >= 0) {
			if (source)
				source->process(app, source);
			if (app->destroyRequested || g_quit)
				return;
			timeout = 0; // drain remaining events without blocking
		}
	}

	if (!app->window || g_quit || app->destroyRequested) {
		LOGI("android_main: exiting before game start");
		return;
	}

	LOGI("android_main: window ready, entering game");
	cubyz_android_main();
	LOGI("android_main: game loop returned");
}
