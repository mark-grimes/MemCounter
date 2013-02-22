#ifndef memcounter_IMemoryCounter_h
#define memcounter_IMemoryCounter_h

#include <iostream>
#include <string>

#include <vector>

namespace memcounter
{
	/** @brief Interface to a class that keeps track of the size of memory blocks that get allocated.
	 *
	 * Modified 22/Feb/2013 to allow sub-counters.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/Jul/2011
	 */
	class IMemoryCounter
	{
	public:
		virtual bool setEnabled( bool enable ) = 0; ///< Returns the state before the call
		virtual bool isEnabled() const = 0; ///< Returns true if the counter is enabled
		virtual void enable() = 0;
		virtual void disable() = 0;
		virtual void reset() = 0;

		/// Leaves everything intact but sets maximumSize to currentSize and maximumNumberOfAllocations to currentNumberOfAllocations
		virtual void resetMaximum() = 0;

		virtual void dumpContents( std::ostream& stream=std::cout, const std::string& prefix=std::string() ) const = 0;
		virtual long int currentSize() const = 0;
		virtual long int maximumSize() const = 0;

		///< Returns the number of allocations still outstanding
		virtual int currentNumberOfAllocations() const = 0;

		/// Returns the number of allocations when the memory was at a maximum (N.B. this is not necessarily the same as the maximum number of allocations)
		virtual int maximumNumberOfAllocations() const = 0;

		virtual const std::vector<IMemoryCounter*>& subCounters() const = 0;
	protected:
		virtual ~IMemoryCounter() {}
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
