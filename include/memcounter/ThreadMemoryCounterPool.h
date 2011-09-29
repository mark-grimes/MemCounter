#ifndef memcounter_ThreadMemoryCounterPool_h
#define memcounter_ThreadMemoryCounterPool_h

#include <vector>

// Forward declarations
namespace memcounter
{
	class IMemoryCounter;
	class ICountingInterface;
}

namespace memcounter
{
	/** @brief Class to keep track of all MemoryCounters for the thread. There will be one instance per thread.
	 *
	 * The name "ThreadMemoryCounterPool" is meant that it's the thread's pool of MemoryCounters, not a pool of
	 * threads.
	 * I don't need to do any locking for this class. Since each thread has its own instance only one thread will
	 * ever be going through the methods.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/Jul/2011
	 */
	class ThreadMemoryCounterPool
	{
	public:
		ThreadMemoryCounterPool();
		virtual ~ThreadMemoryCounterPool();
		memcounter::IMemoryCounter* createNewMemoryCounter();


		void preAddToAllEnabledCounters( void* pointer, size_t size );
		void postAddToAllEnabledCounters( void* pointer, size_t size );
		void preModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize );
		void postModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize );
		void preRemoveFromAllEnabledCounters( void* pointer );
		void postRemoveFromAllEnabledCounters( void* pointer );

		void informEnabled( memcounter::ICountingInterface* pEnabledCounter );
		void informDisabled( memcounter::ICountingInterface* pDisabledCounter );
	protected:
		std::vector<memcounter::ICountingInterface*> createdCounters_;
		memcounter::ICountingInterface* pLastEnabledCounter_;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
