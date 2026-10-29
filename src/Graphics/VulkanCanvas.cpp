#include <iiGUI/VulkanCanvas.h>
#include <iiGUI/VulkanWindow.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/Image.h>
#include <iiGUI/GpuEffects.h>
#include <iiGUI/VulkanShaders.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <optional>
#include <span>

namespace ii::gui::detail {
namespace {
constexpr std::uint64_t gpu_timeout=5'000'000'000;
void checked(VkResult result,const char* operation) {
    if (result!=VK_SUCCESS) throw std::runtime_error(std::string(operation)+" (Vulkan "+std::to_string(result)+")");
}
bool extension(std::span<const VkExtensionProperties> extensions,const char* name) {
    return std::any_of(extensions.begin(),extensions.end(),[&](const auto& e) { return std::strcmp(e.extensionName,name)==0; });
}
struct Buffer final {
    VkDevice device;
    VkBuffer handle=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void* mapped=nullptr;
    VkDeviceSize capacity=0;
    explicit Buffer(VkDevice d):device(d) {}
    ~Buffer() {
        if (mapped) vkUnmapMemory(device,memory);
        if (handle) vkDestroyBuffer(device,handle,nullptr);
        if (memory) vkFreeMemory(device,memory,nullptr);
    }
};
struct Image final {
    VkDevice device;
    VkImage handle=VK_NULL_HANDLE;
    VkImageView view=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    explicit Image(VkDevice d):device(d) {}
    ~Image() {
        if (view) vkDestroyImageView(device,view,nullptr);
        if (handle) vkDestroyImage(device,handle,nullptr);
        if (memory) vkFreeMemory(device,memory,nullptr);
    }
};
}

struct VulkanCanvas::Impl final {
    std::unique_ptr<VulkanWindow> window;
    VkInstance instance=VK_NULL_HANDLE;
    VkSurfaceKHR surface=VK_NULL_HANDLE;
    VkPhysicalDevice physical=VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memory{};
    VkPhysicalDeviceLimits limits{};
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    std::uint32_t family=0;
    VkCommandPool command_pool=VK_NULL_HANDLE;
    VkCommandBuffer command=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    bool pending=false;
    VkSemaphore available=VK_NULL_HANDLE;
    VkSwapchainKHR swapchain=VK_NULL_HANDLE;
    VkExtent2D swap_extent{};
    std::vector<VkImage> swap_images;
    std::vector<VkSemaphore> presented;
    bool swap_invalid=false;
    VkRenderPass render_pass=VK_NULL_HANDLE,load_pass=VK_NULL_HANDLE;
    static constexpr std::size_t MaximumLayers=65;
    VkFramebuffer framebuffer=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout=VK_NULL_HANDLE;
    VkDescriptorPool descriptors=VK_NULL_HANDLE;
    std::array<VkDescriptorSet,MaximumLayers*4> descriptor{};
    VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;
    VkPipeline panels_pipeline=VK_NULL_HANDLE,text_pipeline=VK_NULL_HANDLE,effect_pipeline=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    std::unique_ptr<Image> color,effect_content,effect_current;
    bool content_initialized=false,current_initialized=false;
    struct Layer final {
        std::unique_ptr<Image> glyphs;
        std::unique_ptr<Buffer> panels,glyph_upload;
        ForegroundKey key;
        PixelClip region{};
        std::size_t panel_count=0;
        bool glyph_initialized=false,upload=false,foreground=false;
    };
    std::vector<Layer> layers;
    struct EffectAsset{std::shared_ptr<const RasterImage> source;std::unique_ptr<Image> texture;std::unique_ptr<Buffer> upload;bool pending=true;};
    std::vector<EffectAsset> effect_assets;
    VkImageView effect_image(std::shared_ptr<const RasterImage> source){
        for(const auto& asset:effect_assets)if(asset.source==source)return asset.texture->view;
        const auto width=source?source->width():1,height=source?source->height():1;
        auto pixels=image(width,height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);auto upload=buffer(std::size_t(width)*height*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        if(source)std::memcpy(upload->mapped,source->rgba().data(),source->rgba().size());else std::memset(upload->mapped,0,4);
        effect_assets.push_back({source,std::move(pixels),std::move(upload),true});++statistics.effect_image_uploads;return effect_assets.back().texture->view;
    }

    std::optional<CanvasFrame> previous;
    double previous_scale=0;
    GraphicsInfo statistics;

    ~Impl() {
        if (device) {
            (void)vkDeviceWaitIdle(device);
            clear_swapchain();
            if (framebuffer) vkDestroyFramebuffer(device,framebuffer,nullptr);
            if (panels_pipeline) vkDestroyPipeline(device,panels_pipeline,nullptr);
            if (text_pipeline) vkDestroyPipeline(device,text_pipeline,nullptr);
            if(effect_pipeline)vkDestroyPipeline(device,effect_pipeline,nullptr);
            if (pipeline_layout) vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
            if (descriptors) vkDestroyDescriptorPool(device,descriptors,nullptr);
            if (descriptor_layout) vkDestroyDescriptorSetLayout(device,descriptor_layout,nullptr);
            if (sampler) vkDestroySampler(device,sampler,nullptr);
            if (render_pass) vkDestroyRenderPass(device,render_pass,nullptr);
            if(load_pass)vkDestroyRenderPass(device,load_pass,nullptr);
            if (command_pool) vkDestroyCommandPool(device,command_pool,nullptr);
            if (fence) vkDestroyFence(device,fence,nullptr);
            if (available) vkDestroySemaphore(device,available,nullptr);
            color.reset();effect_content.reset();effect_current.reset(); layers.clear();effect_assets.clear();
            vkDestroyDevice(device,nullptr);
        }
        if (surface) vkDestroySurfaceKHR(instance,surface,nullptr);
        window.reset();
        if (instance) vkDestroyInstance(instance,nullptr);
    }
    std::uint32_t memory_type(std::uint32_t bits,VkMemoryPropertyFlags flags) const {
        for (std::uint32_t i=0;i<memory.memoryTypeCount;++i)
            if ((bits&(1u<<i)) && (memory.memoryTypes[i].propertyFlags&flags)==flags) return i;
        throw std::runtime_error("Vulkan device has no compatible memory type");
    }
    std::unique_ptr<Buffer> buffer(VkDeviceSize size,VkBufferUsageFlags usage) {
        if ((usage&VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) && size>limits.maxStorageBufferRange)
            throw std::runtime_error("Canvas exceeds this Vulkan device's storage buffer limit");
        auto result=std::make_unique<Buffer>(device); result->capacity=size;
        VkBufferCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size=size; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateBuffer(device,&info,nullptr,&result->handle),"Cannot create Vulkan buffer");
        VkMemoryRequirements requirements{}; vkGetBufferMemoryRequirements(device,result->handle,&requirements);
        VkMemoryAllocateInfo allocation{}; allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        checked(vkAllocateMemory(device,&allocation,nullptr,&result->memory),"Cannot allocate Vulkan buffer memory");
        checked(vkBindBufferMemory(device,result->handle,result->memory,0),"Cannot bind Vulkan buffer");
        checked(vkMapMemory(device,result->memory,0,size,0,&result->mapped),"Cannot map Vulkan buffer");
        return result;
    }
    std::unique_ptr<Image> image(std::uint32_t width,std::uint32_t height,VkImageUsageFlags usage) {
        VkImageFormatProperties supported{};
        checked(vkGetPhysicalDeviceImageFormatProperties(physical,VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&supported),"Vulkan device cannot allocate the required sRGB image usage");
        if (width>supported.maxExtent.width || height>supported.maxExtent.height)
            throw std::runtime_error("Canvas exceeds this Vulkan device's sRGB image dimensions");
        auto result=std::make_unique<Image>(device);
        VkImageCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType=VK_IMAGE_TYPE_2D; info.format=VK_FORMAT_R8G8B8A8_SRGB;
        info.extent={width,height,1}; info.mipLevels=info.arrayLayers=1; info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.tiling=VK_IMAGE_TILING_OPTIMAL; info.usage=usage; info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(vkCreateImage(device,&info,nullptr,&result->handle),"Cannot create Vulkan sRGB image");
        VkMemoryRequirements requirements{}; vkGetImageMemoryRequirements(device,result->handle,&requirements);
        VkMemoryAllocateInfo allocation{}; allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocation.allocationSize=requirements.size;
        allocation.memoryTypeIndex=memory_type(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        checked(vkAllocateMemory(device,&allocation,nullptr,&result->memory),"Cannot allocate Vulkan image memory");
        checked(vkBindImageMemory(device,result->handle,result->memory,0),"Cannot bind Vulkan image");
        VkImageViewCreateInfo view{}; view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image=result->handle; view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=info.format;
        view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        checked(vkCreateImageView(device,&view,nullptr,&result->view),"Cannot view Vulkan image");
        return result;
    }
    void initialize(void* parent,void* display,Acceleration acceleration) {
        std::uint32_t count=0;
        checked(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr),"Cannot enumerate Vulkan instance extensions");
        std::vector<VkExtensionProperties> supported(count);
        checked(vkEnumerateInstanceExtensionProperties(nullptr,&count,supported.data()),"Cannot read Vulkan instance extensions");
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME,VulkanWindow::extension()};
        for (auto name:extensions) if (!extension(supported,name)) throw std::runtime_error(std::string("Vulkan instance lacks ")+name);
        VkInstanceCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        VkApplicationInfo app{}; app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName="iiGUI"; app.pEngineName="iiGUI"; app.apiVersion=VK_API_VERSION_1_1;
        info.pApplicationInfo=&app;
        if (extension(supported,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            info.flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        info.enabledExtensionCount=std::uint32_t(extensions.size()); info.ppEnabledExtensionNames=extensions.data();
        checked(vkCreateInstance(&info,nullptr,&instance),"Cannot create Vulkan instance");
        window=std::make_unique<VulkanWindow>(parent,display); surface=window->surface(instance);
        choose_device(acceleration);
        VkCommandPoolCreateInfo pool{}; pool.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.queueFamilyIndex=family; pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        checked(vkCreateCommandPool(device,&pool,nullptr,&command_pool),"Cannot create Vulkan command pool");
        VkCommandBufferAllocateInfo commands{}; commands.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commands.commandPool=command_pool; commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; commands.commandBufferCount=1;
        checked(vkAllocateCommandBuffers(device,&commands,&command),"Cannot allocate Vulkan command buffer");
        VkFenceCreateInfo fence_info{}; fence_info.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        checked(vkCreateFence(device,&fence_info,nullptr,&fence),"Cannot create Vulkan fence");
        VkSemaphoreCreateInfo semaphore{}; semaphore.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        checked(vkCreateSemaphore(device,&semaphore,nullptr,&available),"Cannot create Vulkan acquire semaphore");
        create_pipelines();
    }
    void choose_device(Acceleration acceleration) {
        std::uint32_t count=0; checked(vkEnumeratePhysicalDevices(instance,&count,nullptr),"Cannot enumerate Vulkan devices");
        std::vector<VkPhysicalDevice> devices(count);
        checked(vkEnumeratePhysicalDevices(instance,&count,devices.data()),"Cannot read Vulkan devices");
        int best=-1;
        for (auto candidate:devices) {
            VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(candidate,&properties);
            const bool hardware=properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            if (acceleration==Acceleration::require_hardware && !hardware) continue;
            std::uint32_t n=0; checked(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&n,nullptr),"Cannot enumerate Vulkan device extensions");
            std::vector<VkExtensionProperties> extensions(n);
            checked(vkEnumerateDeviceExtensionProperties(candidate,nullptr,&n,extensions.data()),"Cannot read Vulkan device extensions");
            if (!extension(extensions,VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
            VkFormatProperties format{}; vkGetPhysicalDeviceFormatProperties(candidate,VK_FORMAT_R8G8B8A8_SRGB,&format);
            constexpr auto needed=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT|
                VK_FORMAT_FEATURE_BLIT_SRC_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            if ((format.optimalTilingFeatures&needed)!=needed) continue;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,nullptr);
            std::vector<VkQueueFamilyProperties> queues(n); vkGetPhysicalDeviceQueueFamilyProperties(candidate,&n,queues.data());
            for (std::uint32_t i=0;i<n;++i) {
                VkBool32 present=VK_FALSE;
                checked(vkGetPhysicalDeviceSurfaceSupportKHR(candidate,i,surface,&present),"Cannot query Vulkan native presentation");
                const int score=hardware?100:(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU?0:10);
                if (queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT) && present && score>best) {
                    best=score; physical=candidate; family=i;
                    statistics.adapter=properties.deviceName; statistics.hardware=hardware;
                }
            }
        }
        if (!physical) throw std::runtime_error("No Vulkan graphics/presentation device satisfies the acceleration policy");
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical,&properties); limits=properties.limits;
        std::uint32_t n=0; checked(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,nullptr),"Cannot query selected Vulkan device");
        std::vector<VkExtensionProperties> supported(n);
        checked(vkEnumerateDeviceExtensionProperties(physical,nullptr,&n,supported.data()),"Cannot read selected Vulkan device");
        std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        if (extension(supported,"VK_KHR_portability_subset")) extensions.push_back("VK_KHR_portability_subset");
        float priority=1;
        VkDeviceQueueCreateInfo q{}; q.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        q.queueFamilyIndex=family; q.queueCount=1; q.pQueuePriorities=&priority;
        VkDeviceCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount=1; info.pQueueCreateInfos=&q;
        info.enabledExtensionCount=std::uint32_t(extensions.size()); info.ppEnabledExtensionNames=extensions.data();
        checked(vkCreateDevice(physical,&info,nullptr,&device),"Cannot create Vulkan graphics device");
        vkGetDeviceQueue(device,family,0,&queue); vkGetPhysicalDeviceMemoryProperties(physical,&memory);
        statistics.api=GraphicsApi::vulkan; statistics.srgb_attachment=true;statistics.backdrop_supported=statistics.shadow_supported=statistics.content_transform_supported=true;
    }
    VkShaderModule shader(std::span<const std::uint32_t> code) {
        VkShaderModuleCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize=code.size_bytes(); info.pCode=code.data(); VkShaderModule result=VK_NULL_HANDLE;
        checked(vkCreateShaderModule(device,&info,nullptr,&result),"Cannot create Vulkan shader"); return result;
    }
    VkPipeline pipeline(std::span<const std::uint32_t> vertex,std::span<const std::uint32_t> fragment) {
        auto vs=shader(vertex); VkShaderModule fs=VK_NULL_HANDLE; VkPipeline result=VK_NULL_HANDLE;
        try {
            fs=shader(fragment);
            const VkPipelineShaderStageCreateInfo stages[]{
                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_VERTEX_BIT,vs,"main",nullptr},
                {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_FRAGMENT_BIT,fs,"main",nullptr}};
            VkPipelineVertexInputStateCreateInfo vertices{}; vertices.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            VkPipelineInputAssemblyStateCreateInfo assembly{}; assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{}; viewport.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewport.viewportCount=viewport.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo raster{}; raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode=VK_POLYGON_MODE_FILL; raster.cullMode=VK_CULL_MODE_NONE; raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE; raster.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo samples{}; samples.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            samples.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineColorBlendAttachmentState attachment{}; attachment.blendEnable=VK_TRUE;
            attachment.srcColorBlendFactor=attachment.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
            attachment.dstColorBlendFactor=attachment.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp=attachment.alphaBlendOp=VK_BLEND_OP_ADD;
            attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blend{}; blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount=1; blend.pAttachments=&attachment;
            const VkDynamicState dynamic_states[]{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{}; dynamic.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.dynamicStateCount=2; dynamic.pDynamicStates=dynamic_states;
            VkGraphicsPipelineCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            info.stageCount=2; info.pStages=stages; info.pVertexInputState=&vertices; info.pInputAssemblyState=&assembly;
            info.pViewportState=&viewport; info.pRasterizationState=&raster; info.pMultisampleState=&samples;
            info.pColorBlendState=&blend; info.pDynamicState=&dynamic; info.layout=pipeline_layout; info.renderPass=render_pass;
            checked(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&result),"Cannot create Vulkan graphics pipeline");
        } catch (...) { if (fs) vkDestroyShaderModule(device,fs,nullptr); vkDestroyShaderModule(device,vs,nullptr); throw; }
        vkDestroyShaderModule(device,fs,nullptr); vkDestroyShaderModule(device,vs,nullptr); return result;
    }
    void create_pipelines() {
        VkAttachmentDescription color_attachment{}; color_attachment.format=VK_FORMAT_R8G8B8A8_SRGB;
        color_attachment.samples=VK_SAMPLE_COUNT_1_BIT; color_attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE; color_attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference attachment{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{}; subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount=1; subpass.pColorAttachments=&attachment;
        VkSubpassDependency dependencies[]{
            {VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,0},
            {0,VK_SUBPASS_EXTERNAL,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,0}};
        VkRenderPassCreateInfo pass{}; pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        pass.attachmentCount=1; pass.pAttachments=&color_attachment; pass.subpassCount=1; pass.pSubpasses=&subpass;
        pass.dependencyCount=2; pass.pDependencies=dependencies;
        checked(vkCreateRenderPass(device,&pass,nullptr,&render_pass),"Cannot create Vulkan render pass");
        color_attachment.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;color_attachment.initialLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        checked(vkCreateRenderPass(device,&pass,nullptr,&load_pass),"Cannot create retained Vulkan render pass");
        VkDescriptorSetLayoutBinding bindings[]{
            {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {2,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo layout{}; layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount=4; layout.pBindings=bindings;
        checked(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptor_layout),"Cannot create Vulkan descriptors");
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(GpuEffect)};
        VkPipelineLayoutCreateInfo pipeline_info{}; pipeline_info.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_info.setLayoutCount=1; pipeline_info.pSetLayouts=&descriptor_layout;
        pipeline_info.pushConstantRangeCount=1; pipeline_info.pPushConstantRanges=&push;
        checked(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&pipeline_layout),"Cannot create Vulkan pipeline layout");
        const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,MaximumLayers*4},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,MaximumLayers*12}};
        VkDescriptorPoolCreateInfo pool{}; pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets=MaximumLayers*4; pool.poolSizeCount=2; pool.pPoolSizes=sizes;
        checked(vkCreateDescriptorPool(device,&pool,nullptr,&descriptors),"Cannot create Vulkan descriptor pool");
        VkDescriptorSetAllocateInfo allocation{}; allocation.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        std::array<VkDescriptorSetLayout,MaximumLayers*4> layouts; layouts.fill(descriptor_layout);
        allocation.descriptorPool=descriptors; allocation.descriptorSetCount=MaximumLayers*4; allocation.pSetLayouts=layouts.data();
        checked(vkAllocateDescriptorSets(device,&allocation,descriptor.data()),"Cannot allocate Vulkan layer descriptor sets");
        VkSamplerCreateInfo sampling{}; sampling.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampling.magFilter=sampling.minFilter=VK_FILTER_LINEAR; sampling.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampling.addressModeU=sampling.addressModeV=sampling.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampling.maxLod=0;
        checked(vkCreateSampler(device,&sampling,nullptr,&sampler),"Cannot create Vulkan text sampler");
        panels_pipeline=pipeline(panels_vert,panels_frag); text_pipeline=pipeline(text_vert,text_frag);effect_pipeline=pipeline(text_vert,effect_frag);
    }
    void clear_swapchain() {
        for (auto semaphore:presented) if (semaphore) vkDestroySemaphore(device,semaphore,nullptr);
        presented.clear(); swap_images.clear();
        if (swapchain) vkDestroySwapchainKHR(device,swapchain,nullptr);
        swapchain=VK_NULL_HANDLE; swap_extent={};
    }
    bool ensure_swapchain(std::uint32_t width,std::uint32_t height) {
        VkSurfaceCapabilitiesKHR capabilities{};
        checked(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&capabilities),"Cannot read Vulkan surface capabilities");
        auto extent=capabilities.currentExtent;
        if (extent.width==std::numeric_limits<std::uint32_t>::max()) {
            extent={std::clamp(width,capabilities.minImageExtent.width,capabilities.maxImageExtent.width),
                std::clamp(height,capabilities.minImageExtent.height,capabilities.maxImageExtent.height)};
        }
        if (!extent.width || !extent.height) return false;
        if (swapchain && !swap_invalid && swap_extent.width==extent.width && swap_extent.height==extent.height) return true;
        if (!(capabilities.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)) throw std::runtime_error("Vulkan surface cannot receive native GPU blits");
        checked(vkDeviceWaitIdle(device),"Cannot drain Vulkan presentation before resize"); pending=false;
        std::uint32_t count=0; checked(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,nullptr),"Cannot enumerate Vulkan surface formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        checked(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats.data()),"Cannot read Vulkan surface formats");
        std::optional<VkSurfaceFormatKHR> format;
        for (auto f:formats) if (f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
            (f.format==VK_FORMAT_B8G8R8A8_SRGB || f.format==VK_FORMAT_R8G8B8A8_SRGB || f.format==VK_FORMAT_UNDEFINED)) {
            if (f.format==VK_FORMAT_UNDEFINED) f.format=VK_FORMAT_B8G8R8A8_SRGB;
            format=f; break;
        }
        if (!format) throw std::runtime_error("Vulkan native surface has no sRGB swapchain format");
        VkFormatProperties properties{}; vkGetPhysicalDeviceFormatProperties(physical,format->format,&properties);
        if (!(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_BLIT_DST_BIT)) throw std::runtime_error("Vulkan swapchain format cannot receive blits");
        auto images=capabilities.minImageCount+1;
        if (capabilities.maxImageCount) images=std::min(images,capabilities.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for (auto candidate:{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR})
            if (capabilities.supportedCompositeAlpha&candidate) { alpha=candidate; break; }
        VkSwapchainCreateInfoKHR info{}; info.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface=surface; info.minImageCount=images; info.imageFormat=format->format; info.imageColorSpace=format->colorSpace;
        info.imageExtent=extent; info.imageArrayLayers=1; info.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE; info.preTransform=capabilities.currentTransform;
        info.compositeAlpha=alpha; info.presentMode=VK_PRESENT_MODE_FIFO_KHR; info.clipped=VK_TRUE; info.oldSwapchain=swapchain;
        VkSwapchainKHR replacement=VK_NULL_HANDLE;
        checked(vkCreateSwapchainKHR(device,&info,nullptr,&replacement),"Cannot create native Vulkan swapchain");
        clear_swapchain(); swapchain=replacement; swap_extent=extent; swap_invalid=false;
        checked(vkGetSwapchainImagesKHR(device,swapchain,&count,nullptr),"Cannot count Vulkan swapchain images");
        swap_images.resize(count); presented.resize(count,VK_NULL_HANDLE);
        checked(vkGetSwapchainImagesKHR(device,swapchain,&count,swap_images.data()),"Cannot read Vulkan swapchain images");
        VkSemaphoreCreateInfo semaphore{}; semaphore.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (auto& handle:presented) checked(vkCreateSemaphore(device,&semaphore,nullptr,&handle),"Cannot create Vulkan presentation semaphore");
        return true;
    }
    void wait() {
        if (!pending) return;
        checked(vkWaitForFences(device,1,&fence,VK_TRUE,gpu_timeout),"Vulkan GPU completion failed or timed out"); pending=false;
    }
    void begin() {
        checked(vkResetCommandPool(device,command_pool,0),"Cannot reset Vulkan commands");
        VkCommandBufferBeginInfo info{}; info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        checked(vkBeginCommandBuffer(command,&info),"Cannot begin Vulkan commands");
    }
    void submit(std::optional<std::uint32_t> acquired) {
        checked(vkEndCommandBuffer(command),"Cannot finish Vulkan commands");
        checked(vkResetFences(device,1,&fence),"Cannot reset Vulkan fence");
        VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo info{}; info.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount=1; info.pCommandBuffers=&command;
        if (acquired) {
            info.waitSemaphoreCount=info.signalSemaphoreCount=1; info.pWaitSemaphores=&available;
            info.pWaitDstStageMask=&stage; info.pSignalSemaphores=&presented[*acquired];
        }
        checked(vkQueueSubmit(queue,1,&info,fence),"Cannot submit Vulkan graphics"); pending=true;
        if (acquired) {
            VkPresentInfoKHR present{}; present.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present.waitSemaphoreCount=1; present.pWaitSemaphores=&presented[*acquired];
            present.swapchainCount=1; present.pSwapchains=&swapchain; present.pImageIndices=&*acquired;
            const auto result=vkQueuePresentKHR(queue,&present);
            if (result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR) swap_invalid=true;
            else checked(result,"Cannot present Vulkan native surface");
            if (result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) ++statistics.presentations;
        }
    }
    void barrier(VkImage image,VkImageLayout old_layout,VkImageLayout layout,
                 VkAccessFlags source_access,VkAccessFlags destination_access,VkPipelineStageFlags source_stage,VkPipelineStageFlags destination_stage) {
        VkImageMemoryBarrier info{}; info.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        info.srcAccessMask=source_access; info.dstAccessMask=destination_access;
        info.oldLayout=old_layout; info.newLayout=layout;
        info.srcQueueFamilyIndex=info.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        info.image=image; info.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,source_stage,destination_stage,0,0,nullptr,0,nullptr,1,&info);
    }
    void draw(const CanvasFrame&,double,double);
    PixelBuffer capture();
};

