#pragma once

#include "VulkanAllocator.h"

class VulkanShadow
{
public:
	
	VulkanShadow(const class VulkanDevice* device);
	
	~VulkanShadow();
	
	VkRect2D renderArea;
	
	VkRenderPass renderPass;
	VkFramebuffer framebuffer;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	
private:
	
	const class VulkanDevice* const device;
	
	VulkanImage image;
	VkImageView imageView;
	VkSampler sampler;
};
