#ifndef memcounter_IRecordingInterface_h
#define memcounter_IRecordingInterface_h

#include "memcounter/IMemoryCounter.h"

namespace memcounter
{
	/** @brief Internal use interface to tell the memory counter about new memory allocations/deallocations.
	 *
	 * This interface is used when the size of memory deallocations is known beforehand, e.g. for the
	 * intrusive memory counter that stores the size in the block of memory itself.
	 * The ICountingInterface is used when it's not, so the MemoryCounter has to work it out for itself,
	 * e.g. by recording the size of each allocation and looking it up when the memory is deallocated
	 * (as in TrackingMemoryCounter).
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 21/Jan/2013
	 */
	class IRecordingInterface : public memcounter::IMemoryCounter
	{
	public:
		virtual ~IRecordingInterface() {}

		virtual void add( size_t size ) = 0;
		virtual void modify( size_t oldSize, size_t newSize ) = 0;
		virtual void remove( size_t size ) = 0;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
