// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#define _NBL_STATIC_LIB_
#include <iostream>
#include <cstdio>
#include <nabla.h>

//! I advise to check out this file, its a basic input handler
#include "../common/QToQuitEventReceiver.h"
#include "nbl/asset/utils/CCPUMeshPackerV1.h"
#include "nbl/asset/CCPUMeshPackerV2.h"

using namespace nbl;
using namespace core;
using namespace asset;
using namespace video;

constexpr const char* VERTEX_SHADER_OVERRIDES =
R"(
#define _NBL_VERT_INPUTS_DEFINED_

#define nbl_glsl_VirtualAttribute_t uint

layout(location = 4) flat out uint drawID; //TODO: override main

vec4 nbl_glsl_decodeRGB10A2_UNORM(in uint x)
{
	const uvec3 rgbMask = uvec3(0x3ffu);
	const uvec4 shifted = uvec4(x,uvec3(x)>>uvec3(10,20,30));
	return vec4(vec3(shifted.rgb&rgbMask),shifted.a)/vec4(vec3(rgbMask),3.0);
}

vec4 nbl_glsl_decodeRGB10A2_SNORM(in uint x)
{
    uvec4 shifted = uvec4(x,uvec3(x)>>uvec3(10,20,30));
    const uvec3 rgbMask = uvec3(0x3ffu);
    const uvec3 rgbBias = uvec3(0x200u);
    return max(vec4(vec3(shifted.rgb&rgbMask)-rgbBias,float(shifted.a)-2.0)/vec4(vec3(rgbBias-uvec3(1u)),1.0),vec4(-1.0));
}

//pos
layout(set = 0, binding = 0) uniform samplerBuffer MeshPackedDataFloat[2];

//uv
layout(set = 0, binding = 1) uniform isamplerBuffer MeshPackedDataInt[1];

//normal
layout(set = 0, binding = 2) uniform usamplerBuffer MeshPackedDataUint[1];

layout(set = 0, binding = 3) readonly buffer VirtualAttributes
{
    nbl_glsl_VirtualAttribute_t vAttr[][3];
} virtualAttribTable;

#define _NBL_BASIC_VTX_ATTRIB_FETCH_FUCTIONS_DEFINED_
#define _NBL_POS_FETCH_FUNCTION_DEFINED
#define _NBL_UV_FETCH_FUNCTION_DEFINED
#define _NBL_NORMAL_FETCH_FUNCTION_DEFINED

//vec4 nbl_glsl_readAttrib(uint offset)
//ivec4 nbl_glsl_readAttrib(uint offset)
//uvec4 nbl_glsl_readAttrib(uint offset)
//vec3 nbl_glsl_readAttrib(uint offset) 
//..

struct VirtualAttribute
{
    uint binding;
    int offset;
};

VirtualAttribute unpackVirtualAttribute(in nbl_glsl_VirtualAttribute_t vaPacked)
{
    VirtualAttribute result;
    result.binding = bitfieldExtract(vaPacked, 0, 4);
    result.offset = int(bitfieldExtract(vaPacked, 4, 28));
    
    return result;
}

layout (push_constant) uniform Block 
{
    uint dataBufferOffset;
} pc;

vec3 nbl_glsl_fetchVtxPos(in uint vtxID)
{
    VirtualAttribute va = unpackVirtualAttribute(virtualAttribTable.vAttr[gl_DrawID + pc.dataBufferOffset][0]);
    return texelFetch(MeshPackedDataFloat[va.binding], va.offset + int(vtxID)).xyz;
}

vec2 nbl_glsl_fetchVtxUV(in uint vtxID)
{
    VirtualAttribute va = unpackVirtualAttribute(virtualAttribTable.vAttr[gl_DrawID + pc.dataBufferOffset][1]);
    return texelFetch(MeshPackedDataFloat[va.binding], va.offset + int(vtxID)).xy;
}

vec3 nbl_glsl_fetchVtxNormal(in uint vtxID)
{
    VirtualAttribute va = unpackVirtualAttribute(virtualAttribTable.vAttr[gl_DrawID + pc.dataBufferOffset][2]);
    return nbl_glsl_decodeRGB10A2_SNORM(texelFetch(MeshPackedDataUint[va.binding], va.offset + int(vtxID)).x).xyz;
}

)";

constexpr const char* FRAGMENT_SHADER_OVERRIDES = //also turns off set3 bindings (textures) because they're not needed anymore as we're using VT
R"(
layout(location = 4) flat in uint drawID;

#ifndef _NO_UV
    #include <nbl/builtin/glsl/virtual_texturing/extensions.glsl>

    #define _NBL_VT_DESCRIPTOR_SET 0
    #define _NBL_VT_PAGE_TABLE_BINDING 4

    #define _NBL_VT_FLOAT_VIEWS_BINDING 5 
    #define _NBL_VT_FLOAT_VIEWS_COUNT %u
    #define _NBL_VT_FLOAT_VIEWS

    #define _NBL_VT_INT_VIEWS_BINDING 6
    #define _NBL_VT_INT_VIEWS_COUNT 0
    #define _NBL_VT_INT_VIEWS

    #define _NBL_VT_UINT_VIEWS_BINDING 7
    #define _NBL_VT_UINT_VIEWS_COUNT 0
    #define _NBL_VT_UINT_VIEWS
    #include <nbl/builtin/glsl/virtual_texturing/descriptors.glsl>

    layout (set = 2, binding = 0, std430) restrict readonly buffer PrecomputedStuffSSBO
    {
        uint pgtab_sz_log2;
        float vtex_sz_rcp;
        float phys_pg_tex_sz_rcp[_NBL_VT_MAX_PAGE_TABLE_LAYERS];
        uint layer_to_sampler_ix[_NBL_VT_MAX_PAGE_TABLE_LAYERS];
    } precomputed;
