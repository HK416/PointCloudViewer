#pragma once

class RenderContext;
class ShaderLayout;

//
// =============== RenderPipelineStates ===============
//

/// @brief 그래픽 파이프라인 생성에 필요한 다양한 상태 정보(정점 입력, 래스터라이저, 블렌딩 등)를 캡슐화한 구조체입니다.
struct RenderPipelineStates {
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

    RenderPipelineStates();
};


//
// =============== Shader ===============
//

/// @brief Vulkan 파이프라인 및 셰이더 모듈 생성을 관리하는 셰이더 기저 클래스입니다.
class Shader {
public:
    Shader() = delete;
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(RenderContext* context, ShaderLayout* layout);
    virtual ~Shader();

    virtual void bind(VkCommandBuffer cmd) = 0;

    VkPipeline getPipeline() const { return m_pipeline; }
    ShaderLayout* getLayout() const { return m_layout; }

protected:
    std::vector<char> readSPIRVFile(const std::filesystem::path& filePath);
    VkShaderModule createShaderModule(const std::vector<char>& code);

protected:
    /// @brief 소유하지 않는 클래스 맴버 변수
    RenderContext* m_context = nullptr;
    /// @brief 소유하지 않는 클래스 맴버 변수
    ShaderLayout* m_layout = nullptr;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

//
// =============== GraphicsShader ===============
//

/// @brief 그래픽스 렌더링 파이프라인을 설정하고 관리하는 셰이더 클래스입니다.
class GraphicsShader : public Shader {
public:
    GraphicsShader() = delete;
    GraphicsShader(const GraphicsShader&) = delete;
    GraphicsShader& operator=(const GraphicsShader&) = delete;

    GraphicsShader(RenderContext* context, ShaderLayout* layout);
    virtual ~GraphicsShader() = default;

    virtual void bind(VkCommandBuffer cmd) override;

protected:
    void setupRenderPipeline(
        const RenderPipelineStates& states,
        const std::vector<VkPipelineShaderStageCreateInfo>& stages
    );
};

//
// =============== PointCloudShader ===============
//

/// @brief 포인트 클라우드 렌더링에 특화된 파이프라인 및 셰이더 설정을 처리하는 클래스입니다.
class PointCloudShader : public GraphicsShader {
public:
    PointCloudShader() = delete;
    PointCloudShader(const PointCloudShader&) = delete;
    PointCloudShader& operator=(const PointCloudShader&) = delete;

    PointCloudShader(RenderContext* context, ShaderLayout* layout);
    virtual ~PointCloudShader() = default;
};

//
// =============== SkyboxShader ===============
//

/// @brief 스카이박스 렌더링에 특화된 파이프라인 및 셰이더 설정을 처리하는 클래스입니다.
class SkyboxShader : public GraphicsShader {
public:
    SkyboxShader() = delete;
    SkyboxShader(const SkyboxShader&) = delete;
    SkyboxShader& operator=(const SkyboxShader&) = delete;

    SkyboxShader(RenderContext* context, ShaderLayout* layout);
    virtual ~SkyboxShader() = default;
};