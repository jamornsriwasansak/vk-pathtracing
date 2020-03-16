#pragma once

#include "common/mortar.h"
#include <vulkan/vulkan.hpp>
#include "gavulkan/buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct StbiUint8Result
{
	stbi_uc * m_stbi_data;
	int m_width;
	int m_height;

	StbiUint8Result(const std::filesystem::path & filepath)
	{
		// load image using STB
		int width, height, num_channels;
		const int num_desired_channels = 4;
		m_stbi_data = stbi_load(filepath.string().c_str(),
								&width,
								&height,
								&num_channels,
								num_desired_channels);

		m_width = width;
		m_height = height;

		// check whether loading success or not
		THROW_ASSERT(m_stbi_data, "fail to load texture from " + filepath.string());
		THROW_ASSERT(width > 0, "width <= zero from " + filepath.string());
		THROW_ASSERT(height > 0, "height <= zero from " + filepath.string());
	}
	
	~StbiUint8Result()
	{
		stbi_image_free(m_stbi_data);
	}
};

struct StbiFloat32Result
{
	float * m_stbi_data;
	int m_width;
	int m_height;

	StbiFloat32Result(const std::filesystem::path & filepath)
	{
		int width, height, numChannels;
		const int num_desired_channels = 4;
		m_stbi_data = stbi_loadf(filepath.string().c_str(),
								 &width,
								 &height,
								 &numChannels,
								 num_desired_channels);

		m_width = width;
		m_height = height;
	}

	~StbiFloat32Result()
	{
		stbi_image_free(m_stbi_data);
	}
};

template <typename ScalarType_>
struct DepthImage2d
{
	using ScalarType = ScalarType_;

	vk::Format					m_vk_format;
	vk::UniqueImage				m_vk_image;
	vk::UniqueDeviceMemory		m_vk_memory;
	vk::UniqueImageView			m_vk_image_view;
	size_t						m_width;
	size_t						m_height;

	DepthImage2d(const size_t width,
				 const size_t height,
				 const vk::ImageTiling vk_image_tiling,
				 const bool is_downloadable = false):
		m_width(width),
		m_height(height)
	{
		// choose m_vk_format based on ScalarType
		if constexpr (std::is_same<ScalarType, float>::value)
		{
			m_vk_format = vk::Format::eD32Sfloat;
		}
		else
		{
			THROW("unimplemented ScalarType");
		}

		// create an image
		vk::ImageCreateInfo image_ci = {};
		{
			image_ci.setImageType(vk::ImageType::e2D);
			image_ci.setExtent(vk::Extent3D(uint32_t(width), uint32_t(height), 1u));
			image_ci.setMipLevels(1);
			image_ci.setArrayLayers(1);
			image_ci.setFormat(m_vk_format);
			image_ci.setTiling(vk_image_tiling);
			image_ci.setInitialLayout(vk::ImageLayout::eUndefined);
			image_ci.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);
			image_ci.setSharingMode(vk::SharingMode::eExclusive);
			image_ci.setSamples(vk::SampleCountFlagBits::e1);
			image_ci.setFlags(vk::ImageCreateFlags(0));
		}
		m_vk_image = Core::Inst().m_vk_device->createImageUnique(image_ci);

		// allocate image memory
		vk::MemoryRequirements mem_requirements = Core::Inst().m_vk_device->getImageMemoryRequirements(*m_vk_image);
		uint32_t mem_type = find_mem_type(Core::Inst().m_vk_physical_device,
										  mem_requirements.memoryTypeBits,
										  vk::MemoryPropertyFlagBits::eDeviceLocal);
		vk::MemoryAllocateInfo mem_ai;
		{
			mem_ai.setAllocationSize(mem_requirements.size);
			mem_ai.setMemoryTypeIndex(mem_type);
		}
		m_vk_memory = Core::Inst().m_vk_device->allocateMemoryUnique(mem_ai);

		// bind image and memory together
		const size_t mem_offset = 0;
		Core::Inst().m_vk_device->bindImageMemory(*m_vk_image,
												  *m_vk_memory,
												  vk::DeviceSize(mem_offset));

