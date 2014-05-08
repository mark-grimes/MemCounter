#ifndef memcounter_MemoryCounterImplementation_h
#define memcounter_MemoryCounterImplementation_h

#include "memcounter/ICountingInterface.h"


// Forward declarations
namespace memcounter
{
	class ThreadMemoryCounterPool;
}

namespace memcounter
{
	/** @brief Implementation of the ICountingInterface interface.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 21/Jan/2012
	 */
	class MemoryCounterImplementation : public memcounter::ICountingInterface
	{
	public:
		MemoryCounterImplementation( memcounter::ThreadMemoryCounterPool& parentPool );
		MemoryCounterImplementation( memcounter::MemoryCounterImplementation* pParentCounter );
		virtual ~MemoryCounterImplementation();

		//
		// These methods are from the IMemoryCounter interface
		//
		virtual bool setEnabled( bool enable ); ///< Returns the state before the call
		virtual bool isEnabled() const; ///< Returns true if the counter is enabled
		virtual void enable();
		virtual void disable();
		virtual void reset();
		virtual void resetMaximum();

		virtual void dumpContents( std::ostream& stream=std::cout, const std::string& prefix=std::string() ) const;
		virtual long int currentSize() const;
		virtual long int maximumSize() const;
		virtual int currentNumberOfAllocations() const;
		virtual int maximumNumberOfAllocations() const;

		virtual const std::vector<IMemoryCounter*>& subCounters() const;

		//
		// These methods are from the ICountingInterface interface
		//
		virtual void add( size_t size );
		virtual void modify( size_t oldSize, size_t newSize );
		virtual void remove( size_t size );

	private:
		//
		// These methods are only for sub-counters to inform their parent that
		// they have been enabled or disabled.
		//
		void childEnabled( memcounter::MemoryCounterImplementation* pEnabledSubCounter );
		void childDisabled( memcounter::MemoryCounterImplementation* pDisabledSubCounter );
		void rawSetEnabled( bool enable ); ///< Allows a parent to set the status of a sub-counter without causing a notification
	protected:
		bool enabled_;
		long int currentSize_;
		long int maximumSize_;
		int currentNumberOfAllocations_;
		int maximumNumberOfAllocations_;
		bool verbose_;
		std::vector<IMemoryCounter*> subCounters_;
		memcounter::MemoryCounterImplementation* pCurrentlyActiveSubCounter_;

		/// Only one of parentPool_ and parentCounter_ will be non null, depending on how the counter was constructed
		memcounter::ThreadMemoryCounterPool* pParentPool_;
		/// Only one of parentPool_ and parentCounter_ will be non null, depending on how the counter was constructed
		memcounter::MemoryCounterImplementation* pParentCounter_;

	}; // end of the MemoryCounterImplementation class

} // end of the memcounter namespace

#endif
