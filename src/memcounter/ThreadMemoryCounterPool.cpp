#include "memcounter/ThreadMemoryCounterPool.h"

#include "memcounter/TrackingMemoryCounter.h"
#include "memcounter/MallinfoMemoryCounter.h"
#include "hooksAndEntryPoints.h"

#include <iostream>

memcounter::ThreadMemoryCounterPool::ThreadMemoryCounterPool() : pLastEnabledCounter_(NULL)
{
	if(true) std::cout << "Constructing ThreadMemoryCounterPool" << std::endl;
}

memcounter::ThreadMemoryCounterPool::~ThreadMemoryCounterPool()
{
	if(true) std::cout << "Destructing ThreadMemoryCounterPool" << std::endl;

	for( std::vector<memcounter::ICountingInterface*>::iterator iCounter=createdCounters_.begin(); iCounter!=createdCounters_.end(); ++iCounter )
	{
		delete *iCounter;
	}
}

memcounter::IMemoryCounter* memcounter::ThreadMemoryCounterPool::createNewMemoryCounter()
{
	memcounter::ICountingInterface* pNewMemoryCounter=new MallinfoMemoryCounter(*this);
	// Keep track of all the pointers so that I can delete them later. I'll also use this vector
	// to run through all of the counters when e.g. addToAllEnabledCounters is called.
	createdCounters_.push_back(pNewMemoryCounter);

	return pNewMemoryCounter;
}

void memcounter::ThreadMemoryCounterPool::preAddToAllEnabledCounters( void* pointer, size_t size )
{
	//
	// I'm having performance issues at the moment, so I'll just use the most recently enabled counter
	//

//	for( std::vector<memcounter::MemoryCounterImplementation*>::iterator iCounter=createdCounters_.begin(); iCounter!=createdCounters_.end(); ++iCounter )
//	{
//		if( (*iCounter)->isEnabled() ) (*iCounter)->add( pointer, size );
//	}
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->preAdd( pointer, size );
}

void memcounter::ThreadMemoryCounterPool::postAddToAllEnabledCounters( void* pointer, size_t size )
{
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->postAdd( pointer, size );
}

void memcounter::ThreadMemoryCounterPool::preModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize )
{
//	for( std::vector<memcounter::MemoryCounterImplementation*>::iterator iCounter=createdCounters_.begin(); iCounter!=createdCounters_.end(); ++iCounter )
//	{
//		if( (*iCounter)->isEnabled() ) (*iCounter)->modify( oldPointer, newPointer, newSize );
//	}
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->preModify( oldPointer, newPointer, newSize );
}

void memcounter::ThreadMemoryCounterPool::postModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize )
{
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->postModify( oldPointer, newPointer, newSize );
}

void memcounter::ThreadMemoryCounterPool::preRemoveFromAllEnabledCounters( void* pointer )
{
//	for( std::vector<memcounter::MemoryCounterImplementation*>::iterator iCounter=createdCounters_.begin(); iCounter!=createdCounters_.end(); ++iCounter )
//	{
//		if( (*iCounter)->isEnabled() ) (*iCounter)->remove( pointer );
//	}
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->preRemove(pointer);
}

void memcounter::ThreadMemoryCounterPool::postRemoveFromAllEnabledCounters( void* pointer )
{
	if( pLastEnabledCounter_ ) pLastEnabledCounter_->postRemove(pointer);
}

void memcounter::ThreadMemoryCounterPool::informEnabled( memcounter::ICountingInterface* pEnabledCounter )
{
	pLastEnabledCounter_=pEnabledCounter;
	enableThisThread();
}

void memcounter::ThreadMemoryCounterPool::informDisabled( memcounter::ICountingInterface* pDisabledCounter )
{
	if( pLastEnabledCounter_==pDisabledCounter )
	{
		pLastEnabledCounter_=NULL;
		disableThisThread();
	}
}
