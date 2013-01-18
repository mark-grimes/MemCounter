//#include "hooksAndEntryPoints.h"

#include "memcounter/MemoryCounterManager.h"

#include "macros.h"

bool memcounter_globallyDisabled=true;

using memcounter::IMemoryCounter;

extern "C"
{
	VISIBLE IMemoryCounter* createNewMemoryCounter( void )
	{
		return memcounter::MemoryCounterManager::instance().createNewMemoryCounter();
	}
}
