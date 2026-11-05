// Android shim for GLFW: reproduces the subset of the GLFW 3.4 API that Cubyz uses,
// so the game code compiles unchanged against EGL + Android input instead of GLFW.
// Constant values mirror the real GLFW ABI.
#ifndef CUBYZ_ANDROID_GLFW_SHIM_H
#define CUBYZ_ANDROID_GLFW_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLFW_TRUE 1
#define GLFW_FALSE 0
#define GLFW_DONT_CARE (-1)

// ---- Window hints / attributes ----
#define GLFW_CLIENT_API 0x00022001
#define GLFW_CONTEXT_VERSION_MAJOR 0x00022002
#define GLFW_CONTEXT_VERSION_MINOR 0x00022003
#define GLFW_OPENGL_DEBUG_CONTEXT 0x00022007
#define GLFW_COCOA_CHDIR_RESOURCES 0x00023001
#define GLFW_VISIBLE 0x00020004
#define GLFW_DECORATED 0x00020005
#define GLFW_ICONIFIED 0x00020002
#define GLFW_FOCUSED 0x00020001

#define GLFW_OPENGL_API 0x00030001
#define GLFW_NO_API 0x00000000

#define GLFW_CURSOR 0x00033001
#define GLFW_RAW_MOUSE_MOTION 0x00033005
#define GLFW_CURSOR_NORMAL 0x00034001
#define GLFW_CURSOR_HIDDEN 0x00034002
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_JOYSTICK_LAST 15

// ---- Key/action codes ----
#define GLFW_RELEASE 0
#define GLFW_PRESS 1
#define GLFW_REPEAT 2

#define GLFW_KEY_UNKNOWN (-1)
#define GLFW_KEY_SPACE 32
#define GLFW_KEY_MINUS 45
#define GLFW_KEY_EQUAL 61
#define GLFW_KEY_SLASH 47
#define GLFW_KEY_0 48
#define GLFW_KEY_1 49
#define GLFW_KEY_2 50
#define GLFW_KEY_3 51
#define GLFW_KEY_4 52
#define GLFW_KEY_5 53
#define GLFW_KEY_6 54
#define GLFW_KEY_7 55
#define GLFW_KEY_8 56
#define GLFW_KEY_9 57
#define GLFW_KEY_A 65
#define GLFW_KEY_B 66
#define GLFW_KEY_C 67
#define GLFW_KEY_D 68
#define GLFW_KEY_E 69
#define GLFW_KEY_F 70
#define GLFW_KEY_G 71
#define GLFW_KEY_H 72
#define GLFW_KEY_I 73
#define GLFW_KEY_J 74
#define GLFW_KEY_K 75
#define GLFW_KEY_L 76
#define GLFW_KEY_M 77
#define GLFW_KEY_N 78
#define GLFW_KEY_O 79
#define GLFW_KEY_P 80
#define GLFW_KEY_Q 81
#define GLFW_KEY_R 82
#define GLFW_KEY_S 83
#define GLFW_KEY_T 84
#define GLFW_KEY_U 85
#define GLFW_KEY_V 86
#define GLFW_KEY_W 87
#define GLFW_KEY_X 88
#define GLFW_KEY_Y 89
#define GLFW_KEY_Z 90
#define GLFW_KEY_GRAVE_ACCENT 96

#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_ENTER 257
#define GLFW_KEY_TAB 258
#define GLFW_KEY_BACKSPACE 259
#define GLFW_KEY_INSERT 260
#define GLFW_KEY_DELETE 261
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_UP 265
#define GLFW_KEY_PAGE_UP 266
#define GLFW_KEY_PAGE_DOWN 267
#define GLFW_KEY_HOME 268
#define GLFW_KEY_END 269
#define GLFW_KEY_CAPS_LOCK 280
#define GLFW_KEY_SCROLL_LOCK 281
#define GLFW_KEY_NUM_LOCK 282
#define GLFW_KEY_PRINT_SCREEN 283
#define GLFW_KEY_PAUSE 284
#define GLFW_KEY_F1 290
#define GLFW_KEY_F2 291
#define GLFW_KEY_F3 292
#define GLFW_KEY_F4 293
#define GLFW_KEY_F5 294
#define GLFW_KEY_F6 295
#define GLFW_KEY_F7 296
#define GLFW_KEY_F8 297
#define GLFW_KEY_F9 298
#define GLFW_KEY_F10 299
#define GLFW_KEY_F11 300
#define GLFW_KEY_F12 301
#define GLFW_KEY_F13 302
#define GLFW_KEY_F14 303
#define GLFW_KEY_F15 304
#define GLFW_KEY_F16 305
#define GLFW_KEY_F17 306
#define GLFW_KEY_F18 307
#define GLFW_KEY_F19 308
#define GLFW_KEY_F20 309
#define GLFW_KEY_F21 310
#define GLFW_KEY_F22 311
#define GLFW_KEY_F23 312
#define GLFW_KEY_F24 313
#define GLFW_KEY_F25 314
#define GLFW_KEY_KP_0 320
#define GLFW_KEY_KP_1 321
#define GLFW_KEY_KP_2 322
#define GLFW_KEY_KP_3 323
#define GLFW_KEY_KP_4 324
#define GLFW_KEY_KP_5 325
#define GLFW_KEY_KP_6 326
#define GLFW_KEY_KP_7 327
#define GLFW_KEY_KP_8 328
#define GLFW_KEY_KP_9 329
#define GLFW_KEY_KP_DECIMAL 330
#define GLFW_KEY_KP_DIVIDE 331
#define GLFW_KEY_KP_MULTIPLY 332
#define GLFW_KEY_KP_SUBTRACT 333
#define GLFW_KEY_KP_ADD 334
#define GLFW_KEY_KP_ENTER 335
#define GLFW_KEY_KP_EQUAL 336
#define GLFW_KEY_LEFT_SHIFT 340
#define GLFW_KEY_LEFT_CONTROL 341
#define GLFW_KEY_LEFT_ALT 342
#define GLFW_KEY_LEFT_SUPER 343
#define GLFW_KEY_RIGHT_SHIFT 344
#define GLFW_KEY_RIGHT_CONTROL 345
#define GLFW_KEY_RIGHT_ALT 346
#define GLFW_KEY_RIGHT_SUPER 347
#define GLFW_KEY_MENU 348

#define GLFW_MOUSE_BUTTON_LEFT 0
#define GLFW_MOUSE_BUTTON_RIGHT 1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

// ---- Gamepad ----
#define GLFW_GAMEPAD_BUTTON_A 0
#define GLFW_GAMEPAD_BUTTON_B 1
#define GLFW_GAMEPAD_BUTTON_X 2
#define GLFW_GAMEPAD_BUTTON_Y 3
#define GLFW_GAMEPAD_BUTTON_LEFT_BUMPER 4
#define GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER 5
#define GLFW_GAMEPAD_BUTTON_BACK 6
#define GLFW_GAMEPAD_BUTTON_START 7
#define GLFW_GAMEPAD_BUTTON_GUIDE 8
#define GLFW_GAMEPAD_BUTTON_LEFT_THUMB 9
#define GLFW_GAMEPAD_BUTTON_RIGHT_THUMB 10
#define GLFW_GAMEPAD_BUTTON_DPAD_UP 11
#define GLFW_GAMEPAD_BUTTON_DPAD_RIGHT 12
#define GLFW_GAMEPAD_BUTTON_DPAD_DOWN 13
#define GLFW_GAMEPAD_BUTTON_DPAD_LEFT 14
#define GLFW_GAMEPAD_BUTTON_LAST GLFW_GAMEPAD_BUTTON_DPAD_LEFT

