#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vulkan/vulkan.h>
#include <util/log.h>

struct global {
	bool dmabuf_mod;
};

struct global state = {0};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
VkDebugUtilsMessageTypeFlagsEXT messageType, const
VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	fprintf(stderr, "validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL myOutputDebugString(
VkDebugReportFlagsEXT                       flags,
VkDebugReportObjectTypeEXT                  objectType,
uint64_t                                    object,
size_t                                      location,
int32_t                                     messageCode,
const char*                                 pLayerPrefix,
const char*                                 pMessage,
void*                                       pUserData) {
	fprintf(stderr, "myDebug: %s\n", pMessage);
	return VK_FALSE;
}

static PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerProperties = 0;
static PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdProperties = 0;

static int compare_ext_names(const void *a_, const void *b_) {
	const VkExtensionProperties *a = a_;
	const VkExtensionProperties *b = b_;
	return strcmp(a->extensionName, b->extensionName);
}

static int is_ext_name(const void *a_, const void *b_) {
	const VkExtensionProperties *a = a_;
	const char *b = b_;
	return strcmp(a->extensionName, b);
}

bool check_ext(int n_ext, VkExtensionProperties *ext_p, const char *ext_req) {
	qsort(ext_p, n_ext, sizeof(VkExtensionProperties), compare_ext_names);
	void *found = bsearch(ext_req, ext_p, n_ext,
	                    sizeof(VkExtensionProperties), is_ext_name);
	bool ret = found != 0;
	const char *words[] = {"is NOT", "is"};
	errlog("Extension %s %s supported", ext_req, words[ret]);
	return ret;
}

VkInstance create_instance(bool *dmabuf, bool *dmabuf_mod) {
	uint32_t n_ext;
	vkEnumerateInstanceExtensionProperties(NULL, &n_ext, NULL);
	VkExtensionProperties *ext_p = malloc(n_ext*sizeof(*ext_p));
	vkEnumerateInstanceExtensionProperties(NULL, &n_ext, ext_p);

	check_ext(n_ext, ext_p, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	*dmabuf = check_ext(n_ext, ext_p, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	*dmabuf &= check_ext(n_ext, ext_p, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
	*dmabuf_mod = *dmabuf;

	const char *enabled_ext_none[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
	};

	const char *enabled_ext_dmabuf[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME
	};

	uint32_t enabled_ext_count = 0;
	const char **enabled_ext;

	if (*dmabuf) {
		enabled_ext_count = sizeof(enabled_ext_dmabuf)/sizeof(char*);
		enabled_ext = enabled_ext_dmabuf;
	} else {
		enabled_ext_count = sizeof(enabled_ext_none)/sizeof(char*);
		enabled_ext = enabled_ext_none;
	}

/*	VkDebugUtilsMessengerCreateInfoEXT createInfo2 = {0};
	createInfo2.sType =
	VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo2.messageSeverity =
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo2.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo2.pfnUserCallback = debugCallback;*/

	const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
	VkInstanceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = 0,
		.enabledLayerCount = 1,
		.ppEnabledLayerNames = layers,
		.enabledExtensionCount = enabled_ext_count,
		.ppEnabledExtensionNames = enabled_ext
	};
	VkInstance inst;
	if (vkCreateInstance(&info, NULL, &inst)) {
		fprintf(stderr, "ERROR: create_instance() failed.\n");
		return VK_NULL_HANDLE;
	}

/*	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
	(PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(inst,
	"vkCreateDebugUtilsMessengerEXT");
	VkDebugUtilsMessengerEXT debugMessenger;
	vkCreateDebugUtilsMessenger(inst, &createInfo2, 0, &debugMessenger);*/

/*	PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallback =
	(PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(inst,
	"vkCreateDebugReportCallbackEXT");
	VkDebugReportCallbackEXT cb1;
	    VkDebugReportCallbackCreateInfoEXT callback1 = {
            VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,    // sType
            NULL,                                                       // pNext
            VK_DEBUG_REPORT_ERROR_BIT_EXT |                             // flags
            VK_DEBUG_REPORT_WARNING_BIT_EXT,
            myOutputDebugString,                                        // pfnCallback
            NULL                                                        // pUserData
    };
	errlog("function loaded %p", vkCreateDebugReportCallback);
	vkCreateDebugReportCallback(inst, &callback1, NULL, &cb1);*/

	vkGetMemoryHostPointerProperties =
	(PFN_vkGetMemoryHostPointerPropertiesEXT) vkGetInstanceProcAddr(inst,
	"vkGetMemoryHostPointerPropertiesEXT");

	vkGetMemoryFdProperties =
	(PFN_vkGetMemoryFdPropertiesKHR) vkGetInstanceProcAddr(inst,
	"vkGetMemoryFdPropertiesKHR");

	return inst;
}

VkPhysicalDevice get_physical_device(VkInstance inst) {
	uint32_t n = 1;
	VkPhysicalDevice pdev;
	vkEnumeratePhysicalDevices(inst, &n, &pdev);
	if (n == 0) {
		fprintf(stderr, "ERROR: get_physical_device() failed.\n");
		return VK_NULL_HANDLE;
	}
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(pdev, &props);
	errlog("MEMORY NUM %d", props.memoryTypeCount);
	for (uint32_t i=0; i<props.memoryTypeCount; i++) {
		VkMemoryType t = props.memoryTypes[i];
		errlog("MEMORY PROPS %d", t.propertyFlags);
	}
	return pdev;
}

VkDevice create_device(VkInstance inst, VkPhysicalDevice pdev, bool *dmabuf, bool *dmabuf_mod) {
	uint32_t n_ext;
	vkEnumerateDeviceExtensionProperties(pdev, NULL, &n_ext, NULL);
	VkExtensionProperties *ext_p = malloc(n_ext*sizeof(*ext_p));
	vkEnumerateDeviceExtensionProperties(pdev, NULL, &n_ext, ext_p);

	*dmabuf = check_ext(n_ext, ext_p, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
	*dmabuf &= check_ext(n_ext, ext_p, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
	*dmabuf &= check_ext(n_ext, ext_p, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
	*dmabuf_mod = *dmabuf;
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_KHR_MAINTENANCE1_EXTENSION_NAME);
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
	*dmabuf_mod &= check_ext(n_ext, ext_p, VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);

	const char *enabled_ext_dmabuf[] = {
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME
	};

	const char *enabled_ext_dmabuf_mod[] = {
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
		VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
		VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
		VK_KHR_MAINTENANCE1_EXTENSION_NAME,
		VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
		VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME
	};

	uint32_t enabled_ext_count = 0;
	const char **enabled_ext;

	if (*dmabuf_mod) {
		enabled_ext_count = sizeof(enabled_ext_dmabuf_mod)/sizeof(char*);
		enabled_ext = enabled_ext_dmabuf_mod;
	} else if (*dmabuf) {
		enabled_ext_count = sizeof(enabled_ext_dmabuf)/sizeof(char*);
		enabled_ext = enabled_ext_dmabuf;
	}

	float priority = 1.0f;
	VkDeviceQueueCreateInfo infoQueue = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueCount = 1,
		.pQueuePriorities = &priority
	};
	VkDeviceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &infoQueue,
		.enabledExtensionCount = enabled_ext_count,
		.ppEnabledExtensionNames = enabled_ext,
	};
	VkDevice dev;
	if (vkCreateDevice(pdev, &info, NULL, &dev)) {
		fprintf(stderr, "ERROR: create_device() failed.\n");
		return VK_NULL_HANDLE;
	}
	return dev;
}

VkImage create_image(int32_t width, int32_t height, uint32_t stride, uint64_t modifier, VkDevice dev) {
	VkSubresourceLayout layout = {
		.offset = 0,
		.size = 0,
		.rowPitch = stride,
		.arrayPitch = 0,
		.depthPitch = 0,
	};
	VkImageDrmFormatModifierExplicitCreateInfoEXT info_next2 = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
		.drmFormatModifier = modifier,
		.drmFormatModifierPlaneCount = 1,
		.pPlaneLayouts = &layout
	};

	VkImageDrmFormatModifierExplicitCreateInfoEXT *info_next2_p;
	VkImageTiling tiling;
	if (state.dmabuf_mod) {
		info_next2_p = &info_next2;
		tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
	} else {
		info_next2_p = 0;
		tiling = VK_IMAGE_TILING_LINEAR;
	}

	VkExternalMemoryImageCreateInfo info_next = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		.pNext = info_next2_p,
		.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
	};
	// I must convert from DRM to Vk formats
	// I must choose the correct tiling
	// I must choose the correct src/dst
	VkImageCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = &info_next,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = VK_FORMAT_B8G8R8A8_UNORM, // TODO
		.extent = {width, height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = tiling,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT, // or SRC
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//		.queueFamilyIndexCount = 0, ignored
//		.pQueueFamilyIndices = 0, ignored
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	VkImage img;
	if (vkCreateImage(dev, &info, NULL, &img)) {
		fprintf(stderr, "ERROR: create_image() failed.\n");
		return VK_NULL_HANDLE;
	}
	return img;
}

int times = 0;

// Pass stride * height as size
VkDeviceMemory import_memory(int fd, int size, VkDevice dev) {
	VkMemoryFdPropertiesKHR props = {0};
	props.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	if (vkGetMemoryFdProperties(dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, 73641, &props)) {
		errlog("function call failed");
	}
	errlog("memory fd properties %d", props.memoryTypeBits);

	VkImportMemoryFdInfoKHR info_next = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
// or maybe VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
		.fd = fd,
	};
	errlog("size SIZE %d", size);
	VkMemoryAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &info_next,
		.allocationSize = size,
		.memoryTypeIndex = 1
	};
	VkDeviceMemory mem;
	VkResult res = vkAllocateMemory(dev, &info, NULL, &mem);
	if (res) {
		fprintf(stderr, "ERROR: import_memory(%d, %d) failed. %d\n", fd,
		size, res == VK_ERROR_INVALID_EXTERNAL_HANDLE);

		return VK_NULL_HANDLE;
	}
	errlog("IMPORTED %d times", ++times);
	return mem;
}

