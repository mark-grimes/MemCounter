#include "memcounter/MemoryCounterImplementation.h"

#include <algorithm>

#include "memcounter/ThreadMemoryCounterPool.h"


memcounter::MemoryCounterImplementation::MemoryCounterImplementation( memcounter::ThreadMemoryCounterPool& parentPool )
	: enabled_(false), currentSize_(0), maximumSize_(0), currentNumberOfAllocations_(0),
	  maximumNumberOfAllocations_(0), verbose_(false), pParentPool_(&parentPool), pParentCounter_(NULL)
{
}

memcounter::MemoryCounterImplementation::MemoryCounterImplementation( memcounter::MemoryCounterImplementation* pParentCounter )
	: enabled_(false), currentSize_(0), maximumSize_(0), currentNumberOfAllocations_(0),
	  maximumNumberOfAllocations_(0), verbose_(false), pParentPool_(NULL), pParentCounter_(pParentCounter)
{
}

memcounter::MemoryCounterImplementation::~MemoryCounterImplementation()
{
}

bool memcounter::MemoryCounterImplementation::setEnabled( bool enable )
{
	bool oldEnabled=enabled_;
	enabled_=enable;
	if( enable )
	{
		if( pParentPool_) pParentPool_->informEnabled( this );
		else pParentCounter_->childEnabled( this );
	}
	else
	{
		if( pParentPool_) pParentPool_->informDisabled( this );
		else pParentCounter_->childDisabled( this );
	}
	return oldEnabled;
}

bool memcounter::MemoryCounterImplementation::isEnabled() const
{
	return enabled_;
}

void memcounter::MemoryCounterImplementation::enable()
{
	if( pParentPool_) pParentPool_->informEnabled( this );
	else pParentCounter_->childEnabled( this );
	enabled_=true;
}

void memcounter::MemoryCounterImplementation::disable()
{
	if( pParentPool_) pParentPool_->informDisabled( this );
	else pParentCounter_->childDisabled( this );
	enabled_=false;
}

void memcounter::MemoryCounterImplementation::reset()
{
	currentSize_=0;
	maximumSize_=0;
	currentNumberOfAllocations_=0;
	maximumNumberOfAllocations_=0;
	for( std::vector<IMemoryCounter*>::iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) (*iSubCounter)->reset();
}

void memcounter::MemoryCounterImplementation::resetMaximum()
{
	maximumSize_=currentSize_;
	maximumNumberOfAllocations_=currentNumberOfAllocations_;
	for( std::vector<IMemoryCounter*>::iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) (*iSubCounter)->resetMaximum();
}

void memcounter::MemoryCounterImplementation::dumpContents( std::ostream& stream, const std::string& prefix ) const
{
	stream << prefix << "Running total of current size=" << currentSize_ << ", maximum size=" << maximumSize_ << std::endl;
}

long int memcounter::MemoryCounterImplementation::currentSize() const
{
	long int currentSize=currentSize_;
	for( std::vector<IMemoryCounter*>::const_iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) currentSize+=(*iSubCounter)->currentSize();
	return currentSize;
}

long int memcounter::MemoryCounterImplementation::maximumSize() const
{
	long int maximumSize=maximumSize_;
	for( std::vector<IMemoryCounter*>::const_iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) maximumSize+=(*iSubCounter)->maximumSize();
	return maximumSize;
}

int memcounter::MemoryCounterImplementation::currentNumberOfAllocations() const
{
	int currentNumberOfAllocations=currentNumberOfAllocations_;
	for( std::vector<IMemoryCounter*>::const_iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) currentNumberOfAllocations+=(*iSubCounter)->currentNumberOfAllocations();
	return currentNumberOfAllocations;
}

int memcounter::MemoryCounterImplementation::maximumNumberOfAllocations() const
{
	int maximumNumberOfAllocations=maximumNumberOfAllocations_;
	for( std::vector<IMemoryCounter*>::const_iterator iSubCounter=subCounters_.begin(); iSubCounter!=subCounters_.end(); ++iSubCounter ) maximumNumberOfAllocations+=(*iSubCounter)->maximumNumberOfAllocations();
	return maximumNumberOfAllocations;
}

const std::vector<memcounter::IMemoryCounter*>& memcounter::MemoryCounterImplementation::subCounters() const
{
	return subCounters_;
}

void memcounter::MemoryCounterImplementation::add( size_t size )
{
	if( !enabled_ ) return;

	currentSize_+=size;
	if( currentSize_>maximumSize_ ) maximumSize_=currentSize_;

	++currentNumberOfAllocations_;
	if( currentNumberOfAllocations_>maximumNumberOfAllocations_ ) maximumNumberOfAllocations_=currentNumberOfAllocations_;
}

void memcounter::MemoryCounterImplementation::modify( size_t oldSize, size_t newSize )
{
	if( !enabled_ ) return;

//	if( newSize<oldSize && currentSize_<oldSize-newSize ) std::cerr << " *MEMCOUNTER* - Argh! underflow in modify" << std::endl;

	currentSize_-=oldSize;
	currentSize_+=newSize;
	if( currentSize_>maximumSize_ ) maximumSize_=currentSize_;
}

void memcounter::MemoryCounterImplementation::remove( size_t size )
{
	if( !enabled_ ) return;

//	if( currentSize_<size ) std::cerr << " *MEMCOUNTER* - Argh! underflow in remove" << std::endl;

	currentSize_-=size;
	--currentNumberOfAllocations_;
}

void memcounter::MemoryCounterImplementation::childEnabled( memcounter::MemoryCounterImplementation* pEnabledSubCounter )
{
	// Disable the currently active sub-counter if there is one active
	if( pCurrentlyActiveSubCounter_ ) pCurrentlyActiveSubCounter_->rawSetEnabled(false);
	// and set the currently active pointer
	pCurrentlyActiveSubCounter_=pEnabledSubCounter;
}

void memcounter::MemoryCounterImplementation::childDisabled( memcounter::MemoryCounterImplementation* pDisabledSubCounter )
{
	if( pCurrentlyActiveSubCounter_==pDisabledSubCounter ) pCurrentlyActiveSubCounter_=NULL;
}

bool memcounter::MemoryCounterImplementation::rawSetEnabled( bool enable )
{
	enabled_=enable;
}