#endif
#define _NBL_FRAG_SET3_BINDINGS_DEFINED_

struct VTData
{
    vec3 Ka;
    vec3 Kd;
    vec3 Ks;
    vec3 Ke;
    uvec2 map_Ka_data;
    uvec2 map_Kd_data;
    uvec2 map_Ks_data;
    uvec2 map_Ns_data;
    uvec2 map_d_data;
    uvec2 map_bump_data;
    float Ns;
    float d;
    float Ni;
    uint extra; //flags copied from MTL metadata
};
#define _NBL_FRAG_MATERIAL_PARAMETERS_STRUCT_DEFINED_
#define nbl_glsl_MaterialParametersStruct VTData

layout(set = 2, binding = 1, std430) readonly buffer VTDataBuffer
{
    VTData vtData[];
} vtDataBuffer;

layout (push_constant) uniform Block 
{
    uint dataBufferOffset;
} pc;
#define _NBL_FRAG_PUSH_CONSTANTS_DEFINED_

VTData nbl_glsl_getMaterialParameters()
{
    return vtDataBuffer.vtData[drawID + pc.dataBufferOffset];
}
#define _NBL_FRAG_GET_MATERIAL_PARAMETERS_FUNCTION_DEFINED_

#ifndef _NO_UV
    uint nbl_glsl_VT_layer2pid(in uint layer)
    {
        return precomputed.layer_to_sampler_ix[layer];
    }
    uint nbl_glsl_VT_getPgTabSzLog2()
    {
        return precomputed.pgtab_sz_log2;
    }
    float nbl_glsl_VT_getPhysPgTexSzRcp(in uint layer)
    {
        return precomputed.phys_pg_tex_sz_rcp[layer];
    }
    float nbl_glsl_VT_getVTexSzRcp()
    {
        return precomputed.vtex_sz_rcp;
    }
    #define _NBL_USER_PROVIDED_VIRTUAL_TEXTURING_FUNCTIONS_

    //nbl/builtin/glsl/virtual_texturing/functions.glsl/...
    #include <%s>
#endif


#ifndef _NO_UV
    vec4 nbl_sample_Ka(in vec2 uv, in mat2 dUV)   { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_Ka_data, uv, dUV); }

    vec4 nbl_sample_Kd(in vec2 uv, in mat2 dUV)   { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_Kd_data, uv, dUV); }

    vec4 nbl_sample_Ks(in vec2 uv, in mat2 dUV)   { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_Ks_data, uv, dUV); }

    vec4 nbl_sample_Ns(in vec2 uv, in mat2 dUV)   { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_Ns_data, uv, dUV); }

    vec4 nbl_sample_d(in vec2 uv, in mat2 dUV)    { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_d_data, uv, dUV); }

    vec4 nbl_sample_bump(in vec2 uv, in mat2 dUV) { return nbl_glsl_vTextureGrad(nbl_glsl_getMaterialParameters().map_bump_data, uv, dUV); }
#endif
#define _NBL_TEXTURE_SAMPLE_FUNCTIONS_DEFINED_
)";

    constexpr uint32_t TEX_OF_INTEREST_CNT = 6u; 
#include "nbl/nblpack.h"
struct SPushConstants //TODO: rename
{
    //Ka
    core::vector3df_SIMD ambient;
    //Kd
    core::vector3df_SIMD diffuse;
    //Ks
    core::vector3df_SIMD specular;
    //Ke
    core::vector3df_SIMD emissive;
    uint64_t map_data[TEX_OF_INTEREST_CNT];
    //Ns, specular exponent in phong model
    float shininess = 32.f;
    //d
    float opacity = 1.f;
    //Ni, index of refraction
    float IoR = 1.6f;
    uint32_t extra;
} PACK_STRUCT;
#include "nbl/nblunpack.h"
static_assert(sizeof(SPushConstants) <= asset::ICPUMeshBuffer::MAX_PUSH_CONSTANT_BYTESIZE, "doesnt fit in push constants");

core::smart_refctd_ptr<asset::ICPUSpecializedShader> createModifiedVertexShader(const asset::ICPUSpecializedShader* _fs)
{
    const asset::ICPUShader* unspec = _fs->getUnspecialized();
    assert(unspec->containsGLSL());

    auto begin = reinterpret_cast<const char*>(unspec->getSPVorGLSL()->getPointer());
    auto end = begin + unspec->getSPVorGLSL()->getSize();
    std::string resultShaderSrc(begin, end);

    size_t firstNewlineAfterVersion = resultShaderSrc.find("\n", resultShaderSrc.find("#version "));

    const std::string customSrcCode = VERTEX_SHADER_OVERRIDES;

    resultShaderSrc.insert(firstNewlineAfterVersion, customSrcCode);
    resultShaderSrc.replace(resultShaderSrc.find("#version 430 core"), sizeof("#version 430 core"), "#version 460 core\n");

    auto unspecNew = core::make_smart_refctd_ptr<asset::ICPUShader>(resultShaderSrc.c_str());
    auto specinfo = _fs->getSpecializationInfo();
    auto vsNew = core::make_smart_refctd_ptr<asset::ICPUSpecializedShader>(std::move(unspecNew), std::move(specinfo));

    return vsNew;
}

