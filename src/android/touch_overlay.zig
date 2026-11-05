//! On-screen touch controls, drawn on top of the game on Android.
//!
//! The inputs themselves are handled in C: `glfw_shim.c` turns fingers into a
//! virtual gamepad, so movement, looking, breaking and placing all flow through
//! the engine's ordinary gamepad bindings. This module only *draws* the controls
//! so the player can see where to touch; the layout it reads back from C is the
//! same one the hit-testing uses, in window pixels.

const std = @import("std");

const main = @import("main");
const graphics = main.graphics;
const draw = graphics.draw;
const vec = main.vec;
const Vec2f = vec.Vec2f;

const isAndroid = main.isAndroid;

const c = @import("c");

const ButtonLayout = extern struct {
	x: f32,
	y: f32,
	radius: f32,
	label: [*:0]const u8,
};

extern fn cubyz_touch_button(index: c_int, out: *ButtonLayout) c_int;
extern fn cubyz_touch_move_pad(outOriginX: *f32, outOriginY: *f32, outOffsetX: *f32, outOffsetY: *f32) c_int;
extern fn cubyz_touch_in_game() c_int;
extern fn cubyz_touch_window_size(width: *c_int, height: *c_int) void;

const buttonCount = 11;

const buttonFill = 0x66101820;
const buttonEdge = 0xb0d8e8ff;
const stickFill = 0x44101820;
const stickKnob = 0x88d8e8ff;

pub fn render(currentFrame: *graphics.vulkan.Frame) void {
	if (comptime !isAndroid) return;
	// Controls only make sense while the game owns the pointer; in menus touches
	// are forwarded as ordinary mouse clicks and the normal cursor is shown.
	if (cubyz_touch_in_game() == 0) return;
	if (main.gui.hideGui) return;

	var width: c_int = 0;
	var height: c_int = 0;
	cubyz_touch_window_size(&width, &height);
	const h: f32 = @floatFromInt(height);

	// C reports window pixels; this pass draws in GUI units (see gamepad_cursor,
	// which divides its pixel mouse position by the same scale). Convert once.
	const s = main.gui.scale;
	const toGui = struct {
		fn f(v: f32, scale: f32) f32 {
			return v/scale;
		}
	}.f;

	const savedColor = draw.setColor(0xffffffff);
	defer draw.restoreColor(savedColor);

	for (0..buttonCount) |i| {
		var btn: ButtonLayout = undefined;
		if (cubyz_touch_button(@intCast(i), &btn) == 0) continue;
		const radius = toGui(btn.radius, s);
		const centre = Vec2f{toGui(btn.x, s), toGui(btn.y, s)};
		const pos = centre - Vec2f{radius, radius};
		const dim = Vec2f{radius*2, radius*2};

		_ = draw.setColor(buttonFill);
		draw.rect(currentFrame, pos, dim);
		_ = draw.setColor(buttonEdge);
		draw.rectBorder(currentFrame, pos, dim, @max(2.0, toGui(h, s)/400.0));

		const label = std.mem.span(btn.label);
		if (label.len != 0) {
			const fontSize = radius*0.85;
			draw.print(currentFrame, "{s}", .{label}, centre[0] - fontSize*0.28, centre[1] - fontSize*0.5, fontSize);
		}
		draw.restoreColor(savedColor);
	}

	var originX: f32 = 0;
	var originY: f32 = 0;
	var offX: f32 = 0;
	var offY: f32 = 0;
	if (cubyz_touch_move_pad(&originX, &originY, &offX, &offY) != 0) {
		// Floating stick: the base appears where the finger landed, the knob at
		// the finger's deflection from there.
		const radius = toGui(h*0.13, s);
		const origin = Vec2f{toGui(originX, s), toGui(originY, s)};
		_ = draw.setColor(stickFill);
		draw.rect(currentFrame, origin - Vec2f{radius, radius}, .{radius*2, radius*2});
		_ = draw.setColor(stickKnob);
		const knobR = radius*0.35;
		draw.rect(currentFrame, origin + Vec2f{toGui(offX, s), toGui(offY, s)} - Vec2f{knobR, knobR}, .{knobR*2, knobR*2});
	}
}
