// Copyright (C) 2018-2023 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h
#ifndef _NBL_ASSET_I_CPU_RENDERPASS_INDEPENDENT_PIPELINE_H_INCLUDED_
#define _NBL_ASSET_I_CPU_RENDERPASS_INDEPENDENT_PIPELINE_H_INCLUDED_

#include "nbl/asset/IRenderpassIndependentPipeline.h"
#include "nbl/asset/ICPUSpecializedShader.h"
#include "nbl/asset/ICPUPipelineLayout.h"

namespace nbl
{
namespace asset
{

//! CPU Version of Renderpass Independent Pipeline
/*
	@see IRenderpassIndependentPipeline
*/

class ICPURenderpassIndependentPipeline : public IRenderpassIndependentPipeline<ICPUSpecializedShader>, public IAsset
{
		using base_t = IRenderpassIndependentPipeline<ICPUSpecializedShader>;

	public:
		//(TODO) it is true however it causes DSs to not be cached when ECF_DONT_CACHE_TOP_LEVEL is set which isnt really intuitive
        _NBL_STATIC_INLINE_CONSTEXPR uint32_t DESC_SET_HIERARCHYLEVELS_BELOW = 0u;
		// TODO: @Crisspl HOW ON EARTH DOES THIS MAKE SENSE!?
        _NBL_STATIC_INLINE_CONSTEXPR uint32_t IMAGEVIEW_HIERARCHYLEVELS_BELOW = 1u;
        _NBL_STATIC_INLINE_CONSTEXPR uint32_t IMAGE_HIERARCHYLEVELS_BELOW = 2u;
		// from here its good
		_NBL_STATIC_INLINE_CONSTEXPR uint32_t PIPELINE_LAYOUT_HIERARCHYLEVELS_BELOW = 1u;
		_NBL_STATIC_INLINE_CONSTEXPR uint32_t DESC_SET_LAYOUT_HIERARCHYLEVELS_BELOW = 1u+ICPUPipelineLayout::DESC_SET_LAYOUT_HIERARCHYLEVELS_BELOW;
		_NBL_STATIC_INLINE_CONSTEXPR uint32_t IMMUTABLE_SAMPLER_HIERARCHYLEVELS_BELOW = 1u+ICPUPipelineLayout::IMMUTABLE_SAMPLER_HIERARCHYLEVELS_BELOW;
		_NBL_STATIC_INLINE_CONSTEXPR uint32_t SPECIALIZED_SHADER_HIERARCHYLEVELS_BELOW = 1u;


		ICPURenderpassIndependentPipeline(core::smart_refctd_ptr<ICPUPipelineLayout>&& _layout, const SCreationParams& params) : base_t(params), m_layout(std::move(_layout)) {}

		size_t conservativeSizeEstimate() const override { return sizeof(base_t); }
		void convertToDummyObject(uint32_t referenceLevelsBelowToConvert=0u) override
		{
			if (referenceLevelsBelowToConvert)
			{
                //intentionally parent is not converted
                --referenceLevelsBelowToConvert;
				m_layout->convertToDummyObject(referenceLevelsBelowToConvert);
				for (auto i=0u; i<GRAPHICS_SHADER_STAGE_COUNT; i++)
                    if (m_shaders[i])
					    m_shaders[i]->convertToDummyObject(referenceLevelsBelowToConvert);
			}
		}

        core::smart_refctd_ptr<IAsset> clone(uint32_t _depth = ~0u) const override
        {
            core::smart_refctd_ptr<ICPUPipelineLayout> layout = (_depth > 0u && m_layout) ? core::smart_refctd_ptr_static_cast<ICPUPipelineLayout>(m_layout->clone(_depth-1u)) : m_layout;

            std::array<core::smart_refctd_ptr<ICPUSpecializedShader>, GRAPHICS_SHADER_STAGE_COUNT> shaders;
            for (uint32_t i = 0u; i < shaders.size(); ++i)
                shaders[i] = (_depth > 0u && m_shaders[i]) ? core::smart_refctd_ptr_static_cast<ICPUSpecializedShader>(m_shaders[i]->clone(_depth-1u)) : m_shaders[i];
            std::array<ICPUSpecializedShader*, GRAPHICS_SHADER_STAGE_COUNT> shaders_raw;
            for (uint32_t i = 0u; i < shaders.size(); ++i)
                shaders_raw[i] = shaders[i].get();
            std::sort(shaders_raw.begin(), shaders_raw.end(), [](ICPUSpecializedShader* a, ICPUSpecializedShader* b) { return (a && !b); });

            auto cp = core::make_smart_refctd_ptr<ICPURenderpassIndependentPipeline>(std::move(layout),
                shaders_raw.data(), &*std::find(shaders_raw.begin(), shaders_raw.end(), nullptr),
                m_vertexInputParams, m_blendParams, m_primAsmParams, m_rasterParams
            );
            clone_common(cp.get());

            return cp;
        }

		_NBL_STATIC_INLINE_CONSTEXPR auto AssetType = ET_RENDERPASS_INDEPENDENT_PIPELINE;
		inline E_TYPE getAssetType() const override { return AssetType; }

		inline ICPUSpecializedShader* getShaderAtStage(IShader::E_SHADER_STAGE _stage) 
		{ 
			assert(!isImmutable_debug());
			return m_shaders[core::findLSB<uint32_t>(_stage)].get(); 
		}
		inline ICPUSpecializedShader* getShaderAtIndex(uint32_t _ix) 
		{
			assert(!isImmutable_debug());
			return m_shaders[_ix].get();
		}
		inline const ICPUSpecializedShader* getShaderAtIndex(uint32_t _ix) const { return m_shaders[_ix].get(); }

//

		inline ICPUPipelineLayout* getLayout()
		{
			assert(!isImmutable_debug());
			return m_layout.get();
		}
		const inline ICPUPipelineLayout* getLayout() const { return m_layout.get(); }


		inline void setShaderAtStage(IShader::E_SHADER_STAGE _stage, ICPUSpecializedShader* _shdr) 
		{
			assert(!isImmutable_debug());
			m_shaders[core::findLSB<uint32_t>(_stage)] = core::smart_refctd_ptr<ICPUSpecializedShader>(_shdr); 
		}

		inline void setLayout(core::smart_refctd_ptr<ICPUPipelineLayout>&& _layout) 
		{
			assert(!isImmutable_debug());
			m_layout = std::move(_layout);
		}

		bool canBeRestoredFrom(const IAsset* _other) const override
		{
			auto* other = static_cast<const ICPURenderpassIndependentPipeline*>(_other);
			if (memcmp(&m_cachedParams,&other->m_cachedParams,sizeof(m_cachedParams))!=0)
				return false;

			for (uint32_t i = 0u; i < GRAPHICS_SHADER_STAGE_COUNT; ++i)
			{
				if ((!m_shaders[i]) != (!other->m_shaders[i]))
					return false;
				if (m_shaders[i] && !m_shaders[i]->canBeRestoredFrom(other->m_shaders[i].get()))
					return false;
			}
			if (!m_layout->canBeRestoredFrom(other->m_layout.get()))
				return false;

			return true;
		}

	protected:
		virtual ~ICPURenderpassIndependentPipeline() = default;

		void restoreFromDummy_impl(IAsset* _other, uint32_t _levelsBelow) override
		{
			auto* other = static_cast<ICPURenderpassIndependentPipeline*>(_other);

			if (_levelsBelow)
			{
				--_levelsBelow;

				restoreFromDummy_impl_call(m_layout.get(), other->m_layout.get(), _levelsBelow);
				for (uint32_t i = 0u; i < GRAPHICS_SHADER_STAGE_COUNT; ++i)
					if (m_shaders[i])
						restoreFromDummy_impl_call(m_shaders[i].get(), other->m_shaders[i].get(), _levelsBelow);
			}
		}

		bool isAnyDependencyDummy_impl(uint32_t _levelsBelow) const override
		{
			--_levelsBelow;
			if (m_layout->isAnyDependencyDummy(_levelsBelow))
				return true;
			for (auto& shader : m_shaders)
				if (shader && shader->isAnyDependencyDummy(_levelsBelow))
					return true;
			return false;
		}

		core::smart_refctd_ptr<ICPUPipelineLayout> m_layout;
};

}
}

#endif