core::smart_refctd_ptr<asset::ICPUSpecializedShader> createModifiedFragShader(const asset::ICPUSpecializedShader* _fs, const asset::ICPUVirtualTexture* _vt)
{
    const asset::ICPUShader* unspec = _fs->getUnspecialized();
    assert(unspec->containsGLSL());

    auto begin = reinterpret_cast<const char*>(unspec->getSPVorGLSL()->getPointer());
    auto end = begin + unspec->getSPVorGLSL()->getSize();
    std::string glsl(begin, end);

    std::string prelude(strlen(FRAGMENT_SHADER_OVERRIDES) + 500u, '\0');
    sprintf(prelude.data(), FRAGMENT_SHADER_OVERRIDES, _vt->getFloatViews().size(), _vt->getGLSLFunctionsIncludePath().c_str());
    prelude.resize(strlen(prelude.c_str()));

    size_t firstNewlineAfterVersion = glsl.find("\n", glsl.find("#version "));
    glsl.insert(firstNewlineAfterVersion, prelude);

    auto* f = fopen("fs.glsl", "w");
    fwrite(glsl.c_str(), 1, glsl.size(), f);
    fclose(f);

    auto unspecNew = core::make_smart_refctd_ptr<asset::ICPUShader>(glsl.c_str());
    auto specinfo = _fs->getSpecializationInfo();//intentional copy
    auto fsNew = core::make_smart_refctd_ptr<asset::ICPUSpecializedShader>(std::move(unspecNew), std::move(specinfo));

    return fsNew;
}

//mesh packing stuff

struct DrawIndexedIndirectInput
{
    size_t offset = 0u;
    size_t maxCount = 0u;

    static constexpr asset::E_PRIMITIVE_TOPOLOGY mode = asset::EPT_TRIANGLE_LIST;
    static constexpr asset::E_INDEX_TYPE indexType = asset::EIT_16BIT;
    static constexpr size_t stride = sizeof(DrawElementsIndirectCommand_t);
};

using Range_t = SRange<void, core::vector<ICPUMeshBuffer*>::iterator>;
using MbPipelineRange = std::pair<ICPURenderpassIndependentPipeline*, Range_t>;

core::vector<MbPipelineRange> sortMeshBuffersByPipeline(core::vector<ICPUMeshBuffer*>& meshBuffers)
{
    core::vector<MbPipelineRange> output;

    if (meshBuffers.empty())
        return output;

    auto sortFunc = [](ICPUMeshBuffer* lhs, ICPUMeshBuffer* rhs)
    {
        return lhs->getPipeline() < rhs->getPipeline();
    };
    std::sort(meshBuffers.begin(), meshBuffers.end(), sortFunc);

    auto mbPipeline = (*meshBuffers.begin())->getPipeline();
    auto rangeBegin = meshBuffers.begin();

    for (auto it = meshBuffers.begin() + 1; ; it++)
    {
        if (it == meshBuffers.end())
        {
            output.push_back(std::make_pair(mbPipeline, Range_t(rangeBegin, meshBuffers.end())));
            break;
        }

        if ((*it)->getPipeline() != mbPipeline)
        {
            output.push_back(std::make_pair(mbPipeline, Range_t(rangeBegin, it)));
            rangeBegin = it;
            mbPipeline = (*rangeBegin)->getPipeline();
        }
    }

    return output;
}

struct DrawData
{
    core::vector<MbPipelineRange> ranges;
    core::vector<core::smart_refctd_ptr<IGPURenderpassIndependentPipeline>> gpuPipelines;
    core::vector<DrawIndexedIndirectInput> drawIndirectInput;
    core::smart_refctd_ptr<IGPUBuffer> vtDataSSBO;
    core::vector<uint32_t> pushConstantsData;
    std::array<core::smart_refctd_ptr<IGPUDescriptorSet>, 3> ds;
    core::smart_refctd_ptr<IGPUVirtualTexture> vt;
    core::smart_refctd_ptr<IGPUBuffer> virtualAttribTable;
    core::smart_refctd_ptr<IGPUBuffer> ubo;

    asset::SBufferBinding<IGPUBuffer> vtxBindings[IGPUMeshBuffer::MAX_ATTR_BUF_BINDING_COUNT];
    core::smart_refctd_ptr<IGPUBuffer> idxBuffer;
    core::smart_refctd_ptr<IGPUBuffer> mdiBuffer;
};

