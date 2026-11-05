// Runtime stubs for libraries that exist on Android only as link-time symbols.
//
// Cubyz compiles a Vulkan code path and a SPIR-V shader path unconditionally,
// but on Android neither is reachable: the renderer is GLES 3.2 and GL shaders
// are handed to glCompileShader as GLSL source. The functions below therefore
// only need to satisfy the linker. Anything actually called aborts loudly
// rather than misbehaving silently.
#include <glad/vulkan.h>
#include <vk_mem_alloc_extended.h>

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

#include <tinyfiledialogs.h>

#include <android/log.h>
#include <stddef.h>

#define LOG_TAG "CctesStub"
#define STUB_ABORT(name) \
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "unreachable stub called: " #name)

// ------------------------------------------------------------ VMA (Vulkan only)
// VMA wraps Vulkan allocation; with no Vulkan device there is nothing to
// allocate. All entry points return failure.

VkResult vmaCreateAllocator(const VmaAllocatorCreateInfo* pCreateInfo, VmaAllocator* pAllocator) {
	(void)pCreateInfo;
	*pAllocator = NULL;
	return VK_ERROR_INITIALIZATION_FAILED;
}

void vmaDestroyAllocator(VmaAllocator allocator) {
	(void)allocator;
}

VkResult vmaCreateBuffer(VmaAllocator allocator, const VkBufferCreateInfo* pBufferCreateInfo,
                         const VmaAllocationCreateInfo* pAllocationCreateInfo, VkBuffer* pBuffer,
                         VmaAllocation* pAllocation, VmaAllocationInfo* pAllocationInfo) {
	(void)allocator; (void)pBufferCreateInfo; (void)pAllocationCreateInfo;
	(void)pBuffer; (void)pAllocation; (void)pAllocationInfo;
	return VK_ERROR_INITIALIZATION_FAILED;
}

void vmaDestroyBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation) {
	(void)allocator; (void)buffer; (void)allocation;
}

VkResult vmaCreateImage(VmaAllocator allocator, const VkImageCreateInfo* pImageCreateInfo,
                        const VmaAllocationCreateInfo* pAllocationCreateInfo, VkImage* pImage,
                        VmaAllocation* pAllocation, VmaAllocationInfo* pAllocationInfo) {
	(void)allocator; (void)pImageCreateInfo; (void)pAllocationCreateInfo;
	(void)pImage; (void)pAllocation; (void)pAllocationInfo;
	return VK_ERROR_INITIALIZATION_FAILED;
}

void vmaDestroyImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation) {
	(void)allocator; (void)image; (void)allocation;
}

VkResult vmaMapMemory(VmaAllocator allocator, VmaAllocation allocation, void** ppData) {
	(void)allocator; (void)allocation;
	*ppData = NULL;
	return VK_ERROR_MEMORY_MAP_FAILED;
}

void vmaUnmapMemory(VmaAllocator allocator, VmaAllocation allocation) {
	(void)allocator; (void)allocation;
}

// ---------------------------------------------------------- glslang (SPIR-V)
// Unused on Android; see the header comment. Returning a null/none handle keeps
// the caller's error paths (which are all dead code) from dereferencing junk.

glslang_shader_t* glslang_shader_create(const glslang_input_t* input) {
	(void)input;
	return NULL;
}
void glslang_shader_delete(glslang_shader_t* shader) { (void)shader; }
int glslang_shader_preprocess(glslang_shader_t* shader, const glslang_input_t* input) {
	(void)shader; (void)input;
	return 0;
}
int glslang_shader_parse(glslang_shader_t* shader, const glslang_input_t* input) {
	(void)shader; (void)input;
	return 0;
}
const char* glslang_shader_get_info_log(glslang_shader_t* shader) {
	(void)shader;
	return "";
}
const char* glslang_shader_get_info_debug_log(glslang_shader_t* shader) {
	(void)shader;
	return "";
}

glslang_program_t* glslang_program_create(void) { return NULL; }
void glslang_program_delete(glslang_program_t* program) { (void)program; }
void glslang_program_add_shader(glslang_program_t* program, glslang_shader_t* shader) {
	(void)program; (void)shader;
}
int glslang_program_link(glslang_program_t* program, int messages) {
	(void)program; (void)messages;
	return 0;
}
void glslang_program_SPIRV_generate(glslang_program_t* program, glslang_stage_t stage) {
	(void)program; (void)stage;
}
size_t glslang_program_SPIRV_get_size(glslang_program_t* program) {
	(void)program;
	return 0;
}
void glslang_program_SPIRV_get(glslang_program_t* program, unsigned int* spirv) {
	(void)program; (void)spirv;
}
const glslang_resource_t* glslang_default_resource(void) { return NULL; }
int glslang_initialize_process(void) { return 1; }
void glslang_finalize_process(void) {}

// ------------------------------------------------------------ tinyfiledialogs
// File dialogs have no meaning on Android. The engine only reaches these from
// UI actions; return "cancelled" / NULL so nothing breaks if one is hit.

char* tinyfd_openFileDialog(const char* aTitle, const char* aDefaultPathAndFile, int aNumOfFilterPatterns,
                            const char* const* aFilterPatterns, const char* aSingleFilterDescription, int aAllowMultipleSelects) {
	(void)aTitle; (void)aDefaultPathAndFile; (void)aNumOfFilterPatterns;
	(void)aFilterPatterns; (void)aSingleFilterDescription; (void)aAllowMultipleSelects;
	STUB_ABORT(tinyfd_openFileDialog);
	return NULL;
}

char* tinyfd_saveFileDialog(const char* aTitle, const char* aDefaultPathAndFile, int aNumOfFilterPatterns,
                            const char* const* aFilterPatterns, const char* aSingleFilterDescription) {
	(void)aTitle; (void)aDefaultPathAndFile; (void)aNumOfFilterPatterns;
	(void)aFilterPatterns; (void)aSingleFilterDescription;
	STUB_ABORT(tinyfd_saveFileDialog);
	return NULL;
}

int tinyfd_messageBox(const char* aTitle, const char* aMessage, const char* aDialogType,
                      const char* aIconType, int aDefaultButton) {
	(void)aTitle; (void)aMessage; (void)aDialogType; (void)aIconType; (void)aDefaultButton;
	return 0;
}

char* tinyfd_inputBox(const char* aTitle, const char* aMessage, const char* aDefaultInput) {
	(void)aTitle; (void)aMessage; (void)aDefaultInput;
	return NULL;
}

int tinyfd_notifyPopup(const char* aTitle, const char* aMessage, const char* aIconType) {
	(void)aTitle; (void)aMessage; (void)aIconType;
	return 0;
}
