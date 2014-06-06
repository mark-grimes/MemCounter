#ifndef memcounter_IntrusiveMemoryCounterManager_h
#define memcounter_IntrusiveMemoryCounterManager_h

#include <stddef.h> // needed for size_t
#include <vector>


// Forward declarations
namespace memcounter
{
	class IMemoryCounter;
	class ICountingInterface;
}


namespace memcounter
{
	/** @brief Manager class that gives out MemoryCounters to the correct threads.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/Jul/2011
	 */
	class IntrusiveMemoryCounterManager
	{
	public:
		/// @brief Returns the only instance of the Manager.
		static IntrusiveMemoryCounterManager& instance();

		virtual IMemoryCounter* createNewMemoryCounter() = 0;

		virtual void addToAllEnabledCountersForCurrentThread( size_t size ) = 0;
		virtual void modifyAllEnabledCountersForCurrentThread( size_t oldSize, size_t newSize ) = 0;
		virtual void removeFromAllEnabledCountersForCurrentThread( size_t size ) = 0;

		virtual std::vector<memcounter::ICountingInterface*> enabledCounters() = 0;
	protected:
		IntrusiveMemoryCounterManager();
		virtual ~IntrusiveMemoryCounterManager();
	}; // end of the IntrusiveMemoryCounterManager class

} // end of the memcounter namespace



#endif
