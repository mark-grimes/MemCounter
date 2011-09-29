#ifndef memcounter_TrackingMemoryCounter_h
#define memcounter_TrackingMemoryCounter_h

#include "memcounter/ICountingInterface.h"

#include <list>

// Forward declarations
namespace memcounter
{
	class ThreadMemoryCounterPool;
}

namespace memcounter
{
	/** @brief Implementation of the IMemoryCounter interface
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 01/Aug/2011
	 */
	class TrackingMemoryCounter : public memcounter::ICountingInterface
	{
	public:
		TrackingMemoryCounter( memcounter::ThreadMemoryCounterPool& parent );
		virtual ~TrackingMemoryCounter();

		//
		// These methods are from the IMemoryCounter interface
		//
		virtual bool setEnabled( bool enable ); ///< Returns the state before the call
		virtual bool isEnabled(); ///< Returns true if the counter is enabled
		virtual void enable();
		virtual void disable();
		virtual void reset();
		virtual void resetMaximum();

		virtual void dumpContents( std::ostream& stream=std::cout, const std::string& prefix=std::string() );
		virtual int currentSize();
		virtual int maximumSize();
		virtual int currentNumberOfAllocations();
		virtual int maximumNumberOfAllocations();

		//
		// These methods are in addition to those from the ICoutingInterface interface
		//
		virtual void preAdd( void* pointer, size_t size );
		virtual void postAdd( void* pointer, size_t size );
		virtual void preModify( void* oldPointer, void* newPointer, size_t newSize );
		virtual void postModify( void* oldPointer, void* newPointer, size_t newSize );
		virtual void preRemove( void* pointer );
		virtual void postRemove( void* pointer );
	protected:
		bool enabled_;
		std::list< std::pair<void*,size_t> > memoryAllocationSizes_;
		int currentSize_;
		int maximumSize_;
		int currentNumberOfAllocations_;
		int maximumNumberOfAllocations_;
		bool verbose_;
		memcounter::ThreadMemoryCounterPool& parent_;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
