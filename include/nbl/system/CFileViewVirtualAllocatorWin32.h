#ifndef _NBL_SYSTEM_C_FILE_VIEW_VIRTUAL_ALLOCATOR_WIN32_H_INCLUDED_
#define _NBL_SYSTEM_C_FILE_VIEW_VIRTUAL_ALLOCATOR_WIN32_H_INCLUDED_


#include "nbl/system/IFileViewAllocator.h"


namespace nbl::system 
{
#ifdef _NBL_PLATFORM_WINDOWS_
class CFileViewVirtualAllocatorWin32 : public IFileViewAllocator
{
	public:
		void* alloc(size_t size) override;
		bool dealloc(void* data, size_t size) override;
};
#endif
}

#endif