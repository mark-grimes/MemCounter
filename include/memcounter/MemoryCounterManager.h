#ifndef memcounter_MemoryCounterManager_h
#define memcounter_MemoryCounterManager_h

#include <stddef.h> // needed for size_t


// Forward declarations
namespace memcounter
{
	class IMemoryCounter;
}


namespace memcounter
{
	/** @brief Manager class that gives out MemoryCounters to the correct threads.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/Jul/2011
	 */
	class MemoryCounterManager
	{
	public:
		/// @brief Returns the only instance of the Manager.
		static MemoryCounterManager& instance();

		virtual IMemoryCounter* createNewMemoryCounter() = 0;

		virtual void preAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size ) = 0;
		virtual void postAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size ) = 0;
		virtual void preModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize ) = 0;
		virtual void postModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize ) = 0;
		virtual void preRemoveFromAllEnabledCountersForCurrentThread( void* pointer ) = 0;
		virtual void postRemoveFromAllEnabledCountersForCurrentThread( void* pointer ) = 0;
	protected:
		MemoryCounterManager();
		virtual ~MemoryCounterManager();
	}; // end of the MemoryCounterManager class

} // end of the memcounter namespace



#endif