void packMeshBuffers(video::IVideoDriver* driver, DrawData& drawData)
{
    using MeshPacker = CCPUMeshPackerV2<DrawElementsIndirectCommand_t>;

    MeshPacker::AllocationParams allocParams;
    allocParams.indexBuffSupportedCnt = 20000000u;
    allocParams.indexBufferMinAllocSize = 5000u;
    allocParams.vertexBuffSupportedSize = 200000000u;
    allocParams.vertexBufferMinAllocSize = 5000u;
    allocParams.MDIDataBuffSupportedCnt = 20000u;
    allocParams.MDIDataBuffMinAllocSize = 1u; //so structs are adjacent in memory
    
    assert(!drawData.ranges.empty());
    
    CCPUMeshPackerV2 mp(allocParams, std::numeric_limits<uint16_t>::max() / 3u, std::numeric_limits<uint16_t>::max() / 3u);

    auto wholeMbRangeBegin = drawData.ranges.begin()->second.begin();
    auto wholeMbRangeEnd = (drawData.ranges.end() - 1)->second.end();

    const uint32_t mdiCntTotal = mp.calcMDIStructCount(wholeMbRangeBegin, wholeMbRangeEnd);
    auto allocData = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<MeshPacker::ReservedAllocationMeshBuffers>>(mdiCntTotal);

    const uint32_t offsetTableSz = mdiCntTotal * 3u;
    core::vector<MeshPacker::VirtualAttribute> offsetTableLocal;
    offsetTableLocal.reserve(offsetTableSz);

    uint32_t offsetForDrawCall = 0u;
    for (auto mbRange : drawData.ranges)
    {
        auto mbRangeBegin = mbRange.second.begin();
        auto mbRangeEnd = mbRange.second.end();

        const uint32_t mdiCnt = mp.calcMDIStructCount(mbRangeBegin, mbRangeEnd);

        bool allocSuccessfull = mp.alloc(allocData->data() + offsetForDrawCall, mbRangeBegin, mbRangeEnd);
        if (!allocSuccessfull)
        {
            std::cout << "Alloc failed \n";
            _NBL_DEBUG_BREAK_IF(true);
        }

        drawData.pushConstantsData.push_back(offsetForDrawCall);
        offsetForDrawCall += mdiCnt;
    }

    mp.instantiateDataStorage();
    MeshPacker::PackerDataStore packerDataStore = mp.getPackerDataStore();

    for (uint32_t i = 0u; i < drawData.ranges.size(); i++)
    {
        auto mbRange = drawData.ranges[i].second;
        auto mbRangeBegin = mbRange.begin();
        auto mbRangeEnd = mbRange.end();

        const uint32_t mdiCnt = mp.calcMDIStructCount(mbRangeBegin, mbRangeEnd);

        core::vector<IMeshPackerBase::PackedMeshBufferData> pmbd(mdiCnt);

        core::vector<MeshPacker::CombinedDataOffsetTable> cdot(offsetTableSz);

        bool commitSuccessfull = mp.commit(pmbd.data(), cdot.data(), allocData->data() + drawData.pushConstantsData[i], mbRangeBegin, mbRangeEnd);
        if (!commitSuccessfull)
        {
            std::cout << "Commit failed \n";
            _NBL_DEBUG_BREAK_IF(true);
        }

        DrawIndexedIndirectInput mdiCallInput;

        mdiCallInput.maxCount = mdiCnt;

        drawData.drawIndirectInput.push_back(std::move(mdiCallInput));

        //auto glsl = mp.generateGLSLBufferDefinitions(0u);

        //setOffsetTables
        for (uint32_t i = 0u; i < mdiCnt; i++)
        {
            MeshPacker::CombinedDataOffsetTable& virtualAttribTable = cdot[i];

            offsetTableLocal.push_back(virtualAttribTable.attribInfo[0]);
            offsetTableLocal.push_back(virtualAttribTable.attribInfo[2]);
            offsetTableLocal.push_back(virtualAttribTable.attribInfo[3]);
        }

        /*DrawElementsIndirectCommand_t* mdiPtr = static_cast<DrawElementsIndirectCommand_t*>(packerDataStore.MDIDataBuffer->getPointer()) + 99u;
        uint16_t* idxBuffPtr = static_cast<uint16_t*>(packerDataStore.indexBuffer->getPointer());
        float* vtxBuffPtr = static_cast<float*>(packerDataStore.vertexBuffer->getPointer());

        for (uint32_t i = 0u; i < 264; i++)
        {
            float* firstCoord = vtxBuffPtr + ((*(idxBuffPtr + i) + cdot[99].attribInfo[0].offset) * 3u);
            std::cout << "vtx: " << i << " idx: " << *(idxBuffPtr + i) << "    ";
            std::cout << *firstCoord << ' ' << *(firstCoord + 1u) << ' ' << *(firstCoord + 2u) << std::endl;
        }*/
    }

    //prepare data for (set = 2, binding = 1) frag shader ssbo
    {
        //TODO: this is shit and `calcMDIStructCount` should be cashed so it would't be called so often, rewrite it
        core::vector<SPushConstants> vtData;
        vtData.reserve(mdiCntTotal);

        for (auto it = wholeMbRangeBegin; it != wholeMbRangeEnd; it++)
        {
            const uint32_t mdiCntForThisMb = mp.calcMDIStructCount(it, it + 1);
            for (uint32_t i = 0u; i < mdiCntForThisMb; i++)
                vtData.push_back(*reinterpret_cast<SPushConstants*>((*it)->getPushConstantsDataPtr()));
        }

        drawData.vtDataSSBO = driver->createFilledDeviceLocalGPUBufferOnDedMem(vtData.size() * sizeof(SPushConstants), vtData.data());
    }

    drawData.vtxBindings[0] = { 0ull, driver->createFilledDeviceLocalGPUBufferOnDedMem(packerDataStore.vertexBuffer->getSize(), packerDataStore.vertexBuffer->getPointer()) };
    drawData.idxBuffer = driver->createFilledDeviceLocalGPUBufferOnDedMem(packerDataStore.indexBuffer->getSize(), packerDataStore.indexBuffer->getPointer());
    drawData.mdiBuffer = driver->createFilledDeviceLocalGPUBufferOnDedMem(packerDataStore.MDIDataBuffer->getSize(), packerDataStore.MDIDataBuffer->getPointer());

    drawData.virtualAttribTable = driver->createFilledDeviceLocalGPUBufferOnDedMem(offsetTableLocal.size() * sizeof(MeshPacker::VirtualAttribute), offsetTableLocal.data());
}

//vt stuff

using STextureData = asset::ICPUVirtualTexture::SMasterTextureData;

constexpr uint32_t PAGE_SZ_LOG2 = 7u;
constexpr uint32_t TILES_PER_DIM_LOG2 = 4u;
constexpr uint32_t PAGE_PADDING = 8u;
constexpr uint32_t MAX_ALLOCATABLE_TEX_SZ_LOG2 = 12u; //4096

constexpr uint32_t VT_SET = 0u;
constexpr uint32_t PGTAB_BINDING = 4u;
constexpr uint32_t PHYSICAL_STORAGE_VIEWS_BINDING = 5u;

struct commit_t
{
    STextureData addr;
    core::smart_refctd_ptr<asset::ICPUImage> texture;
    asset::ICPUImage::SSubresourceRange subresource;
    asset::ICPUSampler::E_TEXTURE_CLAMP uwrap;
    asset::ICPUSampler::E_TEXTURE_CLAMP vwrap;
    asset::ICPUSampler::E_TEXTURE_BORDER_COLOR border;
};

