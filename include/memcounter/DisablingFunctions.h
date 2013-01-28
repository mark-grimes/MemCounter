#ifndef memcounter_DisablingFunctions_h
#define memcounter_DisablingFunctions_h

// This extern stops the malloc etc. hooks from doing anything different to the normal
// calls until MemoryCounterManager is fully constructed. It is also used to switch
// off special behaviour when MemoryCounterManager has been destructed.
extern bool memcounter_globallyDisabled;

namespace memcounter
{
	// Note that these functions are defined in MemoryCounterManager.cpp
	void enableThisThread();
	void disableThisThread();
}

#endif