		// always init image view
		init_image_view();
	}

	DepthImage2d(const size_t width,
				 const size_t height,
				 const bool is_downloadable = false):
		DepthImage2d(width,
					 height,
					 vk::ImageTiling::eOptimal,
					 is_downloadable)
	{
	}

	void
	init_image_view()
	{
		vk::ImageViewCreateInfo image_view_ci = {};
		{
			image_view_ci.setImage(*m_vk_image);
			image_view_ci.setViewType(vk::ImageViewType::e2D);
			image_view_ci.setFormat(m_vk_format);
			image_view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);
			image_view_ci.subresourceRange.setBaseMipLevel(0);
			image_view_ci.subresourceRange.setLevelCount(1);
			image_view_ci.subresourceRange.setBaseArrayLayer(0);
			image_view_ci.subresourceRange.setLayerCount(1);
		}
		m_vk_image_view = Core::Inst().m_vk_device->createImageViewUnique(image_view_ci);
	}
};

template <typename ScalarType_>
struct RgbaImage2d
{
	static constexpr uint32_t NumChannels = 4;
	static constexpr vk::ImageUsageFlags RgbaUsageFlagBits = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	using ScalarType = ScalarType_;
	using VectorType = vec<NumChannels, ScalarType_, defaultp>;

	vk::Format					m_vk_format;
	vk::UniqueImage				m_vk_image;
	vk::UniqueDeviceMemory		m_vk_memory;
	vk::UniqueImageView			m_vk_image_view;
	vk::UniqueSampler			m_vk_sampler;
	vk::ImageLayout				m_vk_image_layout;
	size_t						m_width;
	size_t						m_height;
	size_t						m_num_channels;

	RgbaImage2d(const void * data,
				const size_t width,
				const size_t height,
				const vk::ImageTiling & vk_image_tiling,
				const vk::ImageUsageFlags & vk_image_usage_flags,
				const bool use_srgb = false,
				const bool is_downloadable = false):
		m_width(width),
		m_height(height),
		m_vk_image_layout(vk::ImageLayout::eUndefined)
	{
		// choose m_vk_format based on ScalarType
		if constexpr (std::is_same<ScalarType, float>::value)
		{
			m_vk_format = vk::Format::eR32G32B32A32Sfloat;
		}
		else if constexpr (std::is_same<ScalarType, uint8_t>::value)
		{
			if (use_srgb)
			{
				m_vk_format = vk::Format::eR8G8B8A8Srgb;
			}
			else
			{
				m_vk_format = vk::Format::eR8G8B8A8Unorm;
			}
		}
		else
		{
			THROW("unimplemented ScalarType");
		}

		vk::ImageUsageFlags usage_flag = vk_image_usage_flags;
		if (is_downloadable)
		{
			usage_flag |= vk::ImageUsageFlagBits::eTransferSrc;
		}

		// create an image
		vk::ImageCreateInfo image_ci = {};
		{
			image_ci.setImageType(vk::ImageType::e2D);
			image_ci.setExtent(vk::Extent3D(uint32_t(width), uint32_t(height), 1u));
			image_ci.setMipLevels(1);
			image_ci.setArrayLayers(1);
			image_ci.setFormat(m_vk_format);
			image_ci.setTiling(vk_image_tiling);
			image_ci.setInitialLayout(m_vk_image_layout);
			image_ci.setUsage(usage_flag);
			image_ci.setSharingMode(vk::SharingMode::eExclusive);
			image_ci.setSamples(vk::SampleCountFlagBits::e1);
			image_ci.setFlags(vk::ImageCreateFlags(0));
		}
		m_vk_image = Core::Inst().m_vk_device->createImageUnique(image_ci);

		// allocate image memory
		vk::MemoryRequirements mem_requirements = Core::Inst().m_vk_device->getImageMemoryRequirements(*m_vk_image);
		uint32_t mem_type = find_mem_type(Core::Inst().m_vk_physical_device,
										  mem_requirements.memoryTypeBits,
										  vk::MemoryPropertyFlagBits::eDeviceLocal);
		vk::MemoryAllocateInfo mem_ai;
		{
			mem_ai.setAllocationSize(mem_requirements.size);
			mem_ai.setMemoryTypeIndex(mem_type);
		}
		m_vk_memory = Core::Inst().m_vk_device->allocateMemoryUnique(mem_ai);

		// bind image and memory together
		const size_t mem_offset = 0;
		Core::Inst().m_vk_device->bindImageMemory(*m_vk_image,
												  *m_vk_memory,
												  vk::DeviceSize(mem_offset));

		if (data != nullptr)
		{
			copy_from(data,
					  width,
					  height,
					  vk::ImageLayout::eShaderReadOnlyOptimal);
		}
		else if (vk_image_usage_flags & vk::ImageUsageFlagBits::eStorage)
		{
			transition_image_layout(*m_vk_image,
									vk::ImageLayout::eUndefined,
									vk::ImageLayout::eGeneral);
		}

		// always init image view
		init_image_view();
	}