constexpr uint32_t texturesOfInterest[TEX_OF_INTEREST_CNT]{
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_AMBIENT,
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_DIFFUSE,
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_SPECULAR,
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_SHININESS,
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_OPACITY,
    asset::CMTLMetadata::CRenderpassIndependentPipeline::EMP_BUMP
};

STextureData getTextureData(core::vector<commit_t>& _out_commits, const asset::ICPUImage* _img, asset::ICPUVirtualTexture* _vt, asset::ISampler::E_TEXTURE_CLAMP _uwrap, asset::ISampler::E_TEXTURE_CLAMP _vwrap, asset::ISampler::E_TEXTURE_BORDER_COLOR _borderColor)
{
    const auto& extent = _img->getCreationParameters().extent;

    auto imgAndOrigSz = asset::ICPUVirtualTexture::createPoTPaddedSquareImageWithMipLevels(_img, _uwrap, _vwrap, _borderColor);

    asset::IImage::SSubresourceRange subres;
    subres.baseMipLevel = 0u;
    subres.levelCount = core::findLSB(core::roundDownToPoT<uint32_t>(std::max(extent.width, extent.height))) + 1;
    subres.baseArrayLayer = 0u;
    subres.layerCount = 1u;

    auto addr = _vt->alloc(_img->getCreationParameters().format, imgAndOrigSz.second, subres, _uwrap, _vwrap);
    commit_t cm{ addr, std::move(imgAndOrigSz.first), subres, _uwrap, _vwrap, _borderColor };

    _out_commits.push_back(cm);

    return addr;
}

void createVirtualTexture(video::IVideoDriver* driver, core::vector<ICPUMeshBuffer*>& meshBuffers, const asset::COBJMetadata const* meta,
    core::smart_refctd_ptr<asset::ICPUVirtualTexture>& vt, core::smart_refctd_ptr<video::IGPUVirtualTexture>& outputGPUvt)
{
    core::unordered_map<core::smart_refctd_ptr<asset::ICPUImage>, STextureData> VTtexDataMap;

    // all pipelines will have the same metadata
    const asset::CMTLMetadata::CRenderpassIndependentPipeline* pipelineMetadata = nullptr;
    core::vector<commit_t> vt_commits;
    //modifying push constants and default fragment shader for VT
    for (auto it = meshBuffers.begin(); it != meshBuffers.end(); it++)
    {
        SPushConstants pushConsts;
        memset(pushConsts.map_data, 0xff, TEX_OF_INTEREST_CNT * sizeof(pushConsts.map_data[0]));
        pushConsts.extra = 0u;

        auto* ds = (*it)->getAttachedDescriptorSet();
        if (!ds)
            continue;
        for (uint32_t k = 0u; k < TEX_OF_INTEREST_CNT; ++k)
        {
            uint32_t j = texturesOfInterest[k];

            auto* view = static_cast<asset::ICPUImageView*>(ds->getDescriptors(j).begin()->desc.get());
            auto* smplr = ds->getLayout()->getBindings().begin()[j].samplers[0].get();
            const auto uwrap = static_cast<asset::ISampler::E_TEXTURE_CLAMP>(smplr->getParams().TextureWrapU);
            const auto vwrap = static_cast<asset::ISampler::E_TEXTURE_CLAMP>(smplr->getParams().TextureWrapV);
            const auto borderColor = static_cast<asset::ISampler::E_TEXTURE_BORDER_COLOR>(smplr->getParams().BorderColor);
            auto img = view->getCreationParameters().image;
            auto extent = img->getCreationParameters().extent;
            if (extent.width <= 2u || extent.height <= 2u)//dummy 2x2
                continue;
            STextureData texData = STextureData::invalid();
            auto found = VTtexDataMap.find(img);
            if (found != VTtexDataMap.end())
                texData = found->second;
            else {
                const asset::E_FORMAT fmt = img->getCreationParameters().format;
                texData = getTextureData(vt_commits, img.get(), vt.get(), uwrap, vwrap, borderColor);
                VTtexDataMap.insert({ img,texData });
            }

            static_assert(sizeof(texData) == sizeof(pushConsts.map_data[0]), "wrong reinterpret_cast");
            pushConsts.map_data[k] = reinterpret_cast<uint64_t*>(&texData)[0];
        }

        pipelineMetadata = static_cast<const asset::CMTLMetadata::CRenderpassIndependentPipeline*>(meta->getAssetSpecificMetadata((*it)->getPipeline()));

        //copy texture presence flags
        pushConsts.extra = pipelineMetadata->m_materialParams.extra;
        pushConsts.ambient = pipelineMetadata->m_materialParams.ambient;
        pushConsts.diffuse = pipelineMetadata->m_materialParams.diffuse;
        pushConsts.emissive = pipelineMetadata->m_materialParams.emissive;
        pushConsts.specular = pipelineMetadata->m_materialParams.specular;
        pushConsts.IoR = pipelineMetadata->m_materialParams.IoR;
        pushConsts.opacity = pipelineMetadata->m_materialParams.opacity;
        pushConsts.shininess = pipelineMetadata->m_materialParams.shininess;
        memcpy((*it)->getPushConstantsDataPtr(), &pushConsts, sizeof(pushConsts));

        //we dont want this DS to be converted into GPU DS, so set to nullptr
        //dont worry about deletion of textures (invalidation of pointers), they're grabbed in VTtexDataMap
        (*it)->setAttachedDescriptorSet(nullptr);
    }
    assert(pipelineMetadata);

    vt->shrink();
    for (const auto& cm : vt_commits)
    {
        vt->commit(cm.addr, cm.texture.get(), cm.subresource, cm.uwrap, cm.vwrap, cm.border);
    }

    outputGPUvt = core::make_smart_refctd_ptr<video::IGPUVirtualTexture>(driver, vt.get());
}

