#include <iiGUI/VulkanCanvas.h>
#include <iiGUI/VulkanPaintDevice.h>
#include <iiGUI/VulkanWindow.h>
#include <iiGUI/NativeText.h>
#include <iiGUI/Image.h>
#include <iiGUI/GpuEffects.h>
#include <iiGUI/VulkanScenePlan.h>
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
    bool pending=false,faulted=false;
    VkSemaphore available=VK_NULL_HANDLE;
    VkSwapchainKHR swapchain=VK_NULL_HANDLE;
    VkExtent2D swap_extent{};
    std::vector<VkImage> swap_images;
    std::vector<VkSemaphore> presented;
    bool swap_invalid=false;
    VkRenderPass render_pass=VK_NULL_HANDLE,load_pass=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_layout=VK_NULL_HANDLE;
    VkDescriptorPool descriptors=VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptor;
    std::size_t descriptor_capacity=0;
    VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;
    VkPipeline panels_pipeline=VK_NULL_HANDLE,text_pipeline=VK_NULL_HANDLE,effect_pipeline=VK_NULL_HANDLE;
    VkSampler sampler=VK_NULL_HANDLE;
    std::unique_ptr<Image> effect_content,effect_current;
    bool content_initialized=false,current_initialized=false;
    struct Layer final {
        std::unique_ptr<Image> glyphs;
        std::unique_ptr<Buffer> panels,glyph_upload;
        ForegroundKey key;
        PanelKey geometry;
        PixelClip region{};
        std::size_t panel_count=0;
        bool glyph_initialized=false,upload=false,foreground=false;
    };
    struct Target final {
        VkDevice device=VK_NULL_HANDLE;
        VkFramebuffer framebuffer=VK_NULL_HANDLE;
        std::unique_ptr<Image> color;
        std::vector<Layer> layers;
        std::uint32_t width=0,height=0;
        Target()=default;
        Target(const Target&)=delete;
        Target& operator=(const Target&)=delete;
        ~Target(){clear();}
        void clear_attachment()noexcept {
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            framebuffer=VK_NULL_HANDLE;color.reset();width=height=0;
        }
        void clear()noexcept {clear_attachment();layers.clear();}
    };
    Target output;
    std::vector<std::unique_ptr<Target>> scene_targets;
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
            scene_targets.clear();output.clear();
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
            effect_content.reset();effect_current.reset();effect_assets.clear();
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
    void initialize(void* parent,void* display,Acceleration acceleration,const VulkanSurfaceProvider* external=nullptr) {
        std::uint32_t count=0;
        checked(vkEnumerateInstanceExtensionProperties(nullptr,&count,nullptr),"Cannot enumerate Vulkan instance extensions");
        std::vector<VkExtensionProperties> supported(count);
        checked(vkEnumerateInstanceExtensionProperties(nullptr,&count,supported.data()),"Cannot read Vulkan instance extensions");
        std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME};
        if (external) {
            for (const auto& name : external->extensions)
                if (name != VK_KHR_SURFACE_EXTENSION_NAME) extensions.push_back(name.c_str());
        } else extensions.push_back(VulkanWindow::extension());
        for (auto name:extensions) if (!extension(supported,name)) throw std::runtime_error(std::string("Vulkan instance lacks ")+name);
        VkInstanceCreateInfo info{}; info.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        VkApplicationInfo app{}; app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName="iiGUI"; app.pEngineName="iiGUI"; app.apiVersion=VK_API_VERSION_1_1;
        info.pApplicationInfo=&app;
        if (extension(supported,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            if(std::ranges::none_of(extensions,[](const char* name){return std::strcmp(name,VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)==0;}))
                extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            info.flags|=VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
        info.enabledExtensionCount=std::uint32_t(extensions.size()); info.ppEnabledExtensionNames=extensions.data();
        checked(vkCreateInstance(&info,nullptr,&instance),"Cannot create Vulkan instance");
        if (external) {
            auto supplied=external->create(instance);
            if(!supplied)throw VulkanSurfaceFailure{supplied.error()};
            surface=*supplied;
            if(!surface)throw VulkanSurfaceFailure{{core::ErrorCode::invalid_argument,"Surface provider returned a null Vulkan surface"}};
        } else { window=std::make_unique<VulkanWindow>(parent,display); surface=window->surface(instance); }
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
        statistics.api=GraphicsApi::vulkan;statistics.srgb_attachment=true;
        statistics.backdrop_supported=statistics.shadow_supported=statistics.content_transform_supported=statistics.scene_sources_supported=true;
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
            {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {4,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},
            {5,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}};
        VkDescriptorSetLayoutCreateInfo layout{}; layout.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout.bindingCount=std::size(bindings); layout.pBindings=bindings;
        checked(vkCreateDescriptorSetLayout(device,&layout,nullptr,&descriptor_layout),"Cannot create Vulkan descriptors");
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(GpuEffect)};
        VkPipelineLayoutCreateInfo pipeline_info{}; pipeline_info.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_info.setLayoutCount=1; pipeline_info.pSetLayouts=&descriptor_layout;
        pipeline_info.pushConstantRangeCount=1; pipeline_info.pPushConstantRanges=&push;
        checked(vkCreatePipelineLayout(device,&pipeline_info,nullptr,&pipeline_layout),"Cannot create Vulkan pipeline layout");
        VkSamplerCreateInfo sampling{}; sampling.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampling.magFilter=sampling.minFilter=VK_FILTER_LINEAR; sampling.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampling.addressModeU=sampling.addressModeV=sampling.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampling.maxLod=0;
        checked(vkCreateSampler(device,&sampling,nullptr,&sampler),"Cannot create Vulkan text sampler");
        panels_pipeline=pipeline(panels_vert,panels_frag); text_pipeline=pipeline(text_vert,text_frag);effect_pipeline=pipeline(text_vert,effect_frag);
    }
    void ensure_descriptors(std::size_t layers) {
        const auto capacity=vulkan_descriptor_capacity(descriptor_capacity,layers);
        if(capacity==descriptor_capacity)return;
        if(capacity>std::numeric_limits<std::uint32_t>::max()/20)throw std::runtime_error("Vulkan descriptor capacity is too large");
        const auto sets=static_cast<std::uint32_t>(capacity*4);
        const VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,sets},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,sets*5}};
        VkDescriptorPoolCreateInfo pool{};pool.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets=sets;pool.poolSizeCount=std::size(sizes);pool.pPoolSizes=sizes;
        VkDescriptorPool replacement=VK_NULL_HANDLE;
        checked(vkCreateDescriptorPool(device,&pool,nullptr,&replacement),"Cannot create Vulkan descriptor pool");
        try{
            std::vector<VkDescriptorSetLayout> layouts(sets,descriptor_layout);
            std::vector<VkDescriptorSet> values(sets);
            VkDescriptorSetAllocateInfo allocation{};allocation.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocation.descriptorPool=replacement;allocation.descriptorSetCount=sets;allocation.pSetLayouts=layouts.data();
            checked(vkAllocateDescriptorSets(device,&allocation,values.data()),"Cannot allocate Vulkan layer descriptor sets");
            if(descriptors)vkDestroyDescriptorPool(device,descriptors,nullptr);
            descriptors=replacement;descriptor=std::move(values);descriptor_capacity=capacity;
        }catch(...){vkDestroyDescriptorPool(device,replacement,nullptr);throw;}
    }
    [[nodiscard]] VkDescriptorSet layer_descriptor(std::size_t bank,std::size_t layer)const noexcept {
        return descriptor[bank*descriptor_capacity+layer];
    }
    void ensure_target(Target& target,std::uint32_t width,std::uint32_t height,bool sampled) {
        if(target.color&&target.width==width&&target.height==height)return;
        auto color=image(width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|
            (sampled?VK_IMAGE_USAGE_SAMPLED_BIT:VkImageUsageFlags{}));
        VkFramebufferCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass=render_pass;info.attachmentCount=1;info.pAttachments=&color->view;
        info.width=width;info.height=height;info.layers=1;VkFramebuffer framebuffer=VK_NULL_HANDLE;
        checked(vkCreateFramebuffer(device,&info,nullptr,&framebuffer),"Cannot create Vulkan scene framebuffer");
        target.clear_attachment();target.device=device;target.framebuffer=framebuffer;target.color=std::move(color);target.width=width;target.height=height;
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
    void abort_frame(bool acquired) noexcept {
        // An acquired image signals `available` even if encoding later fails.
        // Consume that signal before reusing the semaphore and retire the old
        // swapchain on the next explicit frame request.
        if(acquired && !pending){
            VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkSubmitInfo consume{};consume.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
            consume.waitSemaphoreCount=1;consume.pWaitSemaphores=&available;consume.pWaitDstStageMask=&stage;
            if(vkResetFences(device,1,&fence)!=VK_SUCCESS || vkQueueSubmit(queue,1,&consume,fence)!=VK_SUCCESS)
                faulted=true;
            else pending=true;
        }
        if(pending){
            if(vkWaitForFences(device,1,&fence,VK_TRUE,gpu_timeout)!=VK_SUCCESS)faulted=true;
            else pending=false;
        }
        if(acquired)swap_invalid=true;
        // If GPU completion cannot be established, keep all resources alive
        // and reject further frames until the device is destroyed.
        if(faulted)return;
        previous.reset();output.layers.clear();scene_targets.clear();
        effect_assets.clear();effect_content.reset();effect_current.reset();
        content_initialized=current_initialized=false;
    }
    void begin() {
        checked(vkResetCommandPool(device,command_pool,0),"Cannot reset Vulkan commands");
        VkCommandBufferBeginInfo info{}; info.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        checked(vkBeginCommandBuffer(command,&info),"Cannot begin Vulkan commands");
    }
    bool submit(std::optional<std::uint32_t> acquired) {
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
            return result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR;
        }
        return true;
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
    bool draw(const CanvasFrame&,double,double,bool=false,std::stop_token={});
    PixelBuffer capture();
};

