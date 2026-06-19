#pragma once

class RenderContext;
class MemoryTextureBuilder;
class KtxTextureBuilder;

/// @brief 텍스처 내 각 하위 리소스(Mip 레벨, 배열 레이어 등)의 위치와 크기 정보를 담는 구조체입니다.
struct SubresourceData {
    uint32_t offset;
    uint32_t size;
    uint32_t width;
    uint32_t height;
    uint32_t mipLevel = 0;
    uint32_t arrayLayer = 0;
};

/// @brief 텍스처 생성에 필요한 전체 데이터(픽셀 데이터, 크기, 포맷 및 하위 리소스 정보 등)를 담는 구조체입니다.
struct TextureResourceData {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mipLevels = 1;
    uint32_t layerCount = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    std::vector<uint8_t> pixelData;
    std::vector<SubresourceData> subresources;
};

//
// =============== Texture ===============
//

/// @brief Vulkan 텍스처 객체(이미지, 이미지 뷰, 샘플러)를 관리하는 클래스입니다.
class Texture {
    friend class MemoryTextureBuilder;
    friend class KtxTextureBuilder;

public:
    Texture() = delete;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(RenderContext* context) : m_context(context) {}
    ~Texture();

    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    VkFormat getFormat() const { return m_format; }

    void cleanupStaging();

private:
    void upload(VkCommandBuffer cmd, const TextureResourceData& data);
    void createImageView(uint32_t layerCount, uint32_t mipLevels);
    void createSampler(
        uint32_t mipLevels,
        VkFilter min,
        VkFilter mag,
        VkSamplerAddressMode u,
        VkSamplerAddressMode v
    );

private:
    /// @brief 소유하지 않는 클래스 맴버 변수.
    RenderContext* m_context = nullptr;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;

    struct BufferResource {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    std::vector<BufferResource> m_stagingResources;
};

//
// =============== MemoryTextureBuilder ===============
//

/// @brief 메모리 상의 원시 픽셀 데이터나 인코딩된 이미지 데이터로부터 텍스처를 생성하기 위한 빌더 클래스입니다.
class MemoryTextureBuilder {
public:
    MemoryTextureBuilder& setEncodedData(const uint8_t* data, size_t size, bool srgb = true);
    MemoryTextureBuilder& setRawDataWithSubresources(
        const uint8_t* pixels,
        size_t totalDataSize,
        uint32_t width,
        uint32_t height,
        uint32_t mipLevels,
        uint32_t layerCount,
        VkFormat format,
        const std::vector<SubresourceData>& subresources
    );
    MemoryTextureBuilder& setFilter(VkFilter min, VkFilter mag);
    MemoryTextureBuilder& setWrap(VkSamplerAddressMode u, VkSamplerAddressMode v);

    std::unique_ptr<Texture> build(RenderContext* context, VkCommandBuffer cmd);

private:
    bool isFormatSRGB(VkFormat format) const;

private:
    bool m_isSRGB = true;
    bool m_isEncoded = false;
    std::vector<uint8_t> m_bufferData;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipLevels = 1;
    uint32_t m_layerCount = 1;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    std::vector<SubresourceData> m_subresources;

    VkFilter m_minFilter = VK_FILTER_LINEAR;
    VkFilter m_magFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode m_wrapU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode m_wrapV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
};

//
// =============== KtxTextureBuilder ===============
//

/// @brief KTX 파일로부터 텍스처를 로드하고 생성하기 위한 빌더 클래스입니다.
class KtxTextureBuilder {
public:
    KtxTextureBuilder& setFile(const std::filesystem::path& filePath, bool srgb);
    KtxTextureBuilder& setFilter(VkFilter min, VkFilter mag);
    KtxTextureBuilder& setWrap(VkSamplerAddressMode u, VkSamplerAddressMode v);
    KtxTextureBuilder& setTanscodeTarget(ktx_transcode_fmt_e targetFormat);

    std::unique_ptr<Texture> build(RenderContext* context, VkCommandBuffer cmd);

private:
    VkFormat convertFormat(VkFormat format, bool isSRGB);

private:
    std::filesystem::path m_filePath;
    VkFilter m_minFilter = VK_FILTER_LINEAR;
    VkFilter m_magFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode m_wrapU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode m_wrapV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ktx_transcode_fmt_e m_transcodeTarget = KTX_TTF_BC7_RGBA;
    bool m_isSRGB = true;
};
