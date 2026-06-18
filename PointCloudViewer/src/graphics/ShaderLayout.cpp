#include "stdafx.h"
#include "ShaderLayout.h"
#include "Renderer.h"

//
// =============== ShaderLayout ===============
//

ShaderLayout::~ShaderLayout() {
    if (m_context) {
        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_context->getDevice(), m_pipelineLayout, nullptr);
        }

        for (size_t i = 0; i < m_descriptorSetLayouts.size(); ++i) {
            if (m_ownedLayouts[i] && m_descriptorSetLayouts[i] != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_context->getDevice(), m_descriptorSetLayouts[i], nullptr);
            }
        }
    }
}

//
// =============== ShaderLayoutBuilder ===============
//

ShaderLayoutBuilder& ShaderLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout layout) {
    m_descriptorSetLayouts.push_back({{}, layout});
    return *this;
}

ShaderLayoutBuilder& ShaderLayoutBuilder::addDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    m_descriptorSetLayouts.push_back({bindings, VK_NULL_HANDLE});
    return *this;
}

ShaderLayoutBuilder& ShaderLayoutBuilder::addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size) {
    m_pushConstantRanges.push_back(VkPushConstantRange{stageFlags, offset, size});
    return *this;
}

std::unique_ptr<ShaderLayout> ShaderLayoutBuilder::build(RenderContext* context) {
    auto shaderLayout = std::make_unique<ShaderLayout>(context);

    // Create Descriptor Set Layouts
    for (const auto& info : m_descriptorSetLayouts) {
        if (info.existingLayout != VK_NULL_HANDLE) {
            shaderLayout->m_descriptorSetLayouts.push_back(info.existingLayout);
            shaderLayout->m_ownedLayouts.push_back(false);
        } else {
            VkDescriptorSetLayoutCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            createInfo.bindingCount = static_cast<uint32_t>(info.bindings.size());
            createInfo.pBindings = info.bindings.data();
            
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            VkResult res = vkCreateDescriptorSetLayout(context->getDevice(), &createInfo, nullptr, &layout);
            if (res != VK_SUCCESS) {
                throw std::runtime_error(
                    std::format(
                        "Failed to create descriptor set layout! "
                        "(CODE:{:#08x})",
                        (int)res
                    )
                );
            }

            shaderLayout->m_descriptorSetLayouts.push_back(layout);
            shaderLayout->m_ownedLayouts.push_back(true);
        }
    }

    // Create Pipeline Layout
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = static_cast<uint32_t>(shaderLayout->m_descriptorSetLayouts.size());
    createInfo.pSetLayouts = shaderLayout->m_descriptorSetLayouts.data();
    createInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());
    createInfo.pPushConstantRanges = m_pushConstantRanges.data();

    VkResult res = vkCreatePipelineLayout(
        context->getDevice(),
        &createInfo,
        nullptr,
        &shaderLayout->m_pipelineLayout
    );
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create pipeline layout! (CODE:{:#08x})", (int)res
            )
        );
    }

    return shaderLayout;
}
