#ifndef memcounter_ICountingInterface_h
#define memcounter_ICountingInterface_h

#include "memcounter/IMemoryCounter.h"

namespace memcounter
{
	/** @brief Internal use interface to tell the memory counter about new memory allocations/deallocations.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 21/Jan/2013
	 */
	class ICountingInterface : public memcounter::IMemoryCounter
	{
	public:
		virtual ~ICountingInterface() {}

		virtual void add( size_t size ) = 0;
		virtual void modify( size_t oldSize, size_t newSize ) = 0;
		virtual void remove( size_t size ) = 0;
	}; // end of the ICountingInterface class

} // end of the memcounter namespace

#endif
