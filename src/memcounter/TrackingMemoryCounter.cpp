#include "memcounter/TrackingMemoryCounter.h"

#include <algorithm>

#include "memcounter/ThreadMemoryCounterPool.h"

namespace // Use the unnamed namespace
{
	/** @brief Predicate to find an entry in a list for a particular pointer.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/July/2011
	 */
	class EqualPointersPredicate
	{
	public:
		EqualPointersPredicate( void* pointerToSearchFor, bool verbose=false ) : pointerToSearchFor_(pointerToSearchFor), verbose_(verbose)
		{
		}
		bool operator()( const std::pair<void*,size_t>& pair )
		{
			if( verbose_ ) std::cerr << "Checking to see if " << pair.first << "==" << pointerToSearchFor_ << std::endl;
			return pair.first==pointerToSearchFor_;
		}
	private:
		void* pointerToSearchFor_;
		bool verbose_;
	};

} // end of the unnamed namespace

memcounter::TrackingMemoryCounter::TrackingMemoryCounter( memcounter::ThreadMemoryCounterPool& parent )
	: enabled_(false), currentSize_(0), maximumSize_(0), currentNumberOfAllocations_(0),
	  maximumNumberOfAllocations_(0), verbose_(false), parent_(parent)
{
}

memcounter::TrackingMemoryCounter::~TrackingMemoryCounter()
{
}

bool memcounter::TrackingMemoryCounter::setEnabled( bool enable )
{
	bool oldEnabled=enabled_;
	enabled_=enable;
	if( enable ) parent_.informEnabled( this );
	else parent_.informDisabled( this );
	return oldEnabled;
}

bool memcounter::TrackingMemoryCounter::isEnabled()
{
	return enabled_;
}

void memcounter::TrackingMemoryCounter::enable()
{
	parent_.informEnabled( this );
	enabled_=true;
}

void memcounter::TrackingMemoryCounter::disable()
{
	parent_.informDisabled( this );
	enabled_=false;
}

void memcounter::TrackingMemoryCounter::reset()
{
	currentSize_=0;
	maximumSize_=0;
	currentNumberOfAllocations_=0;
	maximumNumberOfAllocations_=0;
	memoryAllocationSizes_.clear();
}

void memcounter::TrackingMemoryCounter::resetMaximum()
{
	maximumSize_=currentSize_;
	maximumNumberOfAllocations_=currentNumberOfAllocations_;
}

void memcounter::TrackingMemoryCounter::dumpContents( std::ostream& stream, const std::string& prefix )
{
	int total=0;
	for( std::list< std::pair<void*,size_t> >::const_iterator iPair=memoryAllocationSizes_.begin(); iPair!=memoryAllocationSizes_.end(); ++iPair )
	{
		stream << prefix << "pointer " << iPair->first << " has size " << iPair->second << std::endl;
		total+=iPair->second;
	}

	stream << prefix << "Total sum of memory unaccounted for is " << total << std::endl;
	stream << prefix << "Running total of current size=" << currentSize_ << ", maximum size=" << maximumSize_ << std::endl;
}

int memcounter::TrackingMemoryCounter::currentSize()
{
	return currentSize_;
}

int memcounter::TrackingMemoryCounter::maximumSize()
{
	return maximumSize_;
}

int memcounter::TrackingMemoryCounter::currentNumberOfAllocations()
{
	return currentNumberOfAllocations_;
}

int memcounter::TrackingMemoryCounter::maximumNumberOfAllocations()
{
	return maximumNumberOfAllocations_;
}

void memcounter::TrackingMemoryCounter::preAdd( void* pointer, size_t size )
{
	// No operation, but need to implement the interface method
}

void memcounter::TrackingMemoryCounter::postAdd( void* pointer, size_t size )
{
	if( !pointer ) return;

#ifdef TrackingMemoryCounter_CAUTIOUS
	// For now make sure the pointer isn't already in the list
	std::list< std::pair<void*,size_t> >::const_iterator previousEntry=std::find_if( memoryAllocationSizes_.begin(), memoryAllocationSizes_.end(), EqualPointersPredicate(pointer) );
	if( previousEntry!=memoryAllocationSizes_.end() )
	{
		printf("Oh dear, malloc has allocated the same block of memory twice (%p) previous size=%lu, new size=%lu\n", pointer, previousEntry->second, size );
		return;
	}
#endif

	memoryAllocationSizes_.push_back( std::make_pair(pointer,size) );
	currentSize_+=size;
	if( currentSize_>maximumSize_ )
	{
		maximumSize_=currentSize_;
		maximumNumberOfAllocations_=currentNumberOfAllocations_;
	}
	++currentNumberOfAllocations_;
}

void memcounter::TrackingMemoryCounter::preModify( void* oldPointer, void* newPointer, size_t newSize )
{
	// No operation, but need to implement the interface method
}

void memcounter::TrackingMemoryCounter::postModify( void* oldPointer, void* newPointer, size_t newSize )
{
	if( !newPointer ) return postRemove(oldPointer);
	if( !oldPointer ) return postAdd( newPointer, newSize );

	std::list< std::pair<void*,size_t> >::iterator previousEntry;

#ifdef TrackingMemoryCounter_CAUTIOUS
	if( oldPointer!=newPointer )
	{
		previousEntry=std::find_if( memoryAllocationSizes_.begin(), memoryAllocationSizes_.end(), EqualPointersPredicate(newPointer) );
		if( previousEntry!=memoryAllocationSizes_.end() )
		{
			std::cout << "Oh dear, tried to modify to a pointer that already exists (" << newPointer << ")" << std::endl;
			return;
		}
	}
#endif

	previousEntry=std::find_if( memoryAllocationSizes_.begin(), memoryAllocationSizes_.end(), EqualPointersPredicate(oldPointer) );
	if( previousEntry==memoryAllocationSizes_.end() )
	{
		if( verbose_ ) std::cout << "Oh dear, tried to modify an entry that doesn't exist (" << oldPointer << ")" << std::endl;
		return;
	}

	currentSize_-=previousEntry->second;

	if( newSize>0 )
	{
		previousEntry->first=newPointer;
		previousEntry->second=newSize;
		currentSize_+=newSize;
		if( currentSize_>maximumSize_ )
		{
			maximumSize_=currentSize_;
			maximumNumberOfAllocations_=currentNumberOfAllocations_;
		}
	}
	else memoryAllocationSizes_.erase( previousEntry );
}

void memcounter::TrackingMemoryCounter::preRemove( void* pointer )
{
	// No operation, but need to implement the interface method
}

void memcounter::TrackingMemoryCounter::postRemove( void* pointer )
{
	if( !pointer ) return;

	std::list< std::pair<void*,size_t> >::iterator previousEntry=std::find_if( memoryAllocationSizes_.begin(), memoryAllocationSizes_.end(), EqualPointersPredicate(pointer) );
	if( previousEntry==memoryAllocationSizes_.end() )
	{
		if( verbose_ ) std::cerr << "Oh dear, tried to remove an entry that doesn't exist (" << pointer << ")" << std::endl;
		return;
	}

	currentSize_-=previousEntry->second;
	--currentNumberOfAllocations_;
	memoryAllocationSizes_.erase( previousEntry );
}
