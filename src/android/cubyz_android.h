// Shared API between the GLFW shim (glfw_shim.c) and the Android entry point
// (android_main.c). Plain C so it is harmless under Zig's translate-c.
#ifndef CUBYZ_ANDROID_H
#define CUBYZ_ANDROID_H

#ifdef __cplusplus
extern "C" {
#endif

struct android_app;
struct AInputEvent;

// Give the shim the app pointer (EGL window surface creation + event pumping).
void cubyz_android_set_app(struct android_app* app);

// ------------------------------------------------------------- touch controls
//
// The touch layer turns fingers into a virtual gamepad (see glfw_shim.c), so the
// engine's existing gamepad bindings drive movement, jumping, breaking and
// placing blocks without any Zig-side input plumbing. These declarations only
// describe the on-screen controls the Zig overlay draws on top of the game.

enum {
	CUBYZ_TOUCH_BTN_JUMP = 0,
	CUBYZ_TOUCH_BTN_BREAK,
	CUBYZ_TOUCH_BTN_PLACE,
	CUBYZ_TOUCH_BTN_PICK,
	CUBYZ_TOUCH_BTN_INVENTORY,
	CUBYZ_TOUCH_BTN_MENU,
	CUBYZ_TOUCH_BTN_FLY,
	CUBYZ_TOUCH_BTN_SPRINT,
	CUBYZ_TOUCH_BTN_SNEAK,
	CUBYZ_TOUCH_BTN_HOTBAR_PREV,
	CUBYZ_TOUCH_BTN_HOTBAR_NEXT,
	CUBYZ_TOUCH_BTN_COUNT,
};

typedef struct CubyzTouchLayout {
	// Centre in window pixels.
	float x, y;
	// Radius in window pixels.
	float radius;
	// Label drawn centred in the button (NUL-terminated, never NULL).
	const char* label;
} CubyzTouchLayout;

// Describes one on-screen control. Returns 0 for an out-of-range index.
int cubyz_touch_button(int index, CubyzTouchLayout* out);

// 1 while a finger is held on the movement pad. Writes the pad origin (where the
// finger landed) and the knob's offset from it, both in window pixels.
int cubyz_touch_move_pad(float* outOriginX, float* outOriginY, float* outOffsetX, float* outOffsetY);

// 1 while the game owns the pointer (cursor grabbed), i.e. when the in-game
// controls are meaningful. Mirrors GLFW_CURSOR_DISABLED.
int cubyz_touch_in_game(void);

// Window size in pixels, matching what the layout above was measured against.
void cubyz_touch_window_size(int* width, int* height);

// Forwarded from the glue's input handler. Returns 1 when the touch layer
// consumed the event (in-game); 0 lets the caller fall back to mouse emulation.
int cubyz_touch_handle_motion(AInputEvent* event);

// Keeps the layout in sync after a window resize.
void cubyz_touch_set_window_size(int w, int h);

// Re-reads the surface size so the layout can never drift away from the size the
// engine actually renders into.
void cubyz_touch_sync_window_size(void);

// Refreshes touch-derived input once per frame (called from glfwPollEvents).
void cubyz_touch_update(void);

// Absolute paths resolved from the activity (valid once the game starts).
const char* cubyz_android_home_path(void);
const char* cubyz_android_cwd_path(void);

// Pump the Android looper once (drains lifecycle + input queues).
void cubyz_android_pump_events(void);

// Input injection API called by the glue after translating AInputEvent.
void cubyz_shim_key(int key, int scancode, int action, int mods);
void cubyz_shim_char(unsigned int codepoint);
void cubyz_shim_mouse_button(int button, int action, int mods);
void cubyz_shim_cursor_pos(double x, double y);
void cubyz_shim_scroll(double dx, double dy);
void cubyz_shim_framebuffer_size(int w, int h);
void cubyz_shim_request_close(void);

// Game entry point, implemented on the Zig side. Blocks running the game loop.
extern void cubyz_android_main(void);

#ifdef __cplusplus
}
#endif

#endif