bool VulkanCanvas::Impl::draw(const CanvasFrame& frame,double scale,double parent_height,bool external,std::stop_token stop) {
    if(faulted)throw std::runtime_error("Vulkan paint device must be recreated after an unrecoverable frame failure");
    if(stop.stop_requested())throw VulkanSurfaceFailure{{core::ErrorCode::cancelled,"Vulkan frame was cancelled"}};
    const auto [width,height]=external ? canvas_pixels(frame,scale,CanvasPixelRounding::outward) : canvas_pixels(frame,scale);
    const auto plan=vulkan_scene_plan(frame,width,height);
    if(!plan)throw std::invalid_argument("Invalid Vulkan source scene graph");
    statistics.effect_texture_budget_bytes=effect_texture_budget(width,height);
    if(!vulkan_scene_budget_ok(*plan,width,height))
        throw std::runtime_error("Vulkan source scene and effect textures exceed the effect budget");
    wait(); if(window)window->resize(frame.viewport,scale,parent_height);
    const bool can_present=ensure_swapchain(width,height);
    if(external&&!can_present)return false;
    const bool target_changed=!output.color || output.width!=width || output.height!=height || previous_scale!=scale;
    bool changed=!previous || *previous!=frame || previous_scale!=scale;
    std::optional<std::uint32_t> acquired;
    bool committed=false;
    struct Guard final {
        Impl& canvas;std::optional<std::uint32_t>& acquired;bool& committed;
        ~Guard(){if(!committed)canvas.abort_frame(bool(acquired));}
    } guard{*this,acquired,committed};
    if (!output.color || output.width!=width || output.height!=height) {
        ensure_target(output,width,height,false);
        statistics.pixel_width=width; statistics.pixel_height=height;
        effect_content.reset();effect_current.reset();content_initialized=current_initialized=false;
        statistics.cached_texture_bytes=std::uint64_t(width)*height*4; changed=true;
    }
    struct PreparedPass final {
        Target* target=nullptr;
        const CanvasFrame* frame=nullptr;
        std::optional<Rect> crop;
        std::size_t first_layer=0;
        bool source=false;
        CanvasBatchLayers batches;
    };
    std::vector<CanvasFrame> source_frames;
    std::vector<PreparedPass> passes;
    if (changed) {
        ensure_descriptors(plan->layers);
        scene_targets.resize(frame.scenes.size());source_frames.resize(frame.scenes.size());
        std::array<bool,maximum_canvas_scenes> active{};
        for(const auto& pass:plan->passes)if(pass.source)active[*pass.source]=true;
        for(std::size_t i=0;i<scene_targets.size();++i)if(!active[i])scene_targets[i].reset();
        CanvasScenePlan order{};
        std::array<bool,maximum_canvas_scenes> available{};
        for(const auto& pass:plan->passes)if(pass.source){
            const auto index=*pass.source;order.order[order.count++]=index;
            const auto& target=scene_targets[index];
            available[index]=target&&target->color&&target->width==width&&target->height==height;
        }
        const auto dirty=scene_rebuilds(frame,order,previous?&*previous:nullptr,available,target_changed);
        for(const auto& pass:plan->passes)if(pass.source){
            const auto index=*pass.source;
            if(!scene_targets[index])scene_targets[index]=std::make_unique<Target>();
            ensure_target(*scene_targets[index],width,height,true);
            source_frames[index]=canvas_scene_frame(frame.scenes[index]);
        }
        if(plan->effects&&(!effect_content||!effect_current)){
            auto content=image(width,height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            auto current=image(width,height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
            effect_content=std::move(content);effect_current=std::move(current);
            content_initialized=current_initialized=false;
        }
        statistics.cached_texture_bytes=std::uint64_t(width)*height*(effect_content?12:4);
        statistics.scene_texture_bytes=plan->scene_bytes;
        statistics.effect_image_texture_bytes=plan->asset_bytes;
        std::erase_if(effect_assets,[&](const auto& asset){return std::ranges::find(plan->assets,asset.source)==plan->assets.end();});
        for(const auto& image:plan->assets)effect_image(image);
        statistics.foreground_texture_bytes=0;passes.reserve(plan->passes.size());
        for(const auto& item:plan->passes){
            auto* target=item.source?scene_targets[*item.source].get():&output;
            if(item.source&&!dirty[*item.source]){
                for(const auto& layer:target->layers)if(layer.foreground&&layer.glyphs)
                    statistics.foreground_texture_bytes+=std::uint64_t(layer.region.width)*layer.region.height*4;
                continue;
            }
            const auto* input=item.source?&source_frames[*item.source]:&frame;
            const auto crop=item.source?frame.scenes[*item.source].bounds:std::optional<Rect>{};
            auto batches=canvas_batch_layers(*input);
            if(batches.views.size()!=item.layer_count)throw std::logic_error("Vulkan scene layer plan changed");
            target->layers.resize(batches.views.size());
            passes.push_back({target,input,crop,item.first_layer,bool(item.source),std::move(batches)});
        }
        const auto transparent=std::shared_ptr<const RasterImage>{};
        for(auto& pass:passes)for(std::size_t i=0;i<pass.batches.views.size();++i){
            auto& layer=pass.target->layers[i];const auto view=pass.batches.views[i];const auto slot=pass.first_layer+i;
            if(view.backdrop||view.shadow){
                const auto set_images=[&](std::size_t bank,std::shared_ptr<const RasterImage> mask,
                        std::shared_ptr<const RasterImage> underlay,const BackdropMaterial* material){
                    const auto background=material&&material->background_scene?scene_targets[*material->background_scene]->color->view:effect_image(transparent);
                    const auto content=material&&material->content_scene?scene_targets[*material->content_scene]->color->view:effect_image(transparent);
                    const VkDescriptorImageInfo images[]{
                        {sampler,bank==2?effect_current->view:effect_content->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        {sampler,effect_image(mask),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        {sampler,effect_image(underlay),VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        {sampler,background,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                        {sampler,content,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
                    VkWriteDescriptorSet writes[5]{};
                    for(unsigned index=0;index<std::size(writes);++index){auto& write=writes[index];write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        write.dstSet=layer_descriptor(bank,slot);write.dstBinding=index+1;write.descriptorCount=1;
                        write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pImageInfo=&images[index];}
                    vkUpdateDescriptorSets(device,std::size(writes),writes,0,nullptr);
                };
                if(view.shadow)set_images(3,view.shadow->mask,transparent,nullptr);
                if(view.backdrop){set_images(1,view.backdrop->mask,view.backdrop->underlay,&*view.backdrop);set_images(2,view.backdrop->mask,view.backdrop->underlay,&*view.backdrop);}
            }
            const auto geometry=pass.batches.panel_layers(i);layer.panel_count=pass.batches.panel_count(i);
            const auto bytes=std::max<std::size_t>(sizeof(GpuPanel),layer.panel_count*sizeof(GpuPanel));
            if(layer.panel_count&&(!layer.panels||layer.panels->capacity<bytes||!layer.geometry.matches(geometry))){
                const auto packed=pack_panel_batch(geometry,layer.panel_count);PanelKey next;next.assign(geometry);
                if(!layer.panels||layer.panels->capacity<bytes)layer.panels=buffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                // wait() retired the previous submission before this mapped buffer is modified.
                std::memcpy(layer.panels->mapped,packed.data(),packed.size()*sizeof(GpuPanel));
                layer.geometry=std::move(next);++statistics.geometry_uploads;
            }else if(!layer.panel_count){
                if(!layer.panels||layer.panels->capacity>sizeof(GpuPanel))layer.panels=buffer(sizeof(GpuPanel),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                layer.geometry.clear();
            }
            VkDescriptorBufferInfo input{layer.panels->handle,0,bytes};VkWriteDescriptorSet write{};
            write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.dstSet=layer_descriptor(0,slot);write.dstBinding=0;
            write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&input;
            vkUpdateDescriptorSets(device,1,&write,0,nullptr);layer.upload=false;layer.foreground=view.foreground();
            if(layer.foreground){
                if(!layer.glyphs||!layer.key.matches(pass.frame->viewport,scale,view,true)){
                    auto raster=rasterize_foreground_region(pass.frame->viewport,view,scale,true);const auto& bitmap=raster.pixels;
                    if(!layer.glyphs||layer.region.width!=bitmap.width||layer.region.height!=bitmap.height){layer.glyphs=image(bitmap.width,bitmap.height,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);layer.glyph_initialized=false;}
                    layer.region=raster.region;
                    if(!layer.glyph_upload||layer.glyph_upload->capacity<bitmap.rgba.size())layer.glyph_upload=buffer(bitmap.rgba.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                    std::memcpy(layer.glyph_upload->mapped,bitmap.rgba.data(),bitmap.rgba.size());layer.upload=true;
                    layer.key.assign(pass.frame->viewport,scale,view,true);++statistics.foreground_rasterizations;
                }
                VkDescriptorImageInfo text{sampler,layer.glyphs->view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                write.dstBinding=1;write.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;write.pBufferInfo=nullptr;write.pImageInfo=&text;
                vkUpdateDescriptorSets(device,1,&write,0,nullptr);
                statistics.foreground_texture_bytes+=std::uint64_t(layer.region.width)*layer.region.height*4;
            }else{layer.glyphs.reset();layer.glyph_upload.reset();layer.key={};layer.glyph_initialized=false;}
        }
    }
    // Prepare all fallible CPU allocations before acquiring a presentation
    // image, so a preparation error cannot strand its semaphore.
    CanvasFrame retained_frame=frame;
    if (can_present) {
        if(stop.stop_requested())throw VulkanSurfaceFailure{{core::ErrorCode::cancelled,"Vulkan frame was cancelled"}};
        std::uint32_t index=0;
        const auto result=vkAcquireNextImageKHR(device,swapchain,1'000'000'000,available,VK_NULL_HANDLE,&index);
        if (result==VK_SUCCESS || result==VK_SUBOPTIMAL_KHR) {
            acquired=index; if (result==VK_SUBOPTIMAL_KHR) swap_invalid=true;
        } else if (result==VK_ERROR_OUT_OF_DATE_KHR) swap_invalid=true;
        else if (result!=VK_TIMEOUT && result!=VK_NOT_READY) checked(result,"Cannot acquire native Vulkan image");
    }
    if(external&&!acquired){
        if(stop.stop_requested())throw VulkanSurfaceFailure{{core::ErrorCode::cancelled,"Vulkan frame was cancelled"}};
        return false;
    }
    if(!changed&&!acquired){committed=true;return true;}
    if(stop.stop_requested())throw VulkanSurfaceFailure{{core::ErrorCode::cancelled,"Vulkan frame was cancelled"}};
    begin();
    if (changed) {
        for(auto& asset:effect_assets)if(asset.pending){
            barrier(asset.texture->handle,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={asset.source?asset.source->width():1,asset.source?asset.source->height():1,1};
            vkCmdCopyBufferToImage(command,asset.upload->handle,asset.texture->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(asset.texture->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);asset.pending=false;
        }
        for(auto& pass:passes)for(auto& layer:pass.target->layers)if(layer.upload) {
            barrier(layer.glyphs->handle,layer.glyph_initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,layer.glyph_initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,
                layer.glyph_initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={layer.region.width,layer.region.height,1};
            vkCmdCopyBufferToImage(command,layer.glyph_upload->handle,layer.glyphs->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(layer.glyphs->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            layer.glyph_initialized=true;
        }
        const auto snapshot=[&](Image& source,Image& target,bool& initialized){
            barrier(target.handle,initialized?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,initialized?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,initialized?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={width,height,1};vkCmdCopyImage(command,source.handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            barrier(target.handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);initialized=true;
        };
        for(auto& pass:passes){
          for(std::size_t i=0;i<pass.batches.views.size();++i){
            const auto& view=pass.batches.views[i];const auto slot=pass.first_layer+i;
            if(i&&view.backdrop&&view.backdrop->source==BackdropSource::previous_layers)snapshot(*pass.target->color,*effect_current,current_initialized);
            const auto background=pass.frame->background;VkClearValue clear{};
            clear.color={{background.red*background.alpha,background.green*background.alpha,background.blue*background.alpha,background.alpha}};
            VkRenderPassBeginInfo begin_pass{};begin_pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;begin_pass.renderPass=i?load_pass:render_pass;begin_pass.framebuffer=pass.target->framebuffer;begin_pass.renderArea={{0,0},{width,height}};begin_pass.clearValueCount=1;begin_pass.pClearValues=&clear;
            vkCmdBeginRenderPass(command,&begin_pass,VK_SUBPASS_CONTENTS_INLINE);
            VkViewport viewport{0,0,float(width),float(height),0,1};vkCmdSetViewport(command,0,1,&viewport);
            const auto clip=scene_clip_pixels(view,pass.crop,scale,width,height);
            if(clip.width&&clip.height){
                const VkRect2D scissor{{std::int32_t(clip.x),std::int32_t(clip.y)},{clip.width,clip.height}};vkCmdSetScissor(command,0,1,&scissor);
                const auto effect=[&](bool shadow){const auto parameters=effect_parameters(view,pass.frame->viewport,scale,shadow);const auto bank=shadow?3:view.backdrop->source==BackdropSource::content?1:2;const auto set=layer_descriptor(bank,slot);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&set,0,nullptr);vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(parameters),&parameters);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,effect_pipeline);vkCmdDraw(command,6,1,0,0);};
                if(view.shadow)effect(true);if(view.backdrop)effect(false);
                const float size[]{pass.frame->viewport.width,pass.frame->viewport.height};vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(size),size);
                const auto set=layer_descriptor(0,slot);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&set,0,nullptr);
                const auto& layer=pass.target->layers[i];
                if(layer.panel_count){
                    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,panels_pipeline);
                    vkCmdDraw(command,6,std::uint32_t(layer.panel_count),0,0);++statistics.geometry_draw_batches;
                }
                if(layer.foreground){
                    GpuEffect foreground{};
                    foreground.view[0]=pass.frame->viewport.width;foreground.view[1]=pass.frame->viewport.height;
                    foreground.view[2]=float(scale);foreground.view[3]=1;
                    foreground.bounds[0]=float(layer.region.x);foreground.bounds[1]=float(layer.region.y);
                    foreground.bounds[2]=float(layer.region.width);foreground.bounds[3]=float(layer.region.height);
                    vkCmdPushConstants(command,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(foreground),&foreground);
                    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,text_pipeline);vkCmdDraw(command,6,1,0,0);
                }
            }
            vkCmdEndRenderPass(command);if(i==0&&plan->effects)snapshot(*pass.target->color,*effect_content,content_initialized);
          }
          if(pass.source)barrier(pass.target->color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT,
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
    }
    if (acquired) {
        auto destination=swap_images[*acquired];
        barrier(destination,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkImageBlit copy{}; copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        copy.srcOffsets[1]={std::int32_t(width),std::int32_t(height),1};
        copy.dstOffsets[1]={std::int32_t(swap_extent.width),std::int32_t(swap_extent.height),1};
        vkCmdBlitImage(command,output.color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy,VK_FILTER_NEAREST);
        barrier(destination,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,VK_ACCESS_TRANSFER_WRITE_BIT,0,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
    if(stop.stop_requested())throw VulkanSurfaceFailure{{core::ErrorCode::cancelled,"Vulkan frame was cancelled"}};
    const bool presented_frame=submit(acquired);
    if(changed)statistics.scene_renders+=passes.size();
    previous=std::move(retained_frame); previous_scale=scale;committed=true;
    return presented_frame;
}

PixelBuffer VulkanCanvas::Impl::capture() {
    if (!output.color) throw std::runtime_error("Vulkan canvas has not rendered");
    wait(); const auto width=statistics.pixel_width,height=statistics.pixel_height;
    auto output=buffer(std::uint64_t(width)*height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    begin();
    VkBufferImageCopy copy{}; copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; copy.imageExtent={width,height,1};
    vkCmdCopyImageToBuffer(command,this->output.color->handle,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output->handle,1,&copy);
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
VulkanCanvas::VulkanCanvas(const VulkanSurfaceProvider& external,Acceleration acceleration):impl_(std::make_unique<Impl>()) {
    impl_->initialize(nullptr,nullptr,acceleration,&external);
}
VulkanCanvas::~VulkanCanvas()=default;
void VulkanCanvas::draw(const CanvasFrame& frame,double scale,double height) { (void)impl_->draw(frame,scale,height); }
bool VulkanCanvas::draw_external(const CanvasFrame& frame,double scale,std::stop_token stop) { return impl_->draw(frame,scale,frame.viewport.height,true,stop); }
PixelBuffer VulkanCanvas::capture() { return impl_->capture(); }
GraphicsInfo VulkanCanvas::info() const { return impl_->statistics; }
bool VulkanCanvas::native_event(const void* event) {
    if(!impl_->window)return false;
    const auto result=impl_->window->event(event);
    if (result==VulkanWindowEvent::exposed && impl_->previous)
        impl_->draw(*impl_->previous,impl_->previous_scale,0);
    return result!=VulkanWindowEvent::ignored;
}
} // namespace ii::gui::detail
