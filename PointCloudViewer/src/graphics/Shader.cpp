#include "stdafx.h"
#include "Shader.h"
#include "Renderer.h"
#include "ShaderLayout.h"
#include "PointCloudVertex.h"

//
// =============== RenderPipelineStates ===============
//

RenderPipelineStates::RenderPipelineStates() {
    // ---------- Vertex Input State -----------
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // ---------- Input Assembly State -----------
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ---------- Rasterization State -----------
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // ---------- Multisample State -----------
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // ---------- Depth Stencil State -----------
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // ---------- Color Blend Attachment -----------
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    // ---------- Color Blend State -----------
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;
}

//
// =============== Shader ===============
//

Shader::Shader(RenderContext* context, ShaderLayout* layout) 
    : m_context(context), m_layout(layout) {}

Shader::~Shader() {
    if (m_context && m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context->getDevice(), m_pipeline, nullptr);
    }
}

std::vector<char> Shader::readSPIRVFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(
            "Failed to open shader file " + filePath.string()
        );
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule Shader::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult res = vkCreateShaderModule(
        m_context->getDevice(), &createInfo, nullptr, &module
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create shader module! (CODE:{:#08x})", (int)res
            )
        );
    }

    return module;
}

//
// =============== GraphicsShader ===============
//

GraphicsShader::GraphicsShader(RenderContext* context, ShaderLayout* layout) 
    : Shader(context, layout) {}

void GraphicsShader::bind(VkCommandBuffer cmd) {
    if (m_pipeline) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }
}

void GraphicsShader::setupRenderPipeline(
    const RenderPipelineStates& states,
    const std::vector<VkPipelineShaderStageCreateInfo>& stages,
    VkFormat colorFormat,
    VkFormat depthFormat
) {
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &states.vertexInput;
    pipelineInfo.pInputAssemblyState = &states.inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &states.rasterizer;
    pipelineInfo.pMultisampleState = &states.multisampling;
    pipelineInfo.pDepthStencilState = &states.depthStencil;
    pipelineInfo.pColorBlendState = &states.colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout->getPipelineLayout();
    
    VkResult res = vkCreateGraphicsPipelines(
        m_context->getDevice(),
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &m_pipeline
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create graphics pipeline! (CODE:{:#08x})", (int)res
            )
        );
    }
}

//
// =============== PointCloudShader ===============
//

PointCloudShader::PointCloudShader(RenderContext* context, ShaderLayout* layout) 
    : GraphicsShader(context, layout) 
{
    // ----- Load shader binary -----
    auto vertCode = readSPIRVFile("./shaders/point.vert.spv");
    auto fragCode = readSPIRVFile("./shaders/point.frag.spv");

    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    
    const std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        vertShaderStageInfo, fragShaderStageInfo
    };
    
    // ----- Setup input assembly ------
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(PointCloudVertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(PointCloudVertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(PointCloudVertex, color);

    RenderPipelineStates states;
    states.vertexInput.vertexBindingDescriptionCount = 1;
    states.vertexInput.pVertexBindingDescriptions = &bindingDescription;
    states.vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    states.vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();
    states.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    setupRenderPipeline(
        states,
        shaderStages,
        RenderSwapchain::swapchainImageFormat,
        RenderSwapchain::depthImageFormat
    );

    vkDestroyShaderModule(m_context->getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->getDevice(), fragModule, nullptr);
}

//
// =============== SkyboxShader ===============
//

SkyboxShader::SkyboxShader(RenderContext* context, ShaderLayout* layout) 
    : GraphicsShader(context, layout) 
{    
    // ----- Load shader binary -----
    auto vertCode = readSPIRVFile("./shaders/skybox.vert.spv");
    auto fragCode = readSPIRVFile("./shaders/skybox.frag.spv");

    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);
    
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    
    const std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        vertShaderStageInfo, fragShaderStageInfo
    };
    
    RenderPipelineStates states;
    states.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    states.rasterizer.cullMode = VK_CULL_MODE_NONE;
    states.depthStencil.depthWriteEnable = VK_FALSE;
    states.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    setupRenderPipeline(
        states,
        shaderStages,
        RenderSwapchain::swapchainImageFormat,
        RenderSwapchain::depthImageFormat
    );

    vkDestroyShaderModule(m_context->getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->getDevice(), fragModule, nullptr);
}

//
// =============== EDLShader ===============
//

EDLShader::EDLShader(RenderContext* context, ShaderLayout* layout) 
    : GraphicsShader(context, layout) 
{
    auto vertCode = readSPIRVFile("./shaders/edl.vert.spv");
    auto fragCode = readSPIRVFile("./shaders/edl.frag.spv");

    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    
    const std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        vertShaderStageInfo, fragShaderStageInfo
    };
    
    RenderPipelineStates states;
    states.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    states.rasterizer.cullMode = VK_CULL_MODE_NONE;
    states.depthStencil.depthTestEnable = VK_FALSE;
    states.depthStencil.depthWriteEnable = VK_FALSE;

    setupRenderPipeline(
        states,
        shaderStages,
        RenderSwapchain::swapchainImageFormat,
        VK_FORMAT_UNDEFINED
    );

    vkDestroyShaderModule(m_context->getDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->getDevice(), fragModule, nullptr);
}
