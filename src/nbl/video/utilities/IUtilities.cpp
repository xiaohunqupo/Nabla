#include "nbl/video/utilities/IUtilities.h"
#include "nbl/video/utilities/ImageRegionIterator.h"
//#include <numeric>

namespace nbl::video
{

bool IUtilities::updateImageViaStagingBuffer(
    SIntendedSubmitInfo& intendedNextSubmit, const void* srcData, asset::E_FORMAT srcFormat,
    IGPUImage* dstImage, IGPUImage::LAYOUT currentDstImageLayout,
    const std::span<const asset::IImage::SBufferCopy> regions
)
{
    auto* scratch = commonTransferValidation(intendedNextSubmit);
    if (!scratch)
        return false;
   
    const auto& limits = m_device->getPhysicalDevice()->getLimits();
 
    if (regions.size() == 0)
        return false; // won't log an error cause its not one
    
    if (!srcData || !dstImage)
    {
        m_logger.log("Invalid `srcData` or `dstImage` cannot `updateImageViaStagingBuffer`.", nbl::system::ILogger::ELL_ERROR);
        return false;
    }
    

    if (dstImage->getCreationParameters().samples != asset::IImage::E_SAMPLE_COUNT_FLAGS::ESCF_1_BIT)
    {
        _NBL_TODO(); // "Erfan hasn't figured out yet how to copy to multisampled images"
        return false;
    }

    const auto& queueFamProps = m_device->getPhysicalDevice()->getQueueFamilyProperties()[intendedNextSubmit.queue->getFamilyIndex()];
    auto texelBlockInfo = asset::TexelBlockInfo(dstImage->getCreationParameters().format);
    auto minGranularity = queueFamProps.minImageTransferGranularity;
    
    assert(dstImage->getCreationParameters().format != asset::EF_UNKNOWN);
    if (srcFormat == asset::EF_UNKNOWN)
    {
        // If valid srcFormat is not provided, assume srcBuffer is laid out in memory based on dstImage format
        srcFormat = dstImage->getCreationParameters().format;
    }

    // Validate Copies from srcBuffer to dstImage with these regions
    // if the initial regions are valid then ImageRegionIterator will do it's job correctly breaking it down ;)
    // note to future self: couldn't use dstImage->validateCopies because it doesn't consider that cpubuffer will be promoted and hence it will get a validation error about size of the buffer being smaller than max accessible offset.
    bool regionsValid = true;
    for (const auto region : regions)
    {
        auto subresourceSize = dstImage->getMipSize(region.imageSubresource.mipLevel);
        if (!dstImage->validateCopyOffsetAndExtent(region.imageExtent, region.imageOffset, subresourceSize, minGranularity))
            regionsValid = false;
    }
    if (!regionsValid)
    {
        m_logger.log("Invalid regions to copy cannot `updateImageViaStagingBuffer`.", nbl::system::ILogger::ELL_ERROR);
        return false;
    }

    ImageRegionIterator regionIterator(regions, queueFamProps, srcData, srcFormat, dstImage, limits.optimalBufferCopyRowPitchAlignment);

    // TODO: Why did we settle on `/4` ? It definitely wasn't about the uint32_t size!
    // Assuming each thread can handle minImageTranferGranularitySize of texelBlocks:
    const uint32_t maxResidentImageTransferSize = core::min<uint32_t>(limits.maxResidentInvocations*minGranularity.depth*minGranularity.height*minGranularity.width*texelBlockInfo.getBlockByteSize(),m_defaultUploadBuffer->get_total_size()/4);

    core::vector<asset::IImage::SBufferCopy> regionsToCopy;

    // Worst case iterations: remaining blocks --> remaining rows --> remaining slices --> full layers
    const uint32_t maxIterations = regions.size() * 4u;

    regionsToCopy.reserve(maxIterations);

    core::vector<ILogicalDevice::MappedMemoryRange> flushRanges;
    const bool manualFlush = m_defaultUploadBuffer.get()->needsManualFlushOrInvalidate();
    if (manualFlush)
        flushRanges.reserve(maxIterations);

    auto* uploadBuffer = m_defaultUploadBuffer.get()->getBuffer();
    // for the signal to be useful for us to let go of memory, we need to signal after transfer is finished
    const auto oldScratchStage = intendedNextSubmit.scratchSemaphore.stageMask|=asset::PIPELINE_STAGE_FLAGS::COPY_BIT;
    while (!regionIterator.isFinished())
    {
        size_t memoryNeededForRemainingRegions = regionIterator.getMemoryNeededForRemainingRegions();

        uint32_t memoryLowerBound = maxResidentImageTransferSize;
        {
            const asset::IImage::SBufferCopy & region = regions[regionIterator.getCurrentRegion()];
            const auto copyTexelStrides = regionIterator.getOptimalCopyTexelStrides(region.imageExtent);
            const auto byteStrides = texelBlockInfo.convert3DTexelStridesTo1DByteStrides(copyTexelStrides);
            memoryLowerBound = core::max(memoryLowerBound, byteStrides[1]); // max of memoryLowerBound and copy rowPitch
        }

        uint32_t localOffset = video::StreamingTransientDataBufferMT<>::invalid_value;
        uint32_t maxFreeBlock = m_defaultUploadBuffer.get()->max_size();
        const uint32_t allocationSize = getAllocationSizeForStreamingBuffer(memoryNeededForRemainingRegions, m_allocationAlignmentForBufferImageCopy, maxFreeBlock, memoryLowerBound);
        // cannot use `multi_place` because of the extra padding size we could have added
        m_defaultUploadBuffer.get()->multi_allocate(std::chrono::steady_clock::now()+std::chrono::microseconds(500u), 1u, &localOffset, &allocationSize, &m_allocationAlignmentForBufferImageCopy);
        bool failedAllocation = (localOffset == video::StreamingTransientDataBufferMT<>::invalid_value);

        // keep trying again
        if (failedAllocation)
        {
            if (!flushRanges.empty())
            {
                m_device->flushMappedMemoryRanges(flushRanges);
                flushRanges.clear();
            }
            const auto completed = intendedNextSubmit.getFutureScratchSemaphore();
            intendedNextSubmit.overflowSubmit(scratch);
            // first submit we respect whatever stages the user had (maybe they wanted to be notified of the completion of `nextSubmit.prevCommandBuffers`
            intendedNextSubmit.scratchSemaphore.stageMask = asset::PIPELINE_STAGE_FLAGS::COPY_BIT;
            // overflowSubmit no longer blocks for the last submit to have completed, so we must do it ourselves here
            // TODO: if we cleverly overflowed BEFORE completely running out of memory (better heuristics) then we wouldn't need to do this and some CPU-GPU overlap could be achieved
            if (intendedNextSubmit.overflowCallback)
                intendedNextSubmit.overflowCallback(completed);
            m_device->blockForSemaphores({&completed,1});
            m_defaultUploadBuffer->cull_frees(); // is this even needed anymore?
            continue;
        }
        else
        {
            uint32_t currentUploadBufferOffset = localOffset;
            uint32_t availableUploadBufferMemory = allocationSize;

            regionsToCopy.clear();
            for (uint32_t d = 0u; d < maxIterations && !regionIterator.isFinished(); ++d)
            {
                asset::IImage::SBufferCopy nextRegionToCopy = {};
                if (availableUploadBufferMemory > 0u && regionIterator.advanceAndCopyToStagingBuffer(nextRegionToCopy, availableUploadBufferMemory, currentUploadBufferOffset, m_defaultUploadBuffer->getBufferPointer()))
                {
                    regionsToCopy.push_back(nextRegionToCopy);
                }
                else
                    break;
            }

            if (!regionsToCopy.empty())
                scratch->cmdbuf->copyBufferToImage(uploadBuffer, dstImage, currentDstImageLayout, regionsToCopy.size(), regionsToCopy.data());

            assert(!regionsToCopy.empty() && "allocationSize is not enough to support the smallest possible transferable units to image, may be caused if your queueFam's minImageTransferGranularity is large or equal to <0,0,0>.");
            
            // some platforms expose non-coherent host-visible GPU memory, so writes need to be flushed explicitly
            if (manualFlush)
            {
                const auto consumedMemory = allocationSize - availableUploadBufferMemory;
                flushRanges.emplace_back(uploadBuffer->getBoundMemory().memory, localOffset, consumedMemory, ILogicalDevice::MappedMemoryRange::align_non_coherent_tag);
            }
        }

        // this doesn't actually free the memory, the memory is queued up to be freed only after the GPU fence/event is signalled
        m_defaultUploadBuffer.get()->multi_deallocate(1u,&localOffset,&allocationSize,intendedNextSubmit.getFutureScratchSemaphore()); // can queue with a reset but not yet pending fence, just fine
    }
    intendedNextSubmit.scratchSemaphore.stageMask = oldScratchStage;
    if (!flushRanges.empty())
        m_device->flushMappedMemoryRanges(flushRanges);
    return true;
}

bool IUtilities::downloadImageViaStagingBuffer(
    SIntendedSubmitInfo& intendedNextSubmit, const IGPUImage* srcImage, const IGPUImage::LAYOUT currentSrcImageLayout,
    void* dest, const std::span<const asset::IImage::SBufferCopy> regions
)
{
    if (regions.empty())
        return false;

    _NBL_TODO();

    return true;
}

} // namespace nbl::video