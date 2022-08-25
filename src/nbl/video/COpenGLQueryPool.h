// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#ifndef _NBL_VIDEO_C_OPEN_GL_QUERY_POOL_H_INCLUDED_
#define _NBL_VIDEO_C_OPEN_GL_QUERY_POOL_H_INCLUDED_

#include "nbl/core/SRange.h"
#include "nbl/core/decl/Types.h"
#include "nbl/video/IQueryPool.h"
#include "nbl/video/COpenGLCommon.h"
#include "nbl/video/IOpenGL_FunctionTable.h"
#include "nbl/video/IOpenGL_PhysicalDeviceBase.h"
#include "nbl/video/COpenGLCommandPool.h"

namespace nbl::video
{

class COpenGLQueryPool final : public IQueryPool
{
	public:
		using atomic_queue_id = core::atomic<uint32_t>;

	protected:
		virtual ~COpenGLQueryPool();

		uint32_t m_glQueriesPerQuery = 0u;
		core::smart_refctd_dynamic_array<GLuint> m_queries[IOpenGLPhysicalDeviceBase::MaxQueues];

		atomic_queue_id* lastQueueToUseArray;

	public:
		COpenGLQueryPool(core::smart_refctd_ptr<const ILogicalDevice>&& dev, core::smart_refctd_dynamic_array<GLuint> queries[IOpenGLPhysicalDeviceBase::MaxQueues], uint32_t glQueriesPerQuery, IQueryPool::SCreationParams&& _params) 
			: IQueryPool(std::move(dev), std::move(_params)), m_glQueriesPerQuery(glQueriesPerQuery)
		{
			for(uint32_t q = 0; q < IOpenGLPhysicalDeviceBase::MaxQueues; ++q)
				m_queries[q] = queries[q];
			
			// Can we have a #define for allocation with default alignment that takes type and count? this seems long and mostly redundant
			lastQueueToUseArray = reinterpret_cast<atomic_queue_id*>(_NBL_ALIGNED_MALLOC(sizeof(atomic_queue_id) * _params.queryCount, _NBL_DEFAULT_ALIGNMENT(atomic_queue_id)));
		}

		inline core::smart_refctd_dynamic_array<GLuint> getQueriesForQueueIdx(uint32_t queueIdx)
		{
			assert(queueIdx < IOpenGLPhysicalDeviceBase::MaxQueues);
			return m_queries[queueIdx];
		}

		inline core::SRange<const GLuint> getQueries(uint32_t queueIdx) const
		{
			auto ret = core::SRange<const GLuint>(nullptr, nullptr);
			if(queueIdx < IOpenGLPhysicalDeviceBase::MaxQueues)
			{
				auto queryArray = m_queries[queueIdx].get();
				if(queryArray)
					ret = core::SRange<const GLuint>(queryArray->begin(), queryArray->begin() + queryArray->size());
			}
			else
				assert(false);
			return ret;
		}
		
		inline GLuint getQueryAt(uint32_t queueIdx, uint32_t query) const
		{
			auto queries = getQueries(queueIdx);
			if(query < queries.size())
			{
				return queries[query];
			}
			else
			{
				assert(false);
				return GL_NONE;
			}
		}
		
		inline uint32_t getLastQueueToUseForQuery(uint32_t query) const
		{
			assert(query < params.queryCount);
			return lastQueueToUseArray[query].load();
		}

		inline void setLastQueueToUseForQuery(uint32_t query, uint32_t value)
		{
			assert(query < params.queryCount);
			lastQueueToUseArray[query] = value;
		}

		inline GLuint getLatestQueryAt(uint32_t query) const
		{
			const uint32_t queueToUse = getLastQueueToUseForQuery(query);
			auto queries = getQueries(queueToUse);
			if(query < queries.size())
			{
				return queries[query];
			}
			else
			{
				assert(false);
				return 0u;
			}
		}

		inline uint32_t getGLQueriesPerQuery() const { return m_glQueriesPerQuery; }

		// TODO(achal): Temporary overload.
		inline void beginQuery(IOpenGL_FunctionTable* gl, uint32_t ctxid, uint32_t queryIndex, E_QUERY_CONTROL_FLAGS flags) const
		{
			if(gl != nullptr)
			{
				if(params.queryType == EQT_OCCLUSION)
				{
					GLuint query = getQueryAt(ctxid, queryIndex);
					gl->glQuery.pglBeginQuery(GL_SAMPLES_PASSED, query);
				}
				else if(params.queryType == EQT_TIMESTAMP)
				{
					assert(false && "TIMESTAMP QueryPool doesn't work with begin/end functions.");
				}
				else
				{
					assert(false && "QueryType is not supported.");
				}
			}
		}

		inline bool beginQuery(uint32_t queryIndex, E_QUERY_CONTROL_FLAGS flags, IGPUCommandPool* cmdpool, IGPUCommandPool::CCommandSegment::Iterator& segmentListHeadItr, IGPUCommandPool::CCommandSegment*& segmentListTail) const
		{
			if (params.queryType == EQT_OCCLUSION)
			{
				if (!cmdpool->emplace<COpenGLCommandPool::CBeginQueryCmd>(segmentListHeadItr, segmentListTail, queryIndex, GL_SAMPLES_PASSED, core::smart_refctd_ptr<const COpenGLQueryPool>(this)))
					return false;
			}
			else if (params.queryType == EQT_TIMESTAMP)
			{
				assert(!"TIMESTAMP QueryPool doesn't work with begin/end functions.");
				return false;
			}
			else
			{
				assert(!"QueryType is not supported.");
				return false;
			}

			return true;
		}
		
		// TODO(achal): Temporary overload.
		inline void endQuery(IOpenGL_FunctionTable* gl, uint32_t ctxid, uint32_t queryIndex) const
		{
			// End Function doesn't use queryIndex
			if(gl != nullptr)
			{
				if(params.queryType == EQT_OCCLUSION)
				{
					gl->glQuery.pglEndQuery(GL_SAMPLES_PASSED);
					lastQueueToUseArray[queryIndex] = ctxid;
				}
				else if(params.queryType == EQT_TIMESTAMP)
				{
					assert(false && "TIMESTAMP QueryPool doesn't work with begin/end functions.");
				}
				else
				{
					assert(false && "QueryType is not supported.");
				}
			}
		}

		inline bool endQuery(uint32_t queryIndex, IGPUCommandPool* cmdpool, IGPUCommandPool::CCommandSegment::Iterator& segmentListHeadItr, IGPUCommandPool::CCommandSegment*& segmentListTail)
		{
			if (params.queryType == EQT_OCCLUSION)
			{
				// End Function doesn't use queryIndex
				if (!cmdpool->emplace<COpenGLCommandPool::CEndQueryCmd>(segmentListHeadItr, segmentListTail, GL_SAMPLES_PASSED))
					return false;

				if (!cmdpool->emplace<COpenGLCommandPool::CResetQueryCmd>(segmentListHeadItr, segmentListTail, core::smart_refctd_ptr<COpenGLQueryPool>(this), queryIndex))
					return false;
			}
			else if (params.queryType == EQT_TIMESTAMP)
			{
				assert(!"TIMESTAMP QueryPool doesn't work with begin/end functions.");
				return false;
			}
			else
			{
				assert(!"QueryType is not supported.");
				return false;
			}

			return true;
		}

		// TODO(achal): Temporary overload.
		inline void writeTimestamp(IOpenGL_FunctionTable* gl, uint32_t ctxid, uint32_t queryIndex) const
		{
			if(gl != nullptr)
			{
				if(params.queryType == EQT_TIMESTAMP)
				{
					GLuint query = getQueryAt(ctxid, queryIndex);
					gl->glQuery.pglQueryCounter(query, GL_TIMESTAMP);
					lastQueueToUseArray[queryIndex] = ctxid;
				}
				else
				{
					assert(false && "Cannot use writeTimestamp for non-timestamp query pools.");
				}
			}
		}

		inline bool writeTimestamp(uint32_t queryIndex, IGPUCommandPool* cmdpool, IGPUCommandPool::CCommandSegment::Iterator& segmentListHeadItr, IGPUCommandPool::CCommandSegment*& segmentListTail)
		{
			if (params.queryType == EQT_TIMESTAMP)
			{
				if (!cmdpool->emplace<COpenGLCommandPool::CQueryCounterCmd>(segmentListHeadItr, segmentListTail, queryIndex, GL_TIMESTAMP, core::smart_refctd_ptr<COpenGLQueryPool>(this)))
					return false;

				if (!cmdpool->emplace<COpenGLCommandPool::CResetQueryCmd>(segmentListHeadItr, segmentListTail, core::smart_refctd_ptr<COpenGLQueryPool>(this), queryIndex))
					return false;
			}
			else
			{
				assert(!"Cannot use writeTimestamp for non-timestamp query pools.");
				return false;
			}

			return true;
		}

		// TODO(achal): Temporary overload.
		inline bool resetQueries(IOpenGL_FunctionTable* gl, uint32_t ctxid, uint32_t query, uint32_t queryCount)
		{
			// NOTE: There is no Reset Queries on OpenGL
			// NOOP, ONLY set last queue used.
			// TODO: validations about reseting queries and unavailable/available state
			if(query + queryCount <= params.queryCount)
			{
				for(uint32_t i = 0; i < queryCount; ++i)
					lastQueueToUseArray[query + i] = ctxid;
				return true;
			}
			else
			{
				assert(false);
				return false;
			}
		}

		inline bool resetQueries(uint32_t query, uint32_t queryCount, IGPUCommandPool* cmdpool, IGPUCommandPool::CCommandSegment::Iterator& segmentListHeadItr, IGPUCommandPool::CCommandSegment*& segmentListTail)
		{
			// NOTE: There is no Reset Queries on OpenGL
			// NOOP, ONLY set last queue used.
			// TODO: validations about reseting queries and unavailable/available state
			if (query + queryCount <= params.queryCount)
			{
				for (uint32_t i = 0; i < queryCount; ++i)
				{
					if (!cmdpool->emplace<COpenGLCommandPool::CResetQueryCmd>(segmentListHeadItr, segmentListTail, core::smart_refctd_ptr<COpenGLQueryPool>(this), query+i))
						return false;
				}
				return true;
			}
			else
			{
				assert(false);
				return false;
			}
		}

};

} // end namespace nbl::video


#endif

