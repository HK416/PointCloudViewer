#include "stdafx.h"
#include "Texture.h"
#include "Renderer.h"

Texture::~Texture() {
    cleanupStaging();
    if (m_context) {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_context->getDevice(), m_sampler, nullptr);
        }

        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_context->getDevice(), m_imageView, nullptr);
        }

        if (m_image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_context->getAllocator(), m_image, m_allocation);
        }
    }
}

void Texture::cleanupStaging() {
    if (m_context) {
        for (const auto& res : m_stagingResources) {
            vmaDestroyBuffer(m_context->getAllocator(), res.buffer, res.allocation);
        }
        m_stagingResources.clear();
        m_stagingResources.shrink_to_fit();
    }
}

void Texture::upload(VkCommandBuffer cmd, const TextureResourceData& data) {
    m_format = data.format;

    // Create Staging buffer
    BufferResource staging;
    {
        VkBufferCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.size = static_cast<uint32_t>(data.pixelData.size());
        createInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo info;
        VkResult res = vmaCreateBuffer(
            m_context->getAllocator(),
            &createInfo,
            &allocInfo,
            &staging.buffer,
            &staging.allocation,
            &info
        );
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create image buffer resource! (CODE:{:#08x})",
                    (int)res
                )
            );
        }

        memcpy(info.pMappedData, data.pixelData.data(), data.pixelData.size());
        m_stagingResources.push_back(staging);
    }

    // Create Device Local Image
    {
        VkImageCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        createInfo.imageType = VK_IMAGE_TYPE_2D;
        createInfo.extent.width = data.width;
        createInfo.extent.height = data.height;
        createInfo.extent.depth = 1;
        createInfo.mipLevels = data.mipLevels;
        createInfo.arrayLayers = data.layerCount;
        createInfo.format = data.format;
        createInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        createInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        if (data.layerCount == 6) {
            createInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        
        VkResult res = vmaCreateImage(
            m_context->getAllocator(),
            &createInfo,
            &allocInfo,
            &m_image,
            &m_allocation,
            nullptr
        );
        if (res != VK_SUCCESS) {
            throw std::runtime_error(
                std::format(
                    "Failed to create image resource! (CODE:{:#08x})", (int)res
                )
            );
        }
    }

    // UNDEFINED -> TRANSFER_DST_OPTIMAL
    RenderUtils::transitionImageLayout(
        cmd,
        m_image,
        m_format,
        0, data.mipLevels,
        0, data.layerCount,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    // Copy
    std::vector<VkBufferImageCopy> regions;
    if (data.subresources.empty()) {
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = data.layerCount;
        region.imageExtent.width = data.width;
        region.imageExtent.height = data.height;
        region.imageExtent.depth = 1;
        regions.push_back(region);
    } else {
        for (const auto& subRes : data.subresources) {
            VkBufferImageCopy region = {};
            region.bufferOffset = subRes.offset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = subRes.mipLevel;
            region.imageSubresource.baseArrayLayer = subRes.arrayLayer;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = subRes.width;
            region.imageExtent.height = subRes.height;
            region.imageExtent.depth = 1;
            regions.push_back(region);
        }
    }

    vkCmdCopyBufferToImage(
        cmd,
        staging.buffer,
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()),
        regions.data()
    );

    // TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    RenderUtils::transitionImageLayout(
        cmd,
        m_image,
        m_format,
        0, data.mipLevels,
        0, data.layerCount,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
}

void Texture::createImageView(uint32_t layerCount, uint32_t mipLevels) {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m_image;

    if (layerCount == 6) {
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    } else if (layerCount > 1) {
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    } else {
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    createInfo.format = m_format;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = mipLevels;
    createInfo.subresourceRange.layerCount = layerCount;

    VkResult res = vkCreateImageView(m_context->getDevice(), &createInfo, nullptr, &m_imageView);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("Failed to create image view! (CODE:{:#08x})", (int)res)
        );
    }
}

void Texture::createSampler(
    uint32_t mipLevels,
    VkFilter min,
    VkFilter mag,
    VkSamplerAddressMode u,
    VkSamplerAddressMode v
) {
    VkSamplerCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    createInfo.minFilter = min;
    createInfo.magFilter = mag;
    createInfo.addressModeU = u;
    createInfo.addressModeV = v;
    createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    createInfo.maxLod = static_cast<float>(mipLevels);

    VkResult res = vkCreateSampler(m_context->getDevice(), &createInfo, nullptr, &m_sampler);
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to create image sampler! (CODE:{:#08x})", (int)res
            )
        );
    }
}

//
// =============== MemoryTextureBuilder ===============
//

MemoryTextureBuilder& MemoryTextureBuilder::setEncodedData(
    const uint8_t* data, size_t size, bool srgb
) {
    m_isSRGB = srgb;
    m_isEncoded = true;
    m_bufferData.assign(data, data + size);
    m_mipLevels = 1;
    m_layerCount = 1;
    return *this;
}

MemoryTextureBuilder& MemoryTextureBuilder::setRawDataWithSubresources(
    const uint8_t* pixels,
    size_t totalDataSize,
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    uint32_t layerCount,
    VkFormat format,
    const std::vector<SubresourceData>& subresources
) {
    m_isSRGB = isFormatSRGB(format);
    m_isEncoded = false;
    m_bufferData.assign(pixels, pixels + totalDataSize);
    m_width = width;
    m_height = height;
    m_mipLevels = mipLevels;
    m_layerCount = layerCount;
    m_format = format;
    m_subresources = subresources;
    return *this;
}

MemoryTextureBuilder& MemoryTextureBuilder::setFilter(VkFilter min, VkFilter mag) {
    m_minFilter = min;
    m_magFilter = mag;
    return *this;
}

MemoryTextureBuilder& MemoryTextureBuilder::setWrap(VkSamplerAddressMode u, VkSamplerAddressMode v) {
    m_wrapU = u;
    m_wrapV = v;
    return *this;
}

std::unique_ptr<Texture> MemoryTextureBuilder::build(RenderContext* context, VkCommandBuffer cmd) {
    TextureResourceData resData;

    if (m_isEncoded) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load_from_memory(
            m_bufferData.data(),
            static_cast<int>(m_bufferData.size()),
            &texWidth,
            &texHeight,
            &texChannels,
            STBI_rgb_alpha
        );

        if (!pixels) {
            throw std::runtime_error("Memory image decoding failed!");
        }

        resData.width = static_cast<uint32_t>(texWidth);
        resData.height = static_cast<uint32_t>(texHeight);
        resData.mipLevels = 1;
        resData.layerCount = 1;
        resData.format = m_isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

        size_t imageSize = resData.width * resData.height * 4;
        resData.pixelData.assign(pixels, pixels + imageSize);

        SubresourceData subData = {};
        subData.offset = 0;
        subData.size = static_cast<uint32_t>(imageSize);
        subData.width = resData.width;
        subData.height = resData.height;
        subData.mipLevel = 0;
        subData.arrayLayer = 0;
        resData.subresources.push_back(subData);

        stbi_image_free(pixels);
    } else {
        if (m_bufferData.empty()) {
            throw std::invalid_argument("Invalid Raw Data!");
        }

        resData.width = m_width;
        resData.height = m_height;
        resData.format = m_format;
        resData.mipLevels = m_mipLevels;
        resData.layerCount = m_layerCount;
        resData.pixelData = m_bufferData;
        resData.subresources = m_subresources;
    }

    auto texture = std::make_unique<Texture>(context);
    texture->upload(cmd, resData);
    texture->createImageView(resData.layerCount, resData.mipLevels);
    texture->createSampler(resData.mipLevels, m_minFilter, m_magFilter, m_wrapU, m_wrapV);

    return texture;
}

bool MemoryTextureBuilder::isFormatSRGB(VkFormat format) const {
    switch (format) {
        // 주요 R8 포맷
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:

        // 압축 텍스처(BC 계열)
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:

        // 그 외 ASTC, ETC 압축 포맷
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            return true;

        default:
            return false;
    }
}

//
// =============== KtxTextureBuilder ===============
//

KtxTextureBuilder& KtxTextureBuilder::setFile(const std::filesystem::path& filePath, bool srgb) {
    m_isSRGB = srgb;
    m_filePath = filePath;
    return *this;
}

