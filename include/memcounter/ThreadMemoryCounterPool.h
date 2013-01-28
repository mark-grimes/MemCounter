#ifndef memcounter_ThreadMemoryCounterPool_h
#define memcounter_ThreadMemoryCounterPool_h

#include <cstring> // in case size_t isn't declared automatically
#include <vector>
#include <list>

// Forward declarations
namespace memcounter
{
	class IMemoryCounter;
	class ICountingInterface;
	class IRecordingInterface;
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

		//
		// These methods deal with the registered ICountingInterfaces
		//
		memcounter::IMemoryCounter* createNewMemoryCounter();

		void preAddToAllEnabledCounters( void* pointer, size_t size );
		void postAddToAllEnabledCounters( void* pointer, size_t size );
		void preModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize );
		void postModifyAllEnabledCounters( void* oldPointer, void* newPointer, size_t newSize );
		void preRemoveFromAllEnabledCounters( void* pointer );
		void postRemoveFromAllEnabledCounters( void* pointer );

		void informEnabled( memcounter::ICountingInterface* pEnabledCounter );
		void informDisabled( memcounter::ICountingInterface* pDisabledCounter );

		//
		// These methods deal with the registered IRecordingInterfaces
		//
		memcounter::IMemoryCounter* createNewMemoryRecorder();

		void addToAllEnabledRecorders( size_t size );
		void modifyAllEnabledRecorders( size_t oldSize, size_t newSize );
		void removeFromAllEnabledRecorders( size_t size );

		void informEnabled( memcounter::IRecordingInterface* pEnabledCounter );
		void informDisabled( memcounter::IRecordingInterface* pDisabledCounter );

	protected:
		std::vector<memcounter::ICountingInterface*> createdCounters_;
		// I'm having performance issues so currently only using the most recent counter
		memcounter::ICountingInterface* pLastEnabledCounter_;

		std::vector<memcounter::IRecordingInterface*> createdRecorders_;
		std::list<memcounter::IRecordingInterface*> enabledRecorders_;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
