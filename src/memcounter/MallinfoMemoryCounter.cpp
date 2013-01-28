#include "memcounter/MallinfoMemoryCounter.h"

#include <algorithm>
#include <malloc.h>

#include "memcounter/ThreadMemoryCounterPool.h"


memcounter::MallinfoMemoryCounter::MallinfoMemoryCounter( memcounter::ThreadMemoryCounterPool& parent )
	: enabled_(false), currentSize_(0), maximumSize_(0), currentNumberOfAllocations_(0),
	  maximumNumberOfAllocations_(0), parent_(parent)
{
}

memcounter::MallinfoMemoryCounter::~MallinfoMemoryCounter()
{
}

bool memcounter::MallinfoMemoryCounter::setEnabled( bool enable )
{
	bool oldEnabled=enabled_;
	enabled_=enable;
	if( enable ) parent_.informEnabled( this );
	else parent_.informDisabled( this );
	return oldEnabled;
}

bool memcounter::MallinfoMemoryCounter::isEnabled()
{
	return enabled_;
}

void memcounter::MallinfoMemoryCounter::enable()
{
	parent_.informEnabled( this );
	enabled_=true;
}

void memcounter::MallinfoMemoryCounter::disable()
{
	parent_.informDisabled( this );
	enabled_=false;
}

void memcounter::MallinfoMemoryCounter::reset()
{
	currentSize_=0;
	maximumSize_=0;
	currentNumberOfAllocations_=0;
	maximumNumberOfAllocations_=0;
}

void memcounter::MallinfoMemoryCounter::resetMaximum()
{
	maximumSize_=currentSize_;
	maximumNumberOfAllocations_=currentNumberOfAllocations_;
}

void memcounter::MallinfoMemoryCounter::dumpContents( std::ostream& stream, const std::string& prefix )
{
	stream << prefix << "Running total of current size=" << currentSize_ << ", maximum size=" << maximumSize_ << std::endl;
}

size_t memcounter::MallinfoMemoryCounter::currentSize()
{
	return currentSize_;
}

size_t memcounter::MallinfoMemoryCounter::maximumSize()
{
	return maximumSize_;
}

int memcounter::MallinfoMemoryCounter::currentNumberOfAllocations()
{
	return currentNumberOfAllocations_;
}

int memcounter::MallinfoMemoryCounter::maximumNumberOfAllocations()
{
	return maximumNumberOfAllocations_;
}

void memcounter::MallinfoMemoryCounter::preAdd( void* pointer, size_t size )
{
	struct mallinfo memoryInfo=mallinfo();
	pre_uordblks_=memoryInfo.uordblks;
	pre_hblkhd_=memoryInfo.hblkhd;
}

void memcounter::MallinfoMemoryCounter::postAdd( void* pointer, size_t size )
{
	struct mallinfo memoryInfo=mallinfo();
	currentSize_+=( memoryInfo.uordblks-pre_uordblks_ + memoryInfo.hblkhd-pre_hblkhd_);

	if( currentSize_>maximumSize_ )
	{
		maximumSize_=currentSize_;
		maximumNumberOfAllocations_=currentNumberOfAllocations_;
	}
	++currentNumberOfAllocations_;
}

void memcounter::MallinfoMemoryCounter::preModify( void* oldPointer, void* newPointer, size_t newSize )
{
	struct mallinfo memoryInfo=mallinfo();
	pre_uordblks_=memoryInfo.uordblks;
	pre_hblkhd_=memoryInfo.hblkhd;
}

void memcounter::MallinfoMemoryCounter::postModify( void* oldPointer, void* newPointer, size_t newSize )
{
	struct mallinfo memoryInfo=mallinfo();
	currentSize_+=( memoryInfo.uordblks-pre_uordblks_ + memoryInfo.hblkhd-pre_hblkhd_);

	if( currentSize_>maximumSize_ )
	{
		maximumSize_=currentSize_;
		maximumNumberOfAllocations_=currentNumberOfAllocations_;
	}
}

void memcounter::MallinfoMemoryCounter::preRemove( void* pointer )
{
	struct mallinfo memoryInfo=mallinfo();
	pre_uordblks_=memoryInfo.uordblks;
	pre_hblkhd_=memoryInfo.hblkhd;
}

void memcounter::MallinfoMemoryCounter::postRemove( void* pointer )
{
	struct mallinfo memoryInfo=mallinfo();
	currentSize_-=( pre_uordblks_-memoryInfo.uordblks + pre_hblkhd_-memoryInfo.hblkhd);

	--currentNumberOfAllocations_;
}