void createPipeline(IVideoDriver* driver, ICPUSpecializedShader* vs, ICPUSpecializedShader* fs, DrawData& drawData)
{
    ICPUSpecializedShader* cpuShaders[2] = { vs, fs };
    auto gpuShaders = driver->getGPUObjectsFromAssets(cpuShaders, cpuShaders + 2);

    core::smart_refctd_ptr<IGPUDescriptorSetLayout> ds0Layout;
    core::smart_refctd_ptr<IGPUDescriptorSetLayout> ds1Layout;
    core::smart_refctd_ptr<IGPUDescriptorSetLayout> ds2Layout;
    {
        constexpr uint32_t mpBindingsCnt = 4u; //TODO: change when mp util functions are done

        auto vtSizes = drawData.vt->getDSlayoutBindings(nullptr, nullptr);
        auto bindings = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<IGPUDescriptorSetLayout::SBinding>>(mpBindingsCnt + vtSizes.first);
        auto vtSamplers = core::make_refctd_dynamic_array< core::smart_refctd_dynamic_array<core::smart_refctd_ptr<IGPUSampler>>>(vtSizes.second);

        IGPUDescriptorSetLayout::SBinding* b = bindings->data();
        b[0].binding = 0u; b[1].binding = 1u; b[2].binding = 2u; b[3].binding = 3u;
        b[0].type = b[1].type = b[2].type = EDT_UNIFORM_TEXEL_BUFFER;
        b[3].type = EDT_STORAGE_BUFFER;
        b[0].stageFlags = b[1].stageFlags = b[2].stageFlags = b[3].stageFlags = ISpecializedShader::ESS_VERTEX;
        b[0].count = 2u;
        b[1].count = 1u;
        b[2].count = 1u;
        b[3].count = 1u;
        
        drawData.vt->getDSlayoutBindings(&b[mpBindingsCnt], vtSamplers->data(), PGTAB_BINDING, PHYSICAL_STORAGE_VIEWS_BINDING);

        b[mpBindingsCnt].stageFlags = ISpecializedShader::ESS_FRAGMENT;
        b[mpBindingsCnt + 1].stageFlags = ISpecializedShader::ESS_FRAGMENT;

        ds0Layout = driver->createGPUDescriptorSetLayout(b, b + bindings->size());

        IGPUDescriptorSetLayout::SBinding b1;
        b1.binding = 0u;
        b1.count = 1u;
        b1.samplers = nullptr;
        b1.stageFlags = ISpecializedShader::ESS_VERTEX;
        b1.type = EDT_UNIFORM_BUFFER;

        ds1Layout = driver->createGPUDescriptorSetLayout(&b1, &b1 + 1);

        IGPUDescriptorSetLayout::SBinding b2[2];
        b2[0].binding = 0u;
        b2[0].count = 1u;
        b2[0].samplers = nullptr;
        b2[0].stageFlags = asset::ISpecializedShader::ESS_FRAGMENT;
        b2[0].type = asset::EDT_STORAGE_BUFFER;

        b2[1].binding = 1u;
        b2[1].count = 1u;
        b2[1].samplers = nullptr;
        b2[1].stageFlags = asset::ISpecializedShader::ESS_FRAGMENT;
        b2[1].type = asset::EDT_STORAGE_BUFFER;

        ds2Layout = driver->createGPUDescriptorSetLayout(b2, b2 + 1);
    }

    SPushConstantRange pcRange;
    pcRange.size = sizeof(uint32_t);
    pcRange.offset = 0u;
    pcRange.stageFlags = ISpecializedShader::ESS_ALL;

    auto pipelineLayout = driver->createGPUPipelineLayout(&pcRange, &pcRange + 1, core::smart_refctd_ptr(ds0Layout), 
        core::smart_refctd_ptr(ds1Layout), core::smart_refctd_ptr(ds2Layout));

    drawData.ds[0] = driver->createGPUDescriptorSet(std::move(ds0Layout));
    drawData.ds[1] = driver->createGPUDescriptorSet(std::move(ds1Layout));
    drawData.ds[2] = driver->createGPUDescriptorSet(std::move(ds2Layout));
    {
        //mesh packing stuff

        IGPUDescriptorSet::SWriteDescriptorSet w[5];
        w[0].arrayElement = 0u;
        w[1].arrayElement = 1u;
        w[2].arrayElement = 0u;
        w[3].arrayElement = 0u;
        w[4].arrayElement = 0u;
        w[0].count = w[1].count = w[2].count = w[3].count = w[4].count = 1u;
        w[0].binding = 0u; w[1].binding = 0u; w[2].binding = 1u; w[3].binding = 2u; w[4].binding = 3u;
        w[0].descriptorType = w[1].descriptorType = w[2].descriptorType = w[3].descriptorType = EDT_UNIFORM_TEXEL_BUFFER;
        w[4].descriptorType = EDT_STORAGE_BUFFER;
        w[0].dstSet = w[1].dstSet = w[2].dstSet = w[3].dstSet = w[4].dstSet = drawData.ds[0].get();

        IGPUDescriptorSet::SDescriptorInfo info[5];

        info[0].buffer.offset = 0u;
        info[0].buffer.size = drawData.vtxBindings[0].buffer->getSize();
        info[0].desc = driver->createGPUBufferView(drawData.vtxBindings[0].buffer.get(), EF_R32G32B32_SFLOAT);
        info[1].buffer.offset = 0u;
        info[1].buffer.size = drawData.vtxBindings[0].buffer->getSize();
        info[1].desc = driver->createGPUBufferView(drawData.vtxBindings[0].buffer.get(), EF_R32G32_SFLOAT);
        info[2].buffer.offset = 0u;
        info[2].buffer.size = drawData.vtxBindings[0].buffer->getSize();
        info[2].desc = driver->createGPUBufferView(drawData.vtxBindings[0].buffer.get(), EF_R32G32_SFLOAT);
        info[3].buffer.offset = 0u;
        info[3].buffer.size = drawData.vtxBindings[0].buffer->getSize();
        info[3].desc = driver->createGPUBufferView(drawData.vtxBindings[0].buffer.get(), EF_R32_UINT);

        //sampler buffers
        w[0].info = &info[0];
        w[1].info = &info[1];
        w[2].info = &info[2];
        w[3].info = &info[3];

        //offset tables
        info[4].buffer.offset = 0u;
        info[4].buffer.size = drawData.virtualAttribTable->getSize();
        info[4].desc = core::smart_refctd_ptr(drawData.virtualAttribTable);
        w[4].info = &info[4];

        driver->updateDescriptorSets(5u, w, 0u, nullptr);

        IGPUDescriptorSet::SWriteDescriptorSet w1;
        w1.arrayElement = 0u;
        w1.count = 1u;
        w1.binding = 0u;
        w1.descriptorType = EDT_UNIFORM_BUFFER;
        w1.dstSet = drawData.ds[1].get();

        drawData.ubo = driver->createDeviceLocalGPUBufferOnDedMem(sizeof(SBasicViewParameters));

        IGPUDescriptorSet::SDescriptorInfo info1;
        info1.buffer.offset = 0u;
        info1.buffer.size = drawData.ubo->getSize();
        info1.desc = core::smart_refctd_ptr(drawData.ubo);
        w1.info = &info1;

        driver->updateDescriptorSets(1u, &w1, 0u, nullptr);

        //vt stuff
        auto sizes = drawData.vt->getDescriptorSetWrites(nullptr, nullptr, nullptr);
        auto writesVT = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<video::IGPUDescriptorSet::SWriteDescriptorSet>>(sizes.first);
        auto infoVT = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<video::IGPUDescriptorSet::SDescriptorInfo>>(sizes.second);

        drawData.vt->getDescriptorSetWrites(writesVT->data(), infoVT->data(), drawData.ds[0].get(), PGTAB_BINDING, PHYSICAL_STORAGE_VIEWS_BINDING);

        driver->updateDescriptorSets(writesVT->size(), writesVT->data(), 0u, nullptr);

        IGPUDescriptorSet::SWriteDescriptorSet w2[2];
        w2[0].arrayElement = 0u;
        w2[0].count = 1u;
        w2[0].binding = 0u;
        w2[0].descriptorType = EDT_STORAGE_BUFFER;
        w2[0].dstSet = drawData.ds[2].get();

        w2[1].arrayElement = 0u;
        w2[1].count = 1u;
        w2[1].binding = 1u;
        w2[1].descriptorType = EDT_STORAGE_BUFFER;
        w2[1].dstSet = drawData.ds[2].get();

        core::smart_refctd_ptr<video::IGPUBuffer> buffer = driver->createFilledDeviceLocalGPUBufferOnDedMem(sizeof(video::IGPUVirtualTexture::SPrecomputedData), &drawData.vt->getPrecomputedData());

        IGPUDescriptorSet::SDescriptorInfo info2[2];
        info2[0].buffer.offset = 0u;
        info2[0].buffer.size = sizeof(video::IGPUVirtualTexture::SPrecomputedData);
        info2[0].desc = buffer;

        info2[1].buffer.offset = 0u;
        info2[1].buffer.size = drawData.vtDataSSBO->getSize();
        info2[1].desc = drawData.vtDataSSBO;

        w2[0].info = &info2[0];
        w2[1].info = &info2[1];
    }

    IGPUSpecializedShader* shaders[2] = { gpuShaders->operator[](0).get(), gpuShaders->operator[](1).get() };

    drawData.gpuPipelines.reserve(drawData.ranges.size());

    for (auto range : drawData.ranges)
    {
        auto cpuPipeline = range.first;

        drawData.gpuPipelines.emplace_back(
            driver->createGPURenderpassIndependentPipeline(
                nullptr, core::smart_refctd_ptr(pipelineLayout),
                shaders, shaders + 2u,
                cpuPipeline->getVertexInputParams(),
                cpuPipeline->getBlendParams(), 
                cpuPipeline->getPrimitiveAssemblyParams(), 
                cpuPipeline->getRasterizationParams()
            )
        );
    }
}