VkDeviceMemory import_memory_from_shm(uint32_t size, uint8_t *data, VkDevice dev) {
	VkMemoryHostPointerPropertiesEXT props = {.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
	if (vkGetMemoryHostPointerProperties(dev,
	VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, data, &props)) {
		fprintf(stderr, "ERROR: get_properties() failed.\n");
		return VK_NULL_HANDLE;
	}

	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
	errlog("BITMASK: %d\n", props.memoryTypeBits & VK_MEMORY_PROPERTY_PROTECTED_BIT);


	VkImportMemoryHostPointerInfoEXT info_next = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
// or maybe VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT
		.pHostPointer = data
	};
	VkMemoryAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &info_next,
		.allocationSize = size,
		.memoryTypeIndex = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	};
	VkDeviceMemory mem;
	VkResult res = vkAllocateMemory(dev, &info, NULL, &mem);
	if (res) {
		fprintf(stderr, "ERROR: import_memory_from_shm(%d, %p) failed, %d\n",
		size, (void*)data, res == VK_ERROR_INVALID_EXTERNAL_HANDLE);
		return VK_NULL_HANDLE;
	}
	return mem;
}

VkResult bind_image_memory(VkDevice dev, VkImage img, VkDeviceMemory mem) {
	return vkBindImageMemory(dev, img, mem, 0);
}

VkCommandPool create_command_pool(VkDevice dev) {
	VkCommandPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
	};
	VkCommandPool pool;
	if (vkCreateCommandPool(dev, &info, NULL, &pool)) {
		fprintf(stderr, "ERROR: create_command_pool() failed.\n");
		return VK_NULL_HANDLE;
	}
	return pool;
}

VkCommandBuffer record_command_clear(VkDevice dev, VkCommandPool pool, VkImage img) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	VkClearColorValue color = {{0.8984375f, 0.8984375f, 0.9765625f, 1.0f}};
	vkCmdClearColorImage(cmdbuf, img,
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}

int32_t data[2] = {0xFF00FFFF, 0xFF0000FF};

VkCommandBuffer record_command_clear2(VkDevice dev, VkCommandPool pool, VkBuffer buf) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	vkCmdFillBuffer(cmdbuf, buf, 0, VK_WHOLE_SIZE, data[0]);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}

VkCommandBuffer record_command_clear3(VkDevice dev, VkCommandPool pool, VkBuffer buf) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	vkCmdFillBuffer(cmdbuf, buf, 0, VK_WHOLE_SIZE, data[1]);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}

VkCommandBuffer record_command_copy(VkDevice dev, VkCommandPool pool, VkImage
img, VkImage img2, uint32_t width, uint32_t height) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	VkImageSubresourceLayers imageSubresource = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageCopy region = {
		.srcSubresource = imageSubresource,
		.srcOffset = {0,0,0},
		.dstSubresource = imageSubresource,
		.dstOffset = {0,0,0},
		.extent = {width,height,1},
	};
	vkCmdCopyImage(cmdbuf, img,
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img2,
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}

VkCommandBuffer record_command_copy2(VkDevice dev, VkCommandPool pool, VkBuffer
buf, VkBuffer buf2) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	VkBufferCopy region = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = 262144,
	};
	vkCmdCopyBuffer(cmdbuf, buf, buf2, 1, &region);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}

VkCommandBuffer record_command_copy3(VkDevice dev, VkCommandPool pool, VkBuffer
buf, VkImage img2, uint32_t width, uint32_t height) {
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	VkCommandBuffer cmdbuf;
	vkAllocateCommandBuffers(dev, &info, &cmdbuf);

	VkCommandBufferBeginInfo infoBegin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
	};
	vkBeginCommandBuffer(cmdbuf, &infoBegin);

	VkImageSubresourceLayers imageSubresource = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkBufferImageCopy region = {
		.bufferOffset = 0,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource = imageSubresource,
		.imageOffset = {0,0,0},
		.imageExtent = {width,height,1},
	};
	vkCmdCopyBufferToImage(cmdbuf, buf, img2,
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	vkEndCommandBuffer(cmdbuf);
	return cmdbuf;
}



VkPhysicalDevice physical_device;
VkDevice device;
VkQueue queue;
VkCommandPool command_pool;
VkCommandBuffer commands[2] = {0,0};
VkImage screen_image;


int vulkan_init(bool *dmabuf, bool *dmabuf_mod) {
	bool dmabuf_inst, dmabuf_mod_inst;
	VkInstance instance = create_instance(&dmabuf_inst, &dmabuf_mod_inst);
	if (instance == VK_NULL_HANDLE)
		return EXIT_FAILURE;
	printf("Instance created\n");

	physical_device = get_physical_device(instance);
	if (physical_device == VK_NULL_HANDLE)
		return EXIT_FAILURE;
	printf("physical device created\n");

	bool dmabuf_dev, dmabuf_mod_dev;
	device = create_device(instance, physical_device, &dmabuf_dev, &dmabuf_mod_dev);
	if (device == VK_NULL_HANDLE)
		return EXIT_FAILURE;

	*dmabuf = dmabuf_inst && dmabuf_dev;
	*dmabuf_mod = dmabuf_mod_inst && dmabuf_mod_dev;
	state.dmabuf_mod = *dmabuf_mod;

	vkGetDeviceQueue(device, 0, 0, &queue);

	command_pool = create_command_pool(device);
	if (command_pool == VK_NULL_HANDLE)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

	return 0;
        // ERROR
}

VkBuffer create_buffer(uint32_t size, VkDeviceMemory *mem, VkDevice dev) {
	VkBufferCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};
	VkBuffer buf;
	if (vkCreateBuffer(dev, &info, NULL, &buf)) {
		fprintf(stderr, "ERROR: create_buffer() failed.\n");
		return VK_NULL_HANDLE;
	}

	VkMemoryRequirements memreq;
	vkGetBufferMemoryRequirements(dev, buf, &memreq);

	VkMemoryAllocateInfo info2 = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = memreq.size,
		.memoryTypeIndex =
		findMemoryType(memreq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	};
	VkResult res = vkAllocateMemory(dev, &info2, NULL, mem);
	if (res) {
		fprintf(stderr, "ERROR: import_memory failed.\n");
		return VK_NULL_HANDLE;
	}

	vkBindBufferMemory(dev, buf, *mem, 0);

	return buf;
}

void vulkan_render_shm_buffer(uint32_t width, uint32_t height, uint32_t stride,
uint32_t format, uint8_t *data) {
	uint32_t buffer_size = stride*height;
	VkDeviceMemory mem;
	VkBuffer buffer = create_buffer(buffer_size, &mem, device);
	void *dest;
	vkMapMemory(device, mem, 0, buffer_size, 0, &dest);
	errlog("MEMCPY START");
	memcpy(dest, data, (size_t) buffer_size);
	errlog("MEMCPY DONE");
	vkUnmapMemory(device, mem);

	commands[1] = record_command_copy3(device, command_pool, buffer,
	screen_image, width, height);
	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = commands+1
	};
	VkFence fence;
	VkFenceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
		};
	vkCreateFence(device, &info, NULL, &fence);
	errlog("COPY START");
	vkQueueSubmit(queue, 1, &submitInfo, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	errlog("COPY DONE");
	vkResetFences(device, 1, &fence);

	vkDestroyBuffer(device, buffer, NULL);
	vkFreeMemory(device, mem, NULL);
}

int n_fds = 0;
int imported_fds[16];
VkDeviceMemory imported_memory[16];
VkImage imported_imgs[16];

int vulkan_main(int i, int fd, int width, int height, int stride, uint64_t mod) {
//	fd = dup(fd); // otherwise I can't import the same fd again

VkDeviceMemory memory;
	if (i == 1) {
	bool import = true;
	int k;
	errlog("Checking fd %d", fd);
	for (k=0; k<n_fds; k++) {
		errlog("Checking fd %d against fd %d", fd, imported_fds[k]);
		if (fd == imported_fds[k]) {
			errlog("fd %d already imported with index %d", fd, k);
			import = false;
			break;
		}
	}
	if (import) {
		imported_imgs[n_fds] = create_image(width, height, stride, mod, device); //TODO
		memory = import_memory(fd, stride*height, device);
		errlog("Imported fd %d", fd);
		bind_image_memory(device, imported_imgs[n_fds], memory);
		imported_fds[n_fds] = fd;
		imported_memory[n_fds] = memory;
		commands[1] = record_command_copy(device, command_pool, imported_imgs[n_fds],
		screen_image, width, height);
		n_fds++;
	} else {
		errlog("Presenting already imported fd %d", fd);
	}

	} else {
		screen_image = create_image(width, height, stride, mod, device);
		VkDeviceMemory screen_memory = import_memory(fd, stride*height, device);
		bind_image_memory(device, screen_image, screen_memory);
		commands[0] = record_command_clear(device, command_pool, screen_image);
	}

	VkSubmitInfo submitInfo = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = commands+i
	};
	VkFence fence;
	VkFenceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
		};
	vkCreateFence(device, &info, NULL, &fence);
	errlog("COPY START");
	vkQueueSubmit(queue, 1, &submitInfo, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000);
	errlog("COPY DONE");
	vkResetFences(device, 1, &fence);

	if (i == 1) {
//		vkDestroyImage(device, image, NULL);
//		vkFreeMemory(device, memory, NULL);
		}

//	vkFreeCommandBuffers(device, command_pool, 1, commands+1);
//	close(fd);

//	vkFreeCommandBuffers(device, command_pool, 1, commands);

//	vkDestroyCommandPool(device, command_pool, NULL);
//	vkDestroyDevice(device, NULL);
//	vkDestroyInstance(instance, NULL);

	return EXIT_SUCCESS;
}