void VulkanCanvas::Impl::draw(const CanvasFrame& frame,double scale,double parent_height) {
    const auto [width,height]=canvas_pixels(frame,scale);
    wait(); window->resize(frame.viewport,scale,parent_height);
    const bool can_present=ensure_swapchain(width,height);
    bool changed=!previous || *previous!=frame || previous_scale!=scale;
    if (!color || statistics.pixel_width!=width || statistics.pixel_height!=height) {
        if (framebuffer) { vkDestroyFramebuffer(device,framebuffer,nullptr); framebuffer=VK_NULL_HANDLE; }
        color=image(width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        VkFramebufferCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass=render_pass; info.attachmentCount=1; info.pAttachments=&color->view;
        info.width=width; info.height=height; info.layers=1;
        checked(vkCreateFramebuffer(device,&info,nullptr,&framebuffer),"Cannot create Vulkan canvas framebuffer");
        statistics.pixel_width=width; statistics.pixel_height=height;
        effect_content.reset();effect_current.reset();content_initialized=current_initialized=false;
        statistics.cached_texture_bytes=std::uint64_t(width)*height*4; changed=true;
        for(auto& layer:layers) { layer.glyphs.reset();layer.key={};layer.glyph_initialized=false; }
    }
    std::optional<std::uint32_t> acquired;
    if (can_present) {
        std::uint32_t index=0;
        const auto result=vkAcquireNextImageKHR(device,swapchain,1'000'000'000,available,VK_NULL_HANDLE,&index);
        if (result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) {
            acquired=index; if (result==VK_SUBOPTIMAL_KHR) swap_invalid=true;
        } else if (result==VK_ERROR_OUT_OF_DATE_KHR) swap_invalid=true;
        else if (result!=VK_TIMEOUT && result!=VK_NOT_READY) checked(result,"Cannot acquire native Vulkan image");
    }
    if (!changed && !acquired) return;
    const auto views=changed?canvas_layers(frame):std::vector<CanvasLayerView>{};
    if (changed) {
        layers.resize(views.size());statistics.foreground_texture_bytes=0;
        const bool effects=std::ranges::any_of(views,[](const auto& layer){return layer.backdrop||layer.shadow;});
        if(effects&&!effect_content){effect_content=image(width,height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);effect_current=image(width,height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);statistics.cached_texture_bytes+=std::uint64_t(width)*height*8;}
        if(effects)for(std::size_t i=0;i<views.size();++i)for(unsigned bank:{1u,2u,3u}){VkDescriptorImageInfo texture{sampler,bank==2?effect_current->view:effect_content->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};VkWriteDescriptorSet write{};write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.dstSet=descriptor[bank*MaximumLayers+i];write.dstBinding=1;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&texture;vkUpdateDescriptorSets(device,1,&write,0,nullptr);}
        std::vector<std::shared_ptr<const RasterImage>> needed;if(effects)needed.push_back({});
        const auto need=[&](const auto& image){if(image&&std::ranges::find(needed,image)==needed.end())needed.push_back(image);};
        for(const auto& view:views){if(view.shadow)need(view.shadow->mask);if(view.backdrop){need(view.backdrop->mask);need(view.backdrop->underlay);}}
        std::uint64_t effect_bytes=0;for(const auto& image:needed)effect_bytes+=image?image->rgba().size():4;
        statistics.effect_texture_budget_bytes=effect_texture_budget(width,height);if(effect_bytes>statistics.effect_texture_budget_bytes)throw std::runtime_error("Effect image storage exceeds the effect budget");
        std::erase_if(effect_assets,[&](const auto& asset){return std::ranges::find(needed,asset.source)==needed.end();});
        statistics.effect_image_texture_bytes=effect_bytes;
        for(const auto& image:needed)effect_image(image);
        if(effects)for(std::size_t i=0;i<views.size();++i){
            const auto set_images=[&](std::size_t bank,std::shared_ptr<const RasterImage> mask,std::shared_ptr<const RasterImage> underlay){
                const VkDescriptorImageInfo images[]{{sampler,effect_image(mask),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,effect_image(underlay),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
                VkWriteDescriptorSet writes[2]{};for(unsigned index=0;index<2;++index){auto& w=writes[index];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;w.dstSet=descriptor[bank*MaximumLayers+i];w.dstBinding=index+2;w.descriptorCount=1;w.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;w.pImageInfo=&images[index];}vkUpdateDescriptorSets(device,2,writes,0,nullptr);
            };
            // Shadow and backdrop have independent descriptor banks so one draw
            // cannot retroactively change another draw in the command buffer.
            const auto& view=views[i];set_images(3,view.shadow?view.shadow->mask:std::shared_ptr<const RasterImage>{},{});set_images(1,view.backdrop?view.backdrop->mask:std::shared_ptr<const RasterImage>{},view.backdrop?view.backdrop->underlay:std::shared_ptr<const RasterImage>{});set_images(2,view.backdrop?view.backdrop->mask:std::shared_ptr<const RasterImage>{},view.backdrop?view.backdrop->underlay:std::shared_ptr<const RasterImage>{});
        }
        for(std::size_t i=0;i<views.size();++i) {
            auto& layer=layers[i];const auto view=views[i];
            const auto instances=pack_panels(view.panels,view.content_transform);layer.panel_count=instances.size();
            const auto bytes=std::max<std::size_t>(sizeof(GpuPanel),instances.size()*sizeof(GpuPanel));
            if (!layer.panels || layer.panels->capacity<bytes) layer.panels=buffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            if (!instances.empty()) std::memcpy(layer.panels->mapped,instances.data(),instances.size()*sizeof(GpuPanel));
            VkDescriptorBufferInfo input{layer.panels->handle,0,bytes};
            VkWriteDescriptorSet write{}; write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet=descriptor[i]; write.dstBinding=0; write.descriptorCount=1;
            write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; write.pBufferInfo=&input;
            vkUpdateDescriptorSets(device,1,&write,0,nullptr);
            layer.upload=false;layer.foreground=view.foreground();
            if(layer.foreground) {
                if(!layer.glyphs || !layer.key.matches(frame.viewport,scale,view,true)) {
                    auto raster=rasterize_foreground_region(frame.viewport,view,scale,true);const auto& bitmap=raster.pixels;
                    if (!layer.glyphs||layer.region.width!=bitmap.width||layer.region.height!=bitmap.height){layer.glyphs=image(bitmap.width,bitmap.height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);layer.glyph_initialized=false;}layer.region=raster.region;
                    if (!layer.glyph_upload || layer.glyph_upload->capacity<bitmap.rgba.size())
                        layer.glyph_upload=buffer(bitmap.rgba.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    std::memcpy(layer.glyph_upload->mapped,bitmap.rgba.data(),bitmap.rgba.size());
                    layer.upload=true;layer.key.assign(frame.viewport,scale,view,true);
                    ++statistics.foreground_rasterizations;
                }
                VkDescriptorImageInfo text{sampler,layer.glyphs->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                write.dstBinding=1; write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                write.pBufferInfo=nullptr; write.pImageInfo=&text;
                vkUpdateDescriptorSets(device,1,&write,0,nullptr);
                statistics.foreground_texture_bytes+=std::uint64_t(layer.region.width)*layer.region.height*4;
            } else { layer.glyphs.reset();layer.glyph_upload.reset();layer.key={};layer.glyph_initialized=false; }
        }
    }
    begin();
    if (changed) {
        for(auto& asset:effect_assets)if(asset.pending){
            barrier(asset.texture->handle,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={asset.source?asset.source->width():1,asset.source?asset.source->height():1,1};
            vkCmdCopyBufferToImage(command,asset.upload->handle,asset.texture->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(asset.texture->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);asset.pending=false;
        }
        for(auto& layer:layers) if(layer.upload) {
            barrier(layer.glyphs->handle,layer.glyph_initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,layer.glyph_initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,
                layer.glyph_initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={layer.region.width,layer.region.height,1};
            vkCmdCopyBufferToImage(command,layer.glyph_upload->handle,layer.glyphs->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(layer.glyphs->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            layer.glyph_initialized=true;
        }
        const bool has_effects=std::ranges::any_of(views,[](const auto& layer){return layer.backdrop||layer.shadow;});
        const auto snapshot=[&](Image& target,bool& initialized){
            barrier(target.handle,initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={width,height,1};vkCmdCopyImage(command,color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);initialized=true;
        };
        for(std::size_t i=0;i<layers.size();++i){
            if(i&&views[i].backdrop&&views[i].backdrop->source==BackdropSource::previous_layers)snapshot(*effect_current,current_initialized);
            VkClearValue clear{};clear.color={{frame.background.red,frame.background.green,frame.background.blue,1}};
            VkRenderPassBeginInfo pass{};pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;pass.renderPass=i?load_pass:render_pass;pass.framebuffer=framebuffer;pass.renderArea={{0,0},{width,height}};pass.clearValueCount=1;pass.pClearValues=&clear;
            vkCmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{0,0,float(width),float(height),0,1};vkCmdSetViewport(command,0,1,&viewport);
            const auto clip=clip_pixels(views[i],scale,width,height);
            if(clip.width&&clip.height){
                const VkRect2D scissor{{std::int32_t(clip.x),std::int32_t(clip.y)},{clip.width,clip.height}};vkCmdSetScissor(command,0,1,&scissor);
                const auto effect=[&](bool shadow){const auto parameters=effect_parameters(views[i],frame.viewport,scale,shadow);const auto descriptor_index=(shadow?3:views[i].backdrop->source==BackdropSource::content?1:2)*MaximumLayers+i;vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&descriptor[descriptor_index],0,nullptr);vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(parameters),&parameters);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,effect_pipeline);vkCmdDraw(command,6,1,0,0);};
                if(views[i].shadow)effect(true);if(views[i].backdrop)effect(false);
                const float size[]{frame.viewport.width,frame.viewport.height};vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(size),size);
                vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&descriptor[i],0,nullptr);
                const auto& layer=layers[i];if(layer.panel_count){vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,panels_pipeline);vkCmdDraw(command,6,std::uint32_t(layer.panel_count),0,0);}if(layer.foreground){GpuEffect foreground{{frame.viewport.width,frame.viewport.height,float(scale),1},{float(layer.region.x),float(layer.region.y),float(layer.region.width),float(layer.region.height)},{},{},{}};vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(foreground),&foreground);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,text_pipeline);vkCmdDraw(command,6,1,0,0);}
            }
            vkCmdEndRenderPass(command);if(i==0&&has_effects)snapshot(*effect_content,content_initialized);
        }
    }
    if (acquired) {
        auto destination=swap_images[*acquired];
        barrier(destination,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit copy{}; copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.srcOffsets[1]={std::int32_t(width),std::int32_t(height),1};
        copy.dstOffsets[1]={std::int32_t(swap_extent.width),std::int32_t(swap_extent.height),1};
        vkCmdBlitImage(command,color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy,VK_FILTER_NEAREST);
        barrier(destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_WRITE_BIT,0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
    submit(acquired);
    if (changed) ++statistics.scene_renders;
    previous=frame; previous_scale=scale;
}

PixelBuffer VulkanCanvas::Impl::capture() {
    if (!color) throw std::runtime_error("Vulkan canvas has not rendered");
    wait(); const auto width=statistics.pixel_width,height=statistics.pixel_height;
    auto output=buffer(std::uint64_t(width)*height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    begin();
    VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={width,height,1};
    vkCmdCopyImageToBuffer(command,color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output->handle,1,&copy);
    VkBufferMemoryBarrier host{}; host.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    host.buffer=output->handle; host.offset=0; host.size=VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
    submit(std::nullopt); wait();
    PixelBuffer result{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    std::memcpy(result.rgba.data(),output->mapped,result.rgba.size()); return result;
}
VulkanCanvas::VulkanCanvas(void* parent,void* display,Acceleration acceleration):impl_(std::make_unique<Impl>()) {
    impl_->initialize(parent,display,acceleration);
}
VulkanCanvas::~VulkanCanvas()=default;
void VulkanCanvas::draw(const CanvasFrame& frame,double scale,double height) { impl_->draw(frame,scale,height); }
PixelBuffer VulkanCanvas::capture() { return impl_->capture(); }
GraphicsInfo VulkanCanvas::info() const { return impl_->statistics; }
bool VulkanCanvas::native_event(const void* event) {
    const auto result=impl_->window->event(event);
    if (result==VulkanWindowEvent::exposed && impl_->previous)
        impl_->draw(*impl_->previous,impl_->previous_scale,0);
    return result!=VulkanWindowEvent::ignored;
}
} // namespace ii::gui::detail