int main()
{
    // create device with full flexibility over creation parameters
    // you can add more parameters if desired, check irr::SIrrlichtCreationParameters
    nbl::SIrrlichtCreationParameters params;
    params.Bits = 24; //may have to set to 32bit for some platforms
    params.ZBufferBits = 24; //we'd like 32bit here
    params.DriverType = video::EDT_OPENGL; //! Only Well functioning driver, software renderer left for sake of 2D image drawing
    params.WindowSize = dimension2d<uint32_t>(1280, 720);
    params.Fullscreen = false;
    params.Vsync = true; //! If supported by target platform
    params.Doublebuffer = true;
    params.Stencilbuffer = false; //! This will not even be a choice soon
    auto device = createDeviceEx(params);

    if (!device)
        return 1; // could not create selected driver.

    //! disable mouse cursor, since camera will force it to the middle
    //! and we don't want a jittery cursor in the middle distracting us
    device->getCursorControl()->setVisible(false);

    //! Since our cursor will be enslaved, there will be no way to close the window
    //! So we listen for the "Q" key being pressed and exit the application
    QToQuitEventReceiver receiver;
    device->setEventReceiver(&receiver);

    auto* driver = device->getVideoDriver();
    auto* smgr = device->getSceneManager();
    auto* am = device->getAssetManager();
    auto* fs = am->getFileSystem();

    //
    auto* qnc = am->getMeshManipulator()->getQuantNormalCache();
    //loading cache from file
    qnc->loadCacheFromFile<asset::EF_A2B10G10R10_SNORM_PACK32>(fs, "../../tmp/normalCache101010.sse", true);

    // register the zip
    device->getFileSystem()->addFileArchive("../../media/sponza.zip");

    asset::IAssetLoader::SAssetLoadParams lp;
    auto meshes_bundle = am->getAsset("sponza.obj", lp);
    //assert(!meshes_bundle.isEmpty());
    auto mesh = meshes_bundle.getContents().begin()[0];
    auto mesh_raw = static_cast<asset::ICPUMesh*>(mesh.get());

    //saving cache to file
    qnc->saveCacheToFile<asset::EF_A2B10G10R10_SNORM_PACK32>(fs, "../../tmp/normalCache101010.sse");

    //TODO: change it to vector of smart pointers
    core::vector<ICPUMeshBuffer*> meshBuffers;
    for (uint32_t i = 0u; i < 100; i++)
        meshBuffers.push_back(mesh_raw->getMeshBufferVector()[i].get());

    DrawData drawData;
    drawData.ranges = sortMeshBuffersByPipeline(meshBuffers);

    // all pipelines will have the same metadata
    const asset::CMTLMetadata::CRenderpassIndependentPipeline* pipelineMetadata = nullptr;
    //core::vector<commit_t> vt_commits;
    const auto meta = meshes_bundle.getMetadata()->selfCast<const asset::COBJMetadata>();

    core::smart_refctd_ptr<asset::ICPUVirtualTexture> vt = core::make_smart_refctd_ptr<asset::ICPUVirtualTexture>([](asset::E_FORMAT_CLASS) -> uint32_t { return TILES_PER_DIM_LOG2; }, PAGE_SZ_LOG2, PAGE_PADDING, MAX_ALLOCATABLE_TEX_SZ_LOG2);
    core::smart_refctd_ptr<video::IGPUVirtualTexture>& gpuvt = drawData.vt;
    createVirtualTexture(driver, meshBuffers, meta, vt, gpuvt);

    {
        auto* pipeline = meshBuffers[0]->getPipeline();

        auto* vtxShader = pipeline->getShaderAtIndex(asset::ICPURenderpassIndependentPipeline::ESSI_VERTEX_SHADER_IX);
        core::smart_refctd_ptr<ICPUSpecializedShader> vs = createModifiedVertexShader(vtxShader);
        auto* fragShader = pipeline->getShaderAtIndex(asset::ICPURenderpassIndependentPipeline::ESSI_FRAGMENT_SHADER_IX);
        //ICPUSpecializedShader* fs = IAsset::castDown<ICPUSpecializedShader>(am->getAsset("../shader.frag", lp).getContents().begin()->get());
        core::smart_refctd_ptr<ICPUSpecializedShader> fs = createModifiedFragShader(fragShader, vt.get());
        core::smart_refctd_ptr<IGPUBuffer> virtualAttribTable;

        packMeshBuffers(driver, drawData);

        createPipeline(driver, vs.get(), fs.get(), drawData);
    }

    //! we want to move around the scene and view it from different angles
    scene::ICameraSceneNode* camera = smgr->addCameraSceneNodeFPS(0, 100.0f, 0.5f);

    camera->setPosition(core::vector3df(-4, 0, 0));
    camera->setTarget(core::vector3df(0, 0, 0));
    camera->setNearValue(1.f);
    camera->setFarValue(5000.0f);

    smgr->setActiveCamera(camera);

    const video::IGPUDescriptorSet* ds[]{ drawData.ds[0].get(), drawData.ds[1].get(), drawData.ds[2].get() };

    //all pipelines share the same layout
    

    uint64_t lastFPSTime = 0;
    while (device->run() && receiver.keepOpen())
    {
        driver->beginScene(true, true, video::SColor(255, 0, 0, 255));

        //! This animates (moves) the camera and sets the transforms
        camera->OnAnimate(std::chrono::duration_cast<std::chrono::milliseconds>(device->getTimer()->getTime()).count());
        camera->render();
        
        SBasicViewParameters uboData;
        memcpy(uboData.MVP, camera->getConcatenatedMatrix().pointer(), sizeof(core::matrix4SIMD));
        memcpy(uboData.MV, camera->getViewMatrix().pointer(), sizeof(core::matrix3x4SIMD));
        memcpy(uboData.NormalMat, camera->getViewMatrix().pointer(), sizeof(core::matrix3x4SIMD));

        driver->updateBufferRangeViaStagingBuffer(drawData.ubo.get(), 0u, sizeof(SBasicViewParameters), &uboData);

        uint32_t i = 0u;
        for (auto pipeline : drawData.gpuPipelines)
        {
            driver->bindGraphicsPipeline(pipeline.get());
            driver->bindDescriptorSets(video::EPBP_GRAPHICS, pipeline->getLayout(), 0u, 3u, ds, nullptr);

            driver->pushConstants(pipeline->getLayout(), IGPUSpecializedShader::ESS_ALL, 0u, sizeof(uint32_t), &drawData.pushConstantsData[i]);

            driver->drawIndexedIndirect(drawData.vtxBindings, DrawIndexedIndirectInput::mode, DrawIndexedIndirectInput::indexType, 
                drawData.idxBuffer.get(), drawData.mdiBuffer.get(), drawData.drawIndirectInput[i].offset, 
                drawData.drawIndirectInput[i].maxCount, DrawIndexedIndirectInput::stride);

            i++;
        }

        driver->endScene();

        // display frames per second in window title
        uint64_t time = device->getTimer()->getRealTime();
        if (time - lastFPSTime > 1000)
        {
            std::wostringstream str;
            str << L"Meshloaders Demo - IrrlichtBAW Engine [" << driver->getName() << "] FPS:" << driver->getFPS() << " PrimitvesDrawn:" << driver->getPrimitiveCountDrawn();

            device->setWindowCaption(str.str().c_str());
            lastFPSTime = time;
        }
    }
}