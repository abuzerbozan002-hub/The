// OpenGL ES 3.2 compatibility layer for Cubyz's desktop GL 4.6 renderer.
//
// Include this *after* <GLES3/gl32.h>. Cubyz uses 166 distinct GL entry points;
// 159 of them exist verbatim in GLES 3.2. This header supplies the remaining
// ones: a handful of shim functions implemented in terms of GLES entry points,
// plus the desktop-only enums the engine references by name.
//
// The shims deliberately cover the exact call shapes Cubyz uses (see the call
// sites noted per function) rather than being general-purpose replacements.
#ifndef CUBYZ_GLES_COMPAT_H
#define CUBYZ_GLES_COMPAT_H

#include <GLES3/gl32.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------- constants
// Enums the renderer refers to that GLES headers do not define. Values match
// the desktop GL registry so any code that serialises them stays consistent.

// Polygon rasterization modes (glPolygonMode). GLES only supports FILL, but the
// engine passes these through its RasterizationState, so they must exist.
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02

// Polygon offset modes (glPolygonOffset's target, used in RasterizationState).
#define GL_POLYGON_OFFSET_POINT 0x2A01
#define GL_POLYGON_OFFSET_LINE 0x2A02

// glClipControl enums. GLES has no equivalent; see the shim below.
#define GL_UPPER_LEFT 0x8CA2
#define GL_ZERO_TO_ONE 0x935F

// Immutable buffer storage (glBufferStorage). GLES 3.2 has no immutable
// storage; the shim degrades to glBufferData.
#define GL_DYNAMIC_STORAGE_BIT 0x0100

// Depth clamp. GLES has no GL_DEPTH_CLAMP.
#define GL_DEPTH_CLAMP 0x864F

// ARB_texture_filter_anisotropic. The value is the same as the EXT enum, but
// gl32.h does not pull in gl2ext.h, so spell it out rather than aliasing.
#ifndef GL_TEXTURE_MAX_ANISOTROPY
#define GL_TEXTURE_MAX_ANISOTROPY 0x84FE
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MIRROR_CLAMP_TO_EDGE
#define GL_MIRROR_CLAMP_TO_EDGE 0x8743
#endif

// Texture LOD bias (GL_TEXTURE_LOD_BIAS). Android exposes it as an EXT enum.
#ifndef GL_TEXTURE_LOD_BIAS
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif
// Cube-map seamless filtering hint; irrelevant on GLES, but referenced.
#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif

// Dual-source blending (glBlendFuncSeparate's SRC1 terms) — GLES lacks
// dual-source blending, but the engine names these when building blend state.
#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_SRC1_ALPHA 0x8589
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB

// GPU timer query. GLES timer queries are extension-only; keep the enum so the
// engine's query bookkeeping compiles.
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_TIME_ELAPSED_EXT
#define GL_TIME_ELAPSED_EXT 0x88BF
#endif

// ------------------------------------------------------------ shim functions

// GLES has no glBufferStorage. Emulate immutable storage with a plain data
// store; the engine never relies on immutability, only on the contents and on
// subsequent map/unmap, both of which glBufferData supports.
//   call sites: graphics.zig SSBO initStatic/initStaticSize/createBuffer
static inline void cubyz_glBufferStorage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
	(void)flags;
	glBufferData(target, size, data, GL_DYNAMIC_DRAW);
}

// GLES has no glClipControl: its window-space origin is always lower-left and
// depth is always [0,1]. Cubyz asks for upper-left + zero-to-one. The
// zero-to-one depth half already matches; the origin flip is applied in the
// UI/screen-space shaders instead. No-op here.
//   call site: Window.zig init()
static inline void cubyz_glClipControl(GLenum origin, GLenum depth) {
	(void)origin;
	(void)depth;
}

// Desktop glDepthRange takes doubles; GLES offers glDepthRangef. GLES has no
// GLdouble, so the desktop `double` is spelled out directly.
//   call sites: renderer.zig depth range changes
static inline void cubyz_glDepthRange(double nearZ, double farZ) {
	glDepthRangef((GLfloat)nearZ, (GLfloat)farZ);
}

// GLES 3.2 provides glDrawElementsInstancedBaseVertex but not the ...BaseInstance
// variant. Cubyz's chunk draw uses baseInstance to index a per-chunk data table
// via gl_BaseInstance; the ES shader path was ported to read gl_InstanceID
// instead (see chunks/chunk_vertex.vert), so baseInstance can be dropped here.
//   call site: graphics.zig drawChunkInstance
static inline void cubyz_glDrawElementsInstancedBaseVertexBaseInstance(GLenum mode, GLsizei count, GLenum type,
                                                                       const void* indices, GLsizei instancecount,
                                                                       GLint basevertex, GLuint baseinstance) {
	(void)baseinstance;
	glDrawElementsInstancedBaseVertex(mode, count, type, indices, instancecount, basevertex);
}

// glMultiDrawElementsIndirect is available on Android only as the EXT variant.
//   call sites: renderer/chunk_meshing.zig indirect chunk draws
static inline void cubyz_glMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect,
                                                     GLsizei drawcount, GLsizei stride) {
#ifdef GL_EXT_multi_draw_indirect
	glMultiDrawElementsIndirectEXT(mode, type, indirect, drawcount, stride);
#else
	// Fallback: one draw per command. The indirect struct layout matches
	// DrawElementsIndirectCommand (count, instanceCount, firstIndex,
	// baseVertex, baseInstance).
	const struct {
		GLuint count;
		GLuint instanceCount;
		GLuint firstIndex;
		GLint baseVertex;
		GLuint baseInstance;
	}* cmds = (const void*)indirect;
	const GLsizei step = stride ? stride : (GLsizei)sizeof(*cmds);
	for (GLsizei i = 0; i < drawcount; ++i) {
		const unsigned char* p = (const unsigned char*)cmds + (size_t)i * (size_t)step;
		const void* indices = (const void*)(uintptr_t)(((const GLuint*)p)[2] * 4u); // assumes GL_UNSIGNED_INT
		glDrawElementsInstancedBaseVertex(mode, (GLsizei)((const GLuint*)p)[0], GL_UNSIGNED_INT,
		                                  indices, (GLsizei)((const GLuint*)p)[1], ((const GLint*)p)[3]);
		(void)type;
	}
#endif
}

// GLES only rasterises filled polygons; line/point modes are accepted and
// ignored so the pipeline state code path stays uniform.
//   call site: graphics/pipelines.zig raster state setup
static inline void cubyz_glPolygonMode(GLenum face, GLenum mode) {
	(void)face;
	(void)mode;
}

// glTextureBarrier is desktop-only. The closest GLES equivalent is a memory
// barrier covering texture reads and framebuffer writes, which is what the
// engine needs when a pass samples a texture a previous pass rendered.
//   call site: renderer.zig between render passes
static inline void cubyz_glTextureBarrier(void) {
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
	                GL_SHADER_STORAGE_BARRIER_BIT);
}

// Map the desktop names onto the shims above so engine code compiles unchanged.
#define glBufferStorage cubyz_glBufferStorage
#define glClipControl cubyz_glClipControl
#define glDepthRange cubyz_glDepthRange
#define glDrawElementsInstancedBaseVertexBaseInstance cubyz_glDrawElementsInstancedBaseVertexBaseInstance
#define glMultiDrawElementsIndirect cubyz_glMultiDrawElementsIndirect
#define glPolygonMode cubyz_glPolygonMode
#define glTextureBarrier cubyz_glTextureBarrier

#ifdef __cplusplus
}
#endif

#endif
