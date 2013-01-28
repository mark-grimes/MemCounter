#include "memcounter/ThreadMemoryCounterPool.h"

#include "memcounter/TrackingMemoryCounter.h"
#include "memcounter/MallinfoMemoryCounter.h"
#include "memcounter/RecordingMemoryCounter.h"
#include "memcounter/DisablingFunctions.h"

#include <iostream>
#include <algorithm>

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
	memcounter::enableThisThread();
}

void memcounter::ThreadMemoryCounterPool::informDisabled( memcounter::ICountingInterface* pDisabledCounter )
{
	if( pLastEnabledCounter_==pDisabledCounter )
	{
		pLastEnabledCounter_=NULL;
		memcounter::disableThisThread();
	}
}

memcounter::IMemoryCounter* memcounter::ThreadMemoryCounterPool::createNewMemoryRecorder()
{
	memcounter::IRecordingInterface* pNewMemoryRecorder=new RecordingMemoryCounter(*this);
	// Keep track of all the pointers so that I can delete them later. I'll also use this vector
	// to run through all of the counters when e.g. addToAllEnabledCounters is called.
	createdRecorders_.push_back(pNewMemoryRecorder);

	return pNewMemoryRecorder;

}

void memcounter::ThreadMemoryCounterPool::addToAllEnabledRecorders( size_t size )
{
	for( std::list<memcounter::IRecordingInterface*>::iterator iRecorder=enabledRecorders_.begin(); iRecorder!=enabledRecorders_.end(); ++iRecorder )
	{
		(*iRecorder)->add( size );
	}
}

void memcounter::ThreadMemoryCounterPool::modifyAllEnabledRecorders( size_t oldSize, size_t newSize )
{
	for( std::list<memcounter::IRecordingInterface*>::iterator iRecorder=enabledRecorders_.begin(); iRecorder!=enabledRecorders_.end(); ++iRecorder )
	{
		(*iRecorder)->modify( oldSize, newSize );
	}
}

void memcounter::ThreadMemoryCounterPool::removeFromAllEnabledRecorders( size_t size )
{
	for( std::list<memcounter::IRecordingInterface*>::iterator iRecorder=enabledRecorders_.begin(); iRecorder!=enabledRecorders_.end(); ++iRecorder )
	{
		(*iRecorder)->remove( size );
	}
}

void memcounter::ThreadMemoryCounterPool::informEnabled( memcounter::IRecordingInterface* pEnabledRecorder )
{
	// Make sure the recorder is not already in the list of enabled recorders
	std::list<memcounter::IRecordingInterface*>::iterator iFindResult=std::find( enabledRecorders_.begin(), enabledRecorders_.end(), pEnabledRecorder );

	// If it wasn't found, add it
	if( iFindResult==enabledRecorders_.end() ) enabledRecorders_.push_back( pEnabledRecorder );
	memcounter::enableThisThread();
}

void memcounter::ThreadMemoryCounterPool::informDisabled( memcounter::IRecordingInterface* pDisabledRecorder )
{
	// Try and find this recorder in the list
	std::list<memcounter::IRecordingInterface*>::iterator iFindResult=std::find( enabledRecorders_.begin(), enabledRecorders_.end(), pDisabledRecorder );

	// Erase it if it was found
	if( iFindResult!=enabledRecorders_.end() ) enabledRecorders_.erase( iFindResult );

	if( enabledRecorders_.empty() ) memcounter::disableThisThread();
}
