#ifndef memcounter_RecordingMemoryCounter_h
#define memcounter_RecordingMemoryCounter_h

#include "memcounter/IRecordingInterface.h"


// Forward declarations
namespace memcounter
{
	class ThreadMemoryCounterPool;
}

namespace memcounter
{
	/** @brief Implementation of the IRecordingInterface interface.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 21/Jan/2012
	 */
	class RecordingMemoryCounter : public memcounter::IRecordingInterface
	{
	public:
		RecordingMemoryCounter( memcounter::ThreadMemoryCounterPool& parent );
		virtual ~RecordingMemoryCounter();

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
		virtual size_t currentSize();
		virtual size_t maximumSize();
		virtual int currentNumberOfAllocations();
		virtual int maximumNumberOfAllocations();

		//
		// These methods are in addition to those from the ICoutingInterface interface
		//
		virtual void add( size_t size );
		virtual void modify( size_t oldSize, size_t newSize );
		virtual void remove( size_t size );
	protected:
		bool enabled_;
		size_t currentSize_;
		size_t maximumSize_;
		int currentNumberOfAllocations_;
		int maximumNumberOfAllocations_;
		bool verbose_;
		memcounter::ThreadMemoryCounterPool& parent_;
	}; // end of the MemoryCounter class

} // end of the memcounter namespace

#endif
