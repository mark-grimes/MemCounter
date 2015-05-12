#include "memcounter/ThreadMemoryCounterPool.h"

#include "memcounter/MemoryCounterImplementation.h"
#include "memcounter/DisablingFunctions.h"

#include <iostream>
#include <algorithm>

memcounter::ThreadMemoryCounterPool::ThreadMemoryCounterPool() //: pLastEnabledCounter_(NULL)
{
	if(true) std::cout << "Constructing ThreadMemoryCounterPool on thread pthread_self=" << pthread_self() << std::endl;
}

memcounter::ThreadMemoryCounterPool::~ThreadMemoryCounterPool()
{
	if(true) std::cout << "Destructing ThreadMemoryCounterPool on thread pthread_self=" << pthread_self() << std::endl;

	// Getting crashes when I try and use iCounter, so I think the destruction order is messing up.
	// I think this happens at the end but it's stopping me from getting the logs for my batch jobs.
	// Until I can figure out a fix I'll just leak the memory.
//	for( std::vector<memcounter::ICountingInterface*>::iterator iCounter=createdCounters_.begin(); iCounter!=createdCounters_.end(); ++iCounter )
//	{
//		delete *iCounter;
//	}
}

memcounter::IMemoryCounter* memcounter::ThreadMemoryCounterPool::createNewMemoryCounter()
{
	memcounter::ICountingInterface* pNewMemoryRecorder=new MemoryCounterImplementation(*this);
	// Keep track of all the pointers so that I can delete them later. I'll also use this vector
	// to run through all of the counters when e.g. addToAllEnabledCounters is called.
	createdCounters_.push_back(pNewMemoryRecorder);

	return pNewMemoryRecorder;

}

void memcounter::ThreadMemoryCounterPool::addToAllEnabledCounters( size_t size )
{
	for( std::list<memcounter::ICountingInterface*>::iterator iRecorder=enabledCounters_.begin(); iRecorder!=enabledCounters_.end(); ++iRecorder )
	{
		(*iRecorder)->add( size );
	}
}

void memcounter::ThreadMemoryCounterPool::modifyAllEnabledCounters( size_t oldSize, size_t newSize )
{
	for( std::list<memcounter::ICountingInterface*>::iterator iRecorder=enabledCounters_.begin(); iRecorder!=enabledCounters_.end(); ++iRecorder )
	{
		(*iRecorder)->modify( oldSize, newSize );
	}
}

void memcounter::ThreadMemoryCounterPool::removeFromAllEnabledCounters( size_t size )
{
	for( std::list<memcounter::ICountingInterface*>::iterator iRecorder=enabledCounters_.begin(); iRecorder!=enabledCounters_.end(); ++iRecorder )
	{
		(*iRecorder)->remove( size );
	}
}

void memcounter::ThreadMemoryCounterPool::informEnabled( memcounter::ICountingInterface* pEnabledRecorder )
{
	// Make sure the recorder is not already in the list of enabled recorders
	std::list<memcounter::ICountingInterface*>::iterator iFindResult=std::find( enabledCounters_.begin(), enabledCounters_.end(), pEnabledRecorder );

	// If it wasn't found, add it
	if( iFindResult==enabledCounters_.end() ) enabledCounters_.push_back( pEnabledRecorder );
	memcounter::enableThisThread();
}

void memcounter::ThreadMemoryCounterPool::informDisabled( memcounter::ICountingInterface* pDisabledRecorder )
{
	// Try and find this recorder in the list
	std::list<memcounter::ICountingInterface*>::iterator iFindResult=std::find( enabledCounters_.begin(), enabledCounters_.end(), pDisabledRecorder );

	// Erase it if it was found
	if( iFindResult!=enabledCounters_.end() ) enabledCounters_.erase( iFindResult );

	if( enabledCounters_.empty() ) memcounter::disableThisThread();
}

std::vector<memcounter::ICountingInterface*> memcounter::ThreadMemoryCounterPool::enabledCounters()
{
	return std::vector<memcounter::ICountingInterface*>( enabledCounters_.begin(), enabledCounters_.end() );
}
