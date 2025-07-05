//
// Created by capma on 7/5/2025.
//

#include "Buffer.h"

void Buffer::Copy(vk::Buffer SourceBuffer, VmaAllocationInfo SourceInfo, vk::Buffer DestBuffer,
    VmaAllocationInfo DestInfo, vk::Queue Queue, vk::CommandBuffer CommandBuffer) {

    CommandBuffer.reset();

    vk::CommandBufferBeginInfo BufferInfo;
    BufferInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    CommandBuffer.begin(BufferInfo);

    vk::BufferCopy copyRegion;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = SourceInfo.size;

    CommandBuffer.copyBuffer(SourceBuffer, DestBuffer, 1, &copyRegion);
    CommandBuffer.end();

    vk::SubmitInfo SubmitInfo;
    SubmitInfo.setCommandBufferCount(1);
    SubmitInfo.setPCommandBuffers(&CommandBuffer);
    Queue.submit(1, &SubmitInfo,nullptr);
    Queue.waitIdle();

}
