#include "memcounter/RecordingMemoryCounter.h"

#include <algorithm>

#include "memcounter/ThreadMemoryCounterPool.h"


memcounter::RecordingMemoryCounter::RecordingMemoryCounter( memcounter::ThreadMemoryCounterPool& parent )
	: enabled_(false), currentSize_(0), maximumSize_(0), currentNumberOfAllocations_(0),
	  maximumNumberOfAllocations_(0), verbose_(false), parent_(parent)
{
}

memcounter::RecordingMemoryCounter::~RecordingMemoryCounter()
{
}

bool memcounter::RecordingMemoryCounter::setEnabled( bool enable )
{
	bool oldEnabled=enabled_;
	enabled_=enable;
	if( enable ) parent_.informEnabled( this );
	else parent_.informDisabled( this );
	return oldEnabled;
}

bool memcounter::RecordingMemoryCounter::isEnabled()
{
	return enabled_;
}

void memcounter::RecordingMemoryCounter::enable()
{
	parent_.informEnabled( this );
	enabled_=true;
}

void memcounter::RecordingMemoryCounter::disable()
{
	parent_.informDisabled( this );
	enabled_=false;
}

void memcounter::RecordingMemoryCounter::reset()
{
	currentSize_=0;
	maximumSize_=0;
	currentNumberOfAllocations_=0;
	maximumNumberOfAllocations_=0;
}

void memcounter::RecordingMemoryCounter::resetMaximum()
{
	maximumSize_=currentSize_;
	maximumNumberOfAllocations_=currentNumberOfAllocations_;
}

void memcounter::RecordingMemoryCounter::dumpContents( std::ostream& stream, const std::string& prefix )
{
	stream << prefix << "Running total of current size=" << currentSize_ << ", maximum size=" << maximumSize_ << std::endl;
}

size_t memcounter::RecordingMemoryCounter::currentSize()
{
	return currentSize_;
}

size_t memcounter::RecordingMemoryCounter::maximumSize()
{
	return maximumSize_;
}

int memcounter::RecordingMemoryCounter::currentNumberOfAllocations()
{
	return currentNumberOfAllocations_;
}

int memcounter::RecordingMemoryCounter::maximumNumberOfAllocations()
{
	return maximumNumberOfAllocations_;
}

void memcounter::RecordingMemoryCounter::add( size_t size )
{
	if( !enabled_ ) return;

	currentSize_+=size;
	if( currentSize_>maximumSize_ ) maximumSize_=currentSize_;

	++currentNumberOfAllocations_;
	if( currentNumberOfAllocations_>maximumNumberOfAllocations_ ) maximumNumberOfAllocations_=currentNumberOfAllocations_;
}

void memcounter::RecordingMemoryCounter::modify( size_t oldSize, size_t newSize )
{
	if( !enabled_ ) return;

	if( newSize<oldSize && currentSize_<oldSize-newSize ) std::cerr << " *MEMCOUNTER* - Argh! underflow in modify" << std::endl;

	currentSize_-=oldSize;
	currentSize_+=newSize;
	if( currentSize_>maximumSize_ ) maximumSize_=currentSize_;
}

void memcounter::RecordingMemoryCounter::remove( size_t size )
{
	if( !enabled_ ) return;

	if( currentSize_<size ) std::cerr << " *MEMCOUNTER* - Argh! underflow in remove" << std::endl;

	currentSize_-=size;
	--currentNumberOfAllocations_;
}