KtxTextureBuilder& KtxTextureBuilder::setFilter(VkFilter min, VkFilter mag) {
    m_minFilter = min;
    m_magFilter = mag;
    return *this;
}

KtxTextureBuilder& KtxTextureBuilder::setWrap(VkSamplerAddressMode u, VkSamplerAddressMode v) {
    m_wrapU = u;
    m_wrapV = v;
    return *this;
}

KtxTextureBuilder& KtxTextureBuilder::setTanscodeTarget(ktx_transcode_fmt_e targetFormat) {
    m_transcodeTarget = targetFormat;
    return *this;
}

std::unique_ptr<Texture> KtxTextureBuilder::build(RenderContext* context, VkCommandBuffer cmd) {
    ktxTexture* ktxTex = nullptr;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        m_filePath.string().c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &ktxTex
    );
    if (result != KTX_SUCCESS) {
        throw std::runtime_error(
            std::format(
                "Failed to load ktx texture (PATH:{}, CODE:{:#08x})",
                m_filePath.string(),
                (int)result
            )
        );
    }

    if (ktxTexture_NeedsTranscoding(ktxTex)) {
        ktxTexture2* ktxTex2 = reinterpret_cast<ktxTexture2*>(ktxTex);
        result = ktxTexture2_TranscodeBasis(ktxTex2, m_transcodeTarget, NULL);
        if (result != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTex);
            throw std::runtime_error(
                std::format(
                    "Failed to transcode KTX2 Basis texture! (CODE:{:#08x})",
                    (int)result
                )
            );
        }
    }

    TextureResourceData resData;
    resData.width = ktxTex->baseWidth;
    resData.height = ktxTex->baseHeight;
    resData.mipLevels = ktxTex->numLevels;

    uint32_t numLayers = std::max(1u, ktxTex->numLayers);
    uint32_t numFaces = ktxTex->isCubemap ? 6 : 1;
    resData.layerCount = numLayers * numFaces;

    resData.format = convertFormat(ktxTexture_GetVkFormat(ktxTex), m_isSRGB);

    size_t totalSize = ktxTexture_GetDataSize(ktxTex);
    const uint8_t* rawData = ktxTexture_GetData(ktxTex);
    resData.pixelData.assign(rawData, rawData + totalSize);

    for (uint32_t mip = 0; mip < ktxTex->numLevels; ++mip) {
        for (uint32_t layer = 0; layer < numLayers; ++layer) {
            for (uint32_t face = 0; face < numFaces; ++face) {
                size_t offset = 0;
                ktxTexture_GetImageOffset(ktxTex, mip, layer, face, &offset);

                SubresourceData subData = {};
                subData.offset = static_cast<uint32_t>(offset);
                subData.size = static_cast<uint32_t>(ktxTexture_GetImageSize(ktxTex, mip));
                subData.width = std::max(1u, resData.width >> mip);
                subData.height = std::max(1u, resData.height >> mip);
                subData.mipLevel = mip;
                subData.arrayLayer = layer * numFaces + face;
                resData.subresources.push_back(subData);
            }
        }
    }

    ktxTexture_Destroy(ktxTex);
    
    auto texture = std::make_unique<Texture>(context);
    texture->upload(cmd, resData);
    texture->createImageView(resData.layerCount, resData.mipLevels);
    texture->createSampler(resData.mipLevels, m_minFilter, m_magFilter, m_wrapU, m_wrapV);

    return texture;
}

VkFormat KtxTextureBuilder::convertFormat(VkFormat format, bool isSRGB) {
    switch (format) {
        case VK_FORMAT_R8_UNORM:            return isSRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
        case VK_FORMAT_R8G8_UNORM:          return isSRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
        case VK_FORMAT_R8G8B8_UNORM:        
        case VK_FORMAT_R8G8B8_SRGB:         return isSRGB ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:       return isSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:       return isSRGB ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;;

        // BC1, BC4 
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:  return isSRGB ? VK_FORMAT_BC1_RGB_SRGB_BLOCK : VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return isSRGB ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    
        // BC2, BC3, BC5, BC6H, BC7 
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:      return isSRGB ? VK_FORMAT_BC2_SRGB_BLOCK : VK_FORMAT_BC2_UNORM_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:      return isSRGB ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:      return isSRGB ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;

        default:
            return format;
    };
}