#define GLFW_GAMEPAD_AXIS_LEFT_X 0
#define GLFW_GAMEPAD_AXIS_LEFT_Y 1
#define GLFW_GAMEPAD_AXIS_RIGHT_X 2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y 3
#define GLFW_GAMEPAD_AXIS_LEFT_TRIGGER 4
#define GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define GLFW_GAMEPAD_AXIS_LAST GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER

// ---- Types ----
typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWvidmode {
	int width;
	int height;
	int redBits;
	int greenBits;
	int blueBits;
	int refreshRate;
} GLFWvidmode;

typedef struct GLFWgamepadstate {
	unsigned char buttons[15];
	float axes[6];
} GLFWgamepadstate;

typedef struct GLFWimage {
	int width;
	int height;
	unsigned char* pixels;
} GLFWimage;

typedef void (*GLFWglproc)(void);
typedef void (*GLFWerrorfun)(int, const char*);
typedef void (*GLFWkeyfun)(GLFWwindow*, int, int, int, int);
typedef void (*GLFWcharfun)(GLFWwindow*, unsigned int);
typedef void (*GLFWframebuffersizefun)(GLFWwindow*, int, int);
typedef void (*GLFWcursorposfun)(GLFWwindow*, double, double);
typedef void (*GLFWmousebuttonfun)(GLFWwindow*, int, int, int);
typedef void (*GLFWscrollfun)(GLFWwindow*, double, double);

// ---- Functions ----
int glfwInit(void);
void glfwTerminate(void);
void glfwInitHint(int hint, int value);
void glfwWindowHint(int hint, int value);

GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
void glfwDestroyWindow(GLFWwindow* window);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwSetWindowShouldClose(GLFWwindow* window, int value);
void glfwMakeContextCurrent(GLFWwindow* window);
void glfwSwapBuffers(GLFWwindow* window);
void glfwSwapInterval(int interval);
void glfwPollEvents(void);

GLFWglproc glfwGetProcAddress(const char* procname);
int glfwVulkanSupported(void);
const char** glfwGetRequiredInstanceExtensions(uint32_t* count);

void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);
void glfwGetWindowPos(GLFWwindow* window, int* xpos, int* ypos);
int glfwGetWindowAttrib(GLFWwindow* window, int attrib);
void glfwSetWindowAttrib(GLFWwindow* window, int attrib, int value);
void glfwSetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
void glfwSetWindowIcon(GLFWwindow* window, int count, const GLFWimage* images);

GLFWmonitor* glfwGetPrimaryMonitor(void);
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor);

void glfwSetInputMode(GLFWwindow* window, int mode, int value);
int glfwRawMouseMotionSupported(void);

const char* glfwGetKeyName(int key, int scancode);
const char* glfwGetClipboardString(GLFWwindow* window);
void glfwSetClipboardString(GLFWwindow* window, const char* string);

GLFWerrorfun glfwSetErrorCallback(GLFWerrorfun callback);
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* window, GLFWkeyfun callback);
GLFWcharfun glfwSetCharCallback(GLFWwindow* window, GLFWcharfun callback);
GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow* window, GLFWframebuffersizefun callback);
GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow* window, GLFWcursorposfun callback);
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun callback);
GLFWscrollfun glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun callback);

// ---- Gamepad / joystick ----
int glfwJoystickPresent(int jid);
int glfwJoystickIsGamepad(int jid);
int glfwGetGamepadState(int jid, GLFWgamepadstate* state);
int glfwUpdateGamepadMappings(const char* sdlMappings);

// ---- Vulkan surface creation ----
// Declared only when a Vulkan header was included first, matching real GLFW. On
// Android no Vulkan surface is ever created (vulkanTestingMode is off), but the
// symbol has to resolve — see android/vulkan_stubs.c.
#ifdef VK_VERSION_1_0
int glfwCreateWindowSurface(VkInstance instance, GLFWwindow* window, const void* allocator, VkSurfaceKHR* surface);
#endif

#ifdef __cplusplus
}
#endif

#endif
