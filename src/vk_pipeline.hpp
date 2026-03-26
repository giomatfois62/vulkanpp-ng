#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include "vk_mesh.hpp"

#include <string>

namespace vke {

VkShaderModule createShaderModule(const std::string &path, VkDevice device);

class PipelineBuilder {
public:
    PipelineBuilder();

    void clear();
    VkPipeline build(VkDevice device, VkRenderPass renderPass);

    PipelineBuilder& setLayout(VkPipelineLayout layout);
    PipelineBuilder& setInputTopology(VkPrimitiveTopology topology);
    PipelineBuilder& setPolygonMode(VkPolygonMode mode);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& setColorAttachmentFormat(VkFormat format);
    PipelineBuilder& setDepthAttachmentFormat(VkFormat format);
    PipelineBuilder& setVertexDescription(VertexInputDescription desc);
    PipelineBuilder& setDynamicStates(std::vector<VkDynamicState> states);
    PipelineBuilder& disableDepthTesting();
    PipelineBuilder& enableDepthTesting(bool depthWriteEnable,VkCompareOp op);
    PipelineBuilder& disableBlending();
    //PipelineBuilder& enableBlendingAdditive();
    //PipelineBuilder& enableBlendingAlphaBlend();
    PipelineBuilder& setMSAASamples(VkSampleCountFlagBits sampleCount);
    PipelineBuilder& addShaderStage(VkShaderStageFlagBits stage, VkShaderModule shader);

    VkPipelineLayout pipelineLayout;
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
    VkPipelineRenderingCreateInfo renderInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    VkFormat colorAttachmentFormat;
    VertexInputDescription vertexDescription;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkDynamicState> dynamicStates;
};

} // end namespace vke

#endif // PIPELINE_HPP
