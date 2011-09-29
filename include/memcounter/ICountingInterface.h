#ifndef memcounter_ICountingInterface_h
#define memcounter_ICountingInterface_h

#include "memcounter/IMemoryCounter.h"

namespace memcounter
{
	/** @brief Internal use interface to tell the memory counter about new memory allocations/deallocations.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 08/Aug/2011
	 */
	class ICountingInterface : public memcounter::IMemoryCounter
	{
	public:
		virtual ~ICountingInterface() {}

		virtual void preAdd( void* pointer, size_t size ) = 0;
		virtual void postAdd( void* pointer, size_t size ) = 0;
		virtual void preModify( void* oldPointer, void* newPointer, size_t newSize ) = 0;
		virtual void postModify( void* oldPointer, void* newPointer, size_t newSize ) = 0;
		virtual void preRemove( void* pointer ) = 0;
		virtual void postRemove( void* pointer ) = 0;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
