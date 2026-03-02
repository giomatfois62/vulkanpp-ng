#include "vk_pipeline.hpp"
#include "vk_utils.hpp"

using namespace vke;

VkShaderModule vke::createShaderModule(const std::string &path, VkDevice device)
{
    std::vector<char> code = readFile(path);

    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data()) // !make sure data it's aligned!
    };

    VkShaderModule shaderModule;

    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

    return shaderModule;
}

PipelineBuilder::PipelineBuilder()
{
    clear();
}

void PipelineBuilder::clear()
{
    pipelineLayout = {};

    colorBlendAttachmentState = {};

    vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    inputAssemblyInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    rasterizationInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

    renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    dynamicStateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

    depthStencilInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    multisampleInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    shaderStages.clear();
}

VkPipeline PipelineBuilder::build(VkDevice device, VkRenderPass renderPass)
{
    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachmentState
    };

    VkPipelineViewportStateCreateInfo viewportStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssemblyInfo,
        .pViewportState = &viewportStateInfo,
        .pRasterizationState = &rasterizationInfo,
        .pMultisampleState = &multisampleInfo,
        .pDepthStencilState = &depthStencilInfo,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicStateInfo,
        .layout = pipelineLayout,
        .renderPass = renderPass
    };

    VkPipeline pipeline;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));

    return pipeline;
}

PipelineBuilder &PipelineBuilder::setLayout(VkPipelineLayout layout)
{
    pipelineLayout = layout;

    return *this;
}

PipelineBuilder &PipelineBuilder::setInputTopology(VkPrimitiveTopology topology)
{
    inputAssemblyInfo.topology = topology;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    return *this;
}

PipelineBuilder &PipelineBuilder::setPolygonMode(VkPolygonMode mode)
{
    rasterizationInfo.polygonMode = mode;
    rasterizationInfo.lineWidth = 1.0f;

    return *this;
}

PipelineBuilder &PipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    rasterizationInfo.cullMode = cullMode;
    rasterizationInfo.frontFace = frontFace;

    return *this;
}

PipelineBuilder &PipelineBuilder::setColorAttachmentFormat(VkFormat format)
{
    colorAttachmentFormat = format;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachmentFormats = &colorAttachmentFormat;

    return *this;
}

PipelineBuilder &PipelineBuilder::setDepthAttachmentFormat(VkFormat format)
{
    renderInfo.depthAttachmentFormat = format;

    return *this;
}

PipelineBuilder &PipelineBuilder::setVertexDescription(VertexInputDescription desc)
{
    vertexDescription = desc;

    vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
    vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
    vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
    vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

    return *this;
}

PipelineBuilder &PipelineBuilder::setDynamicStates(std::vector<VkDynamicState> states)
{
    dynamicStates = states;

    dynamicStateInfo.dynamicStateCount = 2;
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    return *this;
}

PipelineBuilder &PipelineBuilder::disableDepthTesting()
{
    depthStencilInfo.depthTestEnable = VK_FALSE;
    depthStencilInfo.depthWriteEnable = VK_FALSE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.front = {};
    depthStencilInfo.back = {};
    depthStencilInfo.minDepthBounds = 0.f;
    depthStencilInfo.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder &PipelineBuilder::enableDepthTesting(bool depthWriteEnable, VkCompareOp op)
{
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = depthWriteEnable;
    depthStencilInfo.depthCompareOp = op;
    depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilInfo.stencilTestEnable = VK_FALSE;
    depthStencilInfo.front = {};
    depthStencilInfo.back = {};
    depthStencilInfo.minDepthBounds = 0.f;
    depthStencilInfo.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder &PipelineBuilder::disableBlending()
{
    // default write mask
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    return *this;
}

PipelineBuilder &PipelineBuilder::setMSAASamples(VkSampleCountFlagBits sampleCount)
{
    multisampleInfo.sampleShadingEnable = sampleCount != VK_SAMPLE_COUNT_1_BIT;
    multisampleInfo.rasterizationSamples = sampleCount;
    multisampleInfo.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother
    multisampleInfo.pSampleMask = nullptr;
    multisampleInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleInfo.alphaToOneEnable = VK_FALSE;

    return *this;
}

PipelineBuilder &PipelineBuilder::addShaderStage(VkShaderStageFlagBits stage, VkShaderModule shader)
{
    shaderStages.push_back(
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = stage,
            .module = shader,
            .pName = "main"
        }
    );

    return *this;
}


void PushConstant::addRange(VkShaderStageFlagBits shaderStage, uint32_t size)
{
    uint32_t offset = 0;

    for (auto & range: ranges)
        offset += range.size;

    ranges.push_back({ shaderStage, offset, size }); // TODO: throw if max size exceeded
}

void PushConstant::pushRange(VkCommandBuffer cmd, VkPipelineLayout layout, size_t range, void *data)
{
    vkCmdPushConstants(
        cmd,
        layout,
        ranges[range].stageFlags,
        ranges[range].offset,
        ranges[range].size,
        data
    );
}
