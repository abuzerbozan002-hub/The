//! Minimal Zig-side bindings for the C platform layer in `src/android/`.
//! Only ever called when building for Android; on other targets the enclosing
//! `if (isAndroid)` in main.zig eliminates the calls at comptime.
const std = @import("std");
const builtin = @import("builtin");

extern fn cubyz_android_home_path() [*:0]const u8;
extern fn cubyz_android_cwd_path() [*:0]const u8;
extern fn cubyz_android_pump_events() void;

extern fn __android_log_write(prio: c_int, tag: [*:0]const u8, text: [*:0]const u8) c_int;

/// Writable private data dir (`$HOME` for the game).
pub fn homePath() []const u8 {
	return std.mem.span(cubyz_android_home_path());
}

/// The app directory the C side chdir'd into; relative asset paths hang off it.
pub fn cwdPath() []const u8 {
	return std.mem.span(cubyz_android_cwd_path());
}

pub const LogPriority = struct {
	pub const verbose: c_int = 2;
	pub const debug: c_int = 3;
	pub const info: c_int = 4;
	pub const warn: c_int = 5;
	pub const @"error": c_int = 6;
};

/// `text` must be NUL-terminated. Used by log.zig's Android sink.
pub fn logWrite(prio: c_int, text: [*:0]const u8) void {
	_ = __android_log_write(prio, "CctesGame", text);
}
