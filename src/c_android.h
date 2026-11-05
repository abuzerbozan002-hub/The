// Android equivalent of src/c.h.
//
// The desktop header pulls in GLFW, glad (desktop GL 4.6 + Vulkan dispatch),
// and the Vulkan loader/allocator. On Android the platform layer is EGL + GLES
// 3.2 (see src/android/), Vulkan is never used at runtime, and the GL entry
// points resolve at link time against libGLESv3. So the GL/GLFW/Vulkan half is
// swapped for the Android shims below; everything else (fonts, images, audio,
// networking, crypto, models) uses the same C libraries as the desktop build.

// ---- text rendering and layouting (unchanged) ----
#include <freetype/ftadvanc.h>
#include <freetype/ftbbox.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftcolor.h>
#include <freetype/ftlcdfil.h>
#include <freetype/ftsizes.h>
#include <freetype/ftstroke.h>
#include <freetype/fttrigon.h>
#include <freetype/ftsynth.h>
#include <hb.h>
#include <hb-ft.h>

// ---- rendering: OpenGL ES 3.2 + a glad-compatible shim ----
// The shim header pulls in <GLES3/gl32.h> and the desktop-enum/shim layer.
#include <glad/gl.h>

// ---- Vulkan: types only ----
// Cubyz keeps a `vulkanTestingMode` code path compiled in. It is never taken on
// Android, but the types are referenced from graphics code, so the headers must
// be present. glad's Vulkan header is used (not the NDK's older one) so the
// type/version set matches the desktop build; the functions themselves are
// stubbed in vulkan_stubs.c.
//
// This must come before <GLFW/glfw3.h>: declaring VK_VERSION_1_0 first is what
// makes the GLFW shim expose glfwCreateWindowSurface, exactly as the desktop
// c.h does by including the Vulkan header before GLFW.
#include <glad/vulkan.h>
#include <vk_mem_alloc_extended.h>

// ---- windowing, input, and the platform plumbing ----
#include <GLFW/glfw3.h>

// ---- image and model loading (unchanged) ----
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <cgltf.h>

// ---- TLS / networking (unchanged) ----
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>

// ---- Android platform APIs used by the shim layer ----
#include <android/log.h>
#include <android/native_window.h>

// ---- file monitoring (same POSIX headers as the desktop Linux build) ----
// Android is Linux underneath and bionic provides inotify, ioctl and unistd, so
// src/utils/file_monitor.zig works unmodified.
//
// BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD disables bionic's second ioctl
// declaration, which uses clang's __overloadable/__enable_if attributes —
// constructs zig's translate-c cannot parse.
#define BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

// ---- audio (unchanged) ----
#include <miniaudio.h>
#define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.h>

// ---- shader compilation to SPIR-V ----
// Only the Vulkan path uses glslang; the GL path feeds GLSL straight to
// glCompileShader. The declarations are kept so the Vulkan code compiles, and
// the implementations are stubbed out on Android.
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

// ---- file save/open dialog ----
// Not available on Android; the engine only reaches this through a UI button.
#include <tinyfiledialogs.h>