	RgbaImage2d(const size_t width,
				const size_t height,
				const vk::ImageUsageFlags & vk_image_usage_flags,
				const bool is_downloadable = false):
		RgbaImage2d(nullptr,
					width,
					height,
					vk::ImageTiling::eOptimal,
					vk_image_usage_flags,
					is_downloadable)
	{
	}

	RgbaImage2d(const StbiUint8Result & stbi_result,
				const bool is_downloadable = false):
		RgbaImage2d(stbi_result.m_stbi_data,
					stbi_result.m_width,
					stbi_result.m_height,
					vk::ImageTiling::eOptimal,
					RgbaUsageFlagBits,
					true,
					is_downloadable)
	{
		if constexpr (!std::is_same<uint8_t, ScalarType>::value)
		{
			THROW("uint8_t rgbatexture only support non-hdr images loading");
		}
		init_sampler();
	}

	RgbaImage2d(const StbiFloat32Result & stbi_result,
				const bool is_downloadable = false):
		RgbaImage2d(stbi_result.m_stbi_data,
					stbi_result.m_width,
					stbi_result.m_height,
					vk::ImageTiling::eOptimal,
					RgbaUsageFlagBits,
					false,
					is_downloadable)
	{
		if constexpr (!std::is_same<float, ScalarType>::value)
		{
			THROW("float rgbatexture only support hdr images loading");
		}
		init_sampler();
	}

	RgbaImage2d(const std::filesystem::path & filepath,
				const bool is_downloadable = false)
	{
		if (filepath.extension() == ".hdr")
		{
			*this = std::move(RgbaImage2d(StbiFloat32Result(filepath),
										  is_downloadable));
		}
		else
		{
			*this = std::move(RgbaImage2d(StbiUint8Result(filepath),
										  is_downloadable));
		}
	}

	void
	copy_from(const void * data,
			  const size_t width,
			  const size_t height,
			  const vk::ImageLayout & vk_image_layout)
	{
		THROW_ASSERT(width == m_width, "width mismatches");
		THROW_ASSERT(height == m_height, "height mismatches");

		// make image layout optimal for data being transferred to image (TransferDstOptimal)
		transition_image_layout(*m_vk_image,
								m_vk_image_layout,
								vk::ImageLayout::eTransferDstOptimal);

		// upload data from host's stbi_data to device's staging buffer
		Buffer stage(data,
					 width * height * NumChannels * sizeof(ScalarType),
					 1,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					 vk::BufferUsageFlagBits::eTransferSrc);

		// copy from buffer to image
		copy_vk_buffer_to_vk_image(*m_vk_image,
								   vk::ImageLayout::eTransferDstOptimal,
								   *stage.m_vk_buffer,
								   uint32_t(width),
								   uint32_t(height));

		// make image layout optimal for shader reading
		transition_image_layout(*m_vk_image,
								vk::ImageLayout::eTransferDstOptimal,
								vk_image_layout);
	}

	std::vector<ScalarType>
	download()
	{
		const uint32_t size_in_bytes = NumChannels * m_width * m_height * sizeof(ScalarType);
		vk::ImageLayout prev_vk_image_layout = m_vk_image_layout;

		// transit to make image readable as src
		transition_image_layout(*m_vk_image,
								m_vk_image_layout,
								vk::ImageLayout::eTransferSrcOptimal);

		// upload data from host's stbi_data to device's staging buffer
		Buffer stage(nullptr,
					 m_width * m_height * NumChannels * sizeof(ScalarType),
					 1,
					 vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
					 vk::BufferUsageFlagBits::eTransferDst);

		copy_vk_image_to_vk_buffer(*stage.m_vk_buffer,
								   *m_vk_image,
								   m_vk_image_layout,
								   m_width,
								   m_height);

		const vk::Device & device = *Core::Inst().m_vk_device;
		void * mapped_vertex_mem = device.mapMemory(*stage.m_vk_memory,
													vk::DeviceSize(0),
													vk::DeviceSize(size_in_bytes),
													vk::MemoryMapFlagBits());

		std::vector<ScalarType> result(NumChannels * m_width * m_height);
		std::memcpy(result.data(),
					mapped_vertex_mem,
					size_in_bytes);

		// transit back to original image layout
		transition_image_layout(*m_vk_image,
								vk::ImageLayout::eTransferSrcOptimal,
								prev_vk_image_layout);

		return result;
	}

	void
	init_image_view()
	{
		// create image view
		vk::ImageViewCreateInfo image_view_ci = {};
		{
			image_view_ci.setImage(*m_vk_image);
			image_view_ci.setViewType(vk::ImageViewType::e2D);
			image_view_ci.setFormat(m_vk_format);
			image_view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
			image_view_ci.subresourceRange.setBaseMipLevel(0);
			image_view_ci.subresourceRange.setLevelCount(1);
			image_view_ci.subresourceRange.setBaseArrayLayer(0);
			image_view_ci.subresourceRange.setLayerCount(1);
		}
		m_vk_image_view = Core::Inst().m_vk_device->createImageViewUnique(image_view_ci);
	}

	void
	init_sampler()
	{
		// create image sampler
		vk::SamplerCreateInfo sampler_ci = {};
		{
			sampler_ci.setMagFilter(vk::Filter::eLinear);
			sampler_ci.setMinFilter(vk::Filter::eLinear);
			sampler_ci.setAddressModeU(vk::SamplerAddressMode::eRepeat);
			sampler_ci.setAddressModeV(vk::SamplerAddressMode::eRepeat);
			sampler_ci.setAddressModeW(vk::SamplerAddressMode::eRepeat);
			sampler_ci.setAnisotropyEnable(true);
			sampler_ci.setMaxAnisotropy(16);
			sampler_ci.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
			sampler_ci.setUnnormalizedCoordinates(false);
			sampler_ci.setCompareEnable(false);
			sampler_ci.setCompareOp(vk::CompareOp::eAlways);
			sampler_ci.setMipmapMode(vk::SamplerMipmapMode::eLinear);
			sampler_ci.setMipLodBias(0.0f);
			sampler_ci.setMinLod(0.0f);
			sampler_ci.setMaxLod(0.0f);
		}
		m_vk_sampler = Core::Inst().m_vk_device->createSamplerUnique(sampler_ci);
	}

	void
	transition_image_layout(const vk::Image & vk_image,
							const vk::ImageLayout & old_vk_image_layout,
							const vk::ImageLayout & new_vk_image_layout)
	{
		vk::AccessFlags src_access_mask = access_flags_for_layout(old_vk_image_layout);
		vk::AccessFlags dst_access_mask = access_flags_for_layout(new_vk_image_layout);
		vk::PipelineStageFlags src_pipeline_stage_flags = pipeline_stage_flags(old_vk_image_layout);
		vk::PipelineStageFlags dst_pipeline_stage_flags = pipeline_stage_flags(new_vk_image_layout);

		// setup barrier make sure that image is properly setup
		vk::ImageMemoryBarrier img_mem_barrier;
		{
			img_mem_barrier.setOldLayout(old_vk_image_layout);
			img_mem_barrier.setNewLayout(new_vk_image_layout);
			img_mem_barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			img_mem_barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
			img_mem_barrier.setImage(vk_image);
			img_mem_barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
			img_mem_barrier.subresourceRange.setBaseMipLevel(0);
			img_mem_barrier.subresourceRange.setLevelCount(1);
			img_mem_barrier.subresourceRange.setBaseArrayLayer(0);
			img_mem_barrier.subresourceRange.setLayerCount(1);
			img_mem_barrier.setSrcAccessMask(src_access_mask);
			img_mem_barrier.setDstAccessMask(dst_access_mask);
		}

		auto command_func =
			[&](vk::CommandBuffer & command_buffer)
		{
			command_buffer.pipelineBarrier(src_pipeline_stage_flags,
										   dst_pipeline_stage_flags,
										   vk::DependencyFlagBits(0),
										   nullptr,
										   nullptr,
										   { img_mem_barrier });
		};

		Core::Inst().one_time_command_submit(command_func);

		m_vk_image_layout = new_vk_image_layout;
	}

private:

	vk::PipelineStageFlags pipeline_stage_flags(const vk::ImageLayout vk_image_layout)
	{
		switch (vk_image_layout)
		{
			case vk::ImageLayout::eTransferDstOptimal:
			case vk::ImageLayout::eTransferSrcOptimal:
				return vk::PipelineStageFlagBits::eTransfer;
			case vk::ImageLayout::eColorAttachmentOptimal:
				return vk::PipelineStageFlagBits::eColorAttachmentOutput;
			case vk::ImageLayout::eDepthStencilAttachmentOptimal:
				return vk::PipelineStageFlagBits::eEarlyFragmentTests;
			case vk::ImageLayout::eShaderReadOnlyOptimal:
				return vk::PipelineStageFlagBits::eFragmentShader;
			case vk::ImageLayout::ePreinitialized:
				return vk::PipelineStageFlagBits::eHost;
			case vk::ImageLayout::eUndefined:
				return vk::PipelineStageFlagBits::eTopOfPipe;
			default:
				return vk::PipelineStageFlagBits::eBottomOfPipe;
		}
	}

	vk::AccessFlags access_flags_for_layout(vk::ImageLayout layout)
	{
		switch (layout)
		{
			case vk::ImageLayout::ePreinitialized:
				return vk::AccessFlagBits::eHostWrite;
			case vk::ImageLayout::eTransferDstOptimal:
				return vk::AccessFlagBits::eTransferWrite;
			case vk::ImageLayout::eTransferSrcOptimal:
				return vk::AccessFlagBits::eTransferRead;
			case vk::ImageLayout::eColorAttachmentOptimal:
				return vk::AccessFlagBits::eColorAttachmentWrite;
			case vk::ImageLayout::eDepthStencilAttachmentOptimal:
				return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			case vk::ImageLayout::eShaderReadOnlyOptimal:
				return vk::AccessFlagBits::eShaderRead;
			default:
				return vk::AccessFlags();
		}
	}

	void
	copy_vk_buffer_to_vk_image(const vk::Image & vk_image,
							   const vk::ImageLayout & vk_image_layout,
							   const vk::Buffer & src_vk_buffer,
							   const uint32_t width,
							   const uint32_t height)
	{
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer& command_buffer)
			{
				vk::BufferImageCopy buffer_image_copy = {};
				{
					buffer_image_copy.setBufferOffset(0);
					buffer_image_copy.setBufferRowLength(0);
					buffer_image_copy.setBufferImageHeight(0);
					buffer_image_copy.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
					buffer_image_copy.imageSubresource.setMipLevel(0);
					buffer_image_copy.imageSubresource.setBaseArrayLayer(0);
					buffer_image_copy.imageSubresource.setLayerCount(1);
					buffer_image_copy.setImageOffset({ 0, 0, 0 });
					buffer_image_copy.setImageExtent({ width, height, 1u });
				}
				command_buffer.copyBufferToImage(src_vk_buffer,
												 vk_image,
												 vk_image_layout,
												 { buffer_image_copy });
			});
	}

	void
	copy_vk_image_to_vk_buffer(const vk::Buffer & dst_vk_buffer,
							   const vk::Image & src_vk_image,
							   const vk::ImageLayout & src_vk_image_layout,
							   const uint32_t width,
							   const uint32_t height)
	{
		Core::Inst().one_time_command_submit(
			[&](vk::CommandBuffer& command_buffer)
			{
				vk::BufferImageCopy buffer_image_copy = {};
				{
					buffer_image_copy.setBufferOffset(0);
					buffer_image_copy.setBufferRowLength(0);
					buffer_image_copy.setBufferImageHeight(0);
					buffer_image_copy.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
					buffer_image_copy.imageSubresource.setMipLevel(0);
					buffer_image_copy.imageSubresource.setBaseArrayLayer(0);
					buffer_image_copy.imageSubresource.setLayerCount(1);
					buffer_image_copy.setImageOffset({ 0, 0, 0 });
					buffer_image_copy.setImageExtent({ width, height, 1u });
				}

				command_buffer.copyImageToBuffer(src_vk_image,
												 src_vk_image_layout,
												 dst_vk_buffer,
												 { buffer_image_copy });
			});
	}
};