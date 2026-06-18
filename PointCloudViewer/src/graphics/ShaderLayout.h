#pragma once

class RenderContext;
class ShaderLayoutBuilder;

//
// =============== ShaderLayout ===============
//

/// @brief Vulkan 파이프라인 레이아웃(디스크립터 세트 및 푸시 상수)을 캡슐화하고 메모리를 관리하는 클래스입니다.
class ShaderLayout {
    friend class ShaderLayoutBuilder;

public:
    ShaderLayout() = delete;
    ShaderLayout(const ShaderLayout&) = delete;
    ShaderLayout& operator=(const ShaderLayout&) = delete;

    ShaderLayout(RenderContext* context) : m_context(context) {}
    ~ShaderLayout();

    VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
    const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return m_descriptorSetLayouts; }

private:
    /// @brief 소유하지 않는 클래스 맴버 변수
    RenderContext* m_context = nullptr;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    std::vector<bool> m_ownedLayouts;
};

//
// =============== ShaderLayoutBuilder ===============
//

class ShaderLayoutBuilder {
public:
    ShaderLayoutBuilder& addDescriptorSetLayout(VkDescriptorSetLayout layout);
    ShaderLayoutBuilder& addDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    ShaderLayoutBuilder& addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

    std::unique_ptr<ShaderLayout> build(RenderContext* context);

private:
    /// @brief 기존 레이아웃을 사용하거나 새로 생성할 디스크립터 바인딩 정보를 저장하는 구조체입니다.
    struct DescriptorSetLayoutInfo {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayout existingLayout = VK_NULL_HANDLE;
    };

    std::vector<DescriptorSetLayoutInfo> m_descriptorSetLayouts;
    std::vector<VkPushConstantRange> m_pushConstantRanges;
};
