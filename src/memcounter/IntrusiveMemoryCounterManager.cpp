#include "memcounter/IntrusiveMemoryCounterManager.h"

#include <iostream>
#include <pthread.h>
#include <vector>
#include <memory>

#include "memcounter/MutexSentry.h"
#include "memcounter/IMemoryCounter.h"
#include "memcounter/ICountingInterface.h"
#include "memcounter/ThreadMemoryCounterPool.h"
#include "memcounter/DisablingFunctions.h"

#include <malloc.h>
#include <unistd.h> // Required to get the pagesize for valloc

// The IgHook library
#include <cstdlib>
#include <stddef.h>
#include "macros.h"
#include "hook.h"

// These are the hook functions
DUAL_HOOK(1, void *, domalloc, _main, _libc, (size_t n), (n), "malloc", 0, "libc.so.6")
DUAL_HOOK(2, void *, docalloc, _main, _libc, (size_t n, size_t m), (n, m), "calloc", 0, "libc.so.6")
DUAL_HOOK(2, void *, dorealloc, _main, _libc, (void *ptr, size_t n), (ptr, n), "realloc", 0, "libc.so.6")
DUAL_HOOK(3, int, dopmemalign, _main, _libc, (void **ptr, size_t alignment, size_t size), (ptr, alignment, size), "posix_memalign", 0, "libc.so.6")
DUAL_HOOK(2, void *, domemalign, _main, _libc, (size_t alignment, size_t size), (alignment, size), "memalign", 0, "libc.so.6")
DUAL_HOOK(1, void *, dovalloc, _main, _libc, (size_t size), (size), "valloc", 0, "libc.so.6")
DUAL_HOOK(1, void, dofree, _main, _libc, (void *ptr), (ptr), "free", 0, "libc.so.6")

DUAL_HOOK(1, void, doexit, _main, _libc, (int code), (code), "exit", 0, "libc.so.6")
DUAL_HOOK(1, void, doexit, _main2, _libc2, (int code), (code), "_exit", 0, "libc.so.6")
DUAL_HOOK(2, int, dokill, _main, _libc, (pid_t pid, int sig), (pid, sig), "kill", 0, "libc.so.6")

LIBHOOK(4, int, dopthread_create, _main, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", 0, 0)
LIBHOOK(4, int, dopthread_create, _pthread20, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", "GLIBC_2.0", 0)
LIBHOOK(4, int, dopthread_create, _pthread21, (pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)(void *), void *arg), (thread, attr, start_routine, arg), "pthread_create", "GLIBC_2.1", 0)

// These are the implementations of the functions defined in DisablingFunctions.h
// This is the extern from the disabling functions
bool memcounter_globallyDisabled=true;

namespace // Use the unnamed namespace
{
	// If this key is non-zero for a given thread then the memory counting functions will just do
	// the normal behaviour, i.e. pass the calls on to the real malloc etcetera without recording
	// anything.
	pthread_key_t memcounter_threadDisabled;

	// If this key is non-zero it indicates that I'm doing some internal stuff on this thread.
	// The hooks should not do anything and pass on to the real functions, otherwise I'll probably
	// start an infinite loop.
	pthread_key_t memcounter_threadInternalUsage;

	/** @brief Class that sets memcounter_threadInternalUsage to non-zero and returns it to its previous
	 * value when it goes out of scope.
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 30/May/2014
	 */
	class RecursiveHookGuard
	{
	public:
		RecursiveHookGuard() : previousValue_( pthread_getspecific(memcounter_threadInternalUsage) )
		{
			// I want to set this to any non-zero value. I'll just use the address
			// of the member to avoid horrible casting.
			pthread_setspecific(memcounter_threadInternalUsage,&previousValue_);
		}
		~RecursiveHookGuard()
		{
			pthread_setspecific(memcounter_threadInternalUsage,previousValue_);
		}
		/// @brief Needs to be performed only once at program startup
		static void initialise()
		{
			if( !( pthread_key_create(&memcounter_threadInternalUsage,NULL)==0 ) )
			{
				std::cerr << "Oh dear, couldn't create a key for some reason" << std::endl;
			}
		}
		/** @brief Returns true if the current thread is doing something internal for the memory counting, so the
		 * hooks should just return the defaults. */
		static bool internalUsage()
		{
			if( pthread_getspecific(memcounter_threadInternalUsage)==0 ) return false;
			else return true;
		}
		/// @brief Forces the current value of what internalUsage() returns.
		static void forceCurrentValue( bool active )
		{
			// Any non-zero value will do. use the address of memcounter_globallyDisabled for convenience
			if( active ) pthread_setspecific(memcounter_threadInternalUsage,&memcounter_globallyDisabled);
			else pthread_setspecific(memcounter_threadInternalUsage,0);
		}
	private:
		const void* previousValue_;
		//static pthread_key_t memcounter_threadInternalUsage;
	};
}

void memcounter::enableThisThread()
{
//	std::cerr << " *** Enabling thread *** " << std::endl;
	pthread_setspecific( memcounter_threadDisabled, 0 );
}

void memcounter::disableThisThread()
{
//	std::cerr << " *** Disabling thread *** " << std::endl;
	pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );
}

/*
 * This is the main entry point for external applications to get access
 * the MemCounter functionality. The following code is the sort of thing
 * that should be used to call this function:
 *
 *
 *	// Define the function pointer
 *	using memcounter::IMemoryCounter;
 *	IMemoryCounter* (*createNewMemoryCounter)( void );
 *	if( void *sym = dlsym(0, "createNewMemoryCounter") )
 *	{
 *		// Set the function pointer
 *		createNewMemoryCounter = __extension__(IMemoryCounter*(*)(void)) sym;
 *		// Use the function
 *		memcounter::IMemoryCounter* pMemoryCounter=createNewMemoryCounter();
 *		// Do stuff...
 *	}
 *
 *
 */
using memcounter::IMemoryCounter;
extern "C"
{
	VISIBLE IMemoryCounter* createNewMemoryCounter( void )
	{
		return memcounter::IntrusiveMemoryCounterManager::instance().createNewMemoryCounter();
	}
}


namespace // Use the unnamed namespace
{
	struct HeaderIdentifier
	{
		// I've found that putting this structure in the MemoryBlockHeader structures
		// adds this size of this rounded up to 8 to their sizes. Therefore 8 char identifiers
		// take up as much space as 1, so to be a little better protected against
		// accidental matches to random memory locations all add a few more in.
		char identifier1;
		char identifier2;
		char identifier3;
		char identifier4;
	};

	bool operator==( const struct HeaderIdentifier& header1, const HeaderIdentifier& header2 )
	{
		return header1.identifier1==header2.identifier1
				&& header1.identifier2==header2.identifier2
				&& header1.identifier3==header2.identifier3
				&& header1.identifier4==header2.identifier4;
	}

	std::ostream& operator<<( std::ostream& theStream, const struct HeaderIdentifier& header )
	{
		theStream << header.identifier1 << header.identifier2 << header.identifier3 << header.identifier4;
		return theStream;
	}

	struct VariableMemoryBlockHeader
	{
		void* pOriginalPtr;
		size_t size;
		/** Imperative that this is the last member. N.B. I play with byte alignment so the member might not
		 * actually be accessible here. It's declared so as to reserve enough size in the structure for it. */
		struct HeaderIdentifier do_not_access_this;
	};

	struct FixedMemoryBlockHeader
	{
		size_t size;
		/** Imperative that this is the last member. N.B. I play with byte alignment so the member might not
		 * actually be accessible here. It's declared so as to reserve enough size in the structure for it. */
		struct HeaderIdentifier do_not_access_this;
	};

	/** @Memory block header that keeps track of which IMemoryCounters should be notified when it's freed
	 *
	 * Using this memory block header is sometimes necessary when passing memory around. If the memory is
	 * released when the required IMemoryCounters are not or cannot be active, then there will be an accounting
	 * surplus. It will look like there are memory leaks when there actually aren't.
	 */
	struct TrackedMemoryBlockHeader
	{
		void *pOriginalPtr;
		std::vector<memcounter::ICountingInterface*>* pCountersToNotify;
		size_t size;
		/** Imperative that this is the last member. N.B. I play with byte alignment so the member might not
		 * actually be accessible here. It's declared so as to reserve enough size in the structure for it. */
		struct HeaderIdentifier do_not_access_this;
	};

	/*
	 * To identify which blocks of memory have had the size put at the start of the block and
	 * the pointer incremented, I'm going to put a unique identifier in as well. It's not
	 * foolproof, but the chances of a random memory location having this exact same value are
	 * slim. The value doesn't really matter as long it's unique and unlikely to happen at
	 * random, e.g. not 0;
	 */
	const HeaderIdentifier sizeHasBeenStored={'g','%','z','('}; // random arbitrary bytes
	const HeaderIdentifier variableSizeHasBeenStored={'r','q','q','$'};
	const HeaderIdentifier trackedSizeHasBeenStored={'x','q','&','}'};


	/** @brief Implementation of the IntrusiveMemoryCounterManager.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/July/2011
	 */
	class IntrusiveMemoryCounterManagerImplementation : public memcounter::IntrusiveMemoryCounterManager
	{
		friend void* proxyThreadStartRoutine( void *pThreadCreationArguments );;
	public:
		IntrusiveMemoryCounterManagerImplementation();
		~IntrusiveMemoryCounterManagerImplementation();
		memcounter::IMemoryCounter* createNewMemoryCounter();
		virtual void addToAllEnabledCountersForCurrentThread( size_t size );
		virtual void modifyAllEnabledCountersForCurrentThread( size_t oldSize, size_t newSize );
		virtual void removeFromAllEnabledCountersForCurrentThread( size_t size );
		virtual std::vector<memcounter::ICountingInterface*> enabledCounters();
	protected:
		inline memcounter::ThreadMemoryCounterPool* getThreadMemoryCounterPool();
		memcounter::ThreadMemoryCounterPool* createThreadMemoryCounterPool();

		const int verbosity_;
		pthread_key_t keyThreadMemoryCounterPool_;
		pthread_mutex_t mutex_;
		std::vector<memcounter::ThreadMemoryCounterPool*> threadPools_; //< @Keep track of the allocated pools so that I can delete them at the end
	}; // end of the IntrusiveMemoryCounterManagerImplementation class

	/*
	 * I *think* creating the only instance in this way is thread safe. It should be created at program/library
	 * invocation when there's only one thread, so I guess so.  I normally use the Meyer's singleton pattern
	 * but I've read somewhere that it's not thread safe, I don't fully understand why not though.
	 */
	IntrusiveMemoryCounterManagerImplementation onlyInstance;

	/** @brief Struct to wrap thread creation function pointer and arguments in.
	 * @author Mark Grimes
	 * @date 01/Sep/2011
	 */
	struct ThreadCreationArguments
	{
		void* (*pFunction_)(void *);
		void* pArguments_;
		bool createPool_;
		ThreadCreationArguments( void* (*pFunction)(void *), void* pArguments, bool createMemoryCounterPool=true )
			: pFunction_(pFunction), pArguments_(pArguments), createPool_(createMemoryCounterPool) { /* No operation except the initialiser list*/ }
	};

	/** @brief Function that will be passed as the creation function for all threads. It will do my stuff then pass on to the original function.
	 * @author Mark Grimes
	 * @date 05/Sep/2011
	 */
	void* proxyThreadStartRoutine( void *pThreadCreationArguments )
	{
		std::cerr << "proxyThreadStartRoutine starting on thread pthread_self=" << pthread_self() << std::endl;
		// First disable memory counting for this thread while I set stuff up. I want it set to any non
		// zero value, I'll use the address of memcounter_globallyDisabled purely for convenience.
		pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

		// Copy out the required info before I delete the ThreadCreationArguments object that was 'new'ed
		// in the creating thread.
		ThreadCreationArguments* pCreationArguments=static_cast<ThreadCreationArguments*>(pThreadCreationArguments);
		bool createPool=pCreationArguments->createPool_;
		void* pArguments=pCreationArguments->pArguments_;
		void* (&start_routine)(void *)=*pCreationArguments->pFunction_;
		delete pCreationArguments;

		// Create a memory counter pool if required
		if( createPool ) onlyInstance.createThreadMemoryCounterPool();

		// I only want memory counting enabled for this thread if a pool has been created.
		// Otherwise any memory allocation in this thread would try and call a non-existent
		// pool and cause a segfault.
		if( createPool ) pthread_setspecific( memcounter_threadDisabled, 0 );
		::RecursiveHookGuard::forceCurrentValue(false);

		std::cerr << "proxyThreadStartRoutine passing to start_routine on thread pthread_self=" << pthread_self() << std::endl;

		// Now pass on to the function that the caller originally wanted
		return start_routine(pArguments);
	}

} // end of the unnamed namespace

memcounter::IntrusiveMemoryCounterManager& memcounter::IntrusiveMemoryCounterManager::instance()
{
	return onlyInstance;
}

memcounter::IntrusiveMemoryCounterManager::IntrusiveMemoryCounterManager()
{
}

memcounter::IntrusiveMemoryCounterManager::~IntrusiveMemoryCounterManager()
{
}

::IntrusiveMemoryCounterManagerImplementation::IntrusiveMemoryCounterManagerImplementation() : verbosity_(1)
{
	if(verbosity_>0) std::cerr << "Creating memcounter::IntrusiveMemoryCounterManagerImplementation on thread pthread_self=" << pthread_self() << std::endl;

	if( !( pthread_key_create(&keyThreadMemoryCounterPool_,NULL)==0 ) )
	{
		std::cerr << "Oh dear, couldn't create a key for some reason" << std::endl;
	}

	if( !( pthread_key_create(&memcounter_threadDisabled,NULL)==0 ) )
	{
		std::cerr << "Oh dear, couldn't create a key for some reason" << std::endl;
	}

	if( !( pthread_mutex_init(&mutex_,NULL)==0 ) )
	{
		std::cerr << "Oh dear, couldn't create the mutex for some reason" << std::endl;
	}

	::RecursiveHookGuard::initialise();

	// Turn off memory counting for this thread until the user enables it by explicitly enabling one of
	// the IMemoryCounter interfaces. Note that I want it to be any non-zero value, I'm only using
	// the address of memcounter_globallyDisabled to save ugly casts.
	pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

	// I'm creating the ThreadMemoryCounterPools for each thread when it starts up.  I never get the chance
	// for the main thread however, so I'll do it here since
	createThreadMemoryCounterPool();

	if( memcounter_globallyDisabled )
	{
		IgHook::hook( domalloc_hook_main.raw );
		IgHook::hook( docalloc_hook_main.raw );
		IgHook::hook( dorealloc_hook_main.raw );
		IgHook::hook( dopmemalign_hook_main.raw );
		IgHook::hook( domemalign_hook_main.raw );
		IgHook::hook( dovalloc_hook_main.raw );
		IgHook::hook( dofree_hook_main.raw );

		IgHook::hook( doexit_hook_main.raw );
		IgHook::hook( dokill_hook_main.raw );
		IgHook::hook( dopthread_create_hook_main.raw );
		IgHook::hook( dopthread_create_hook_pthread20.raw );
		IgHook::hook( dopthread_create_hook_pthread21.raw );
	}
	else std::cerr << " *** Oh dear *** " << std::endl;

	std::cerr << " *** memcounteractive ***" << std::endl;
//	std::cerr << " *** sizes *** HeaderIdentifier=" << sizeof(::HeaderIdentifier) << " FixedMemoryBlockHeader=" << sizeof(::FixedMemoryBlockHeader)
//			<< " VariableMemoryBlockHeader=" << sizeof(::VariableMemoryBlockHeader)
//			<< " void*=" << sizeof(void*) << " char=" << sizeof(char) << " size_t=" << sizeof(size_t)
//			 << " Test=" << sizeof(::Test)<< std::endl;
	// Now that everything is setup, enable the hooks
	::RecursiveHookGuard::forceCurrentValue(false);
	memcounter_globallyDisabled=false;
}

::IntrusiveMemoryCounterManagerImplementation::~IntrusiveMemoryCounterManagerImplementation()
{
	if(verbosity_>0) std::cerr << "Destroying memcounter::IntrusiveMemoryCounterManagerImplementation on thread pthread_self=" << pthread_self() << std::endl;

	// Delete all of the ThreadMemoryCounterPool objects I've created. This destructor should
	// only be called at the end of program execution but I might as well tidy up in case I
	// later put some code in the ThreadMemoryCounterPool destructors.
// Having odd crashes because there appears to be more than one IntrusiveMemoryCounterManagerImplementation being
// deleted. For now I'll just leak the memory while I'm testing to see what the problem is.
//	for( std::vector<memcounter::ThreadMemoryCounterPool*>::iterator iPool=threadPools_.begin(); iPool!=threadPools_.end(); ++iPool )
//	{
//		delete *iPool;
//	}
}

memcounter::IMemoryCounter* ::IntrusiveMemoryCounterManagerImplementation::createNewMemoryCounter()
{
	if(verbosity_>1) std::cerr << "IntrusiveMemoryCounterManagerImplementation::createNewMemoryCounter() called" << std::endl;

	// Disable memory counting while I do this in case any of my calls create a
	// recursive loop.
	::RecursiveHookGuard myGuard;

	return getThreadMemoryCounterPool()->createNewMemoryCounter();;
}

void ::IntrusiveMemoryCounterManagerImplementation::addToAllEnabledCountersForCurrentThread( size_t size )
{
	getThreadMemoryCounterPool()->addToAllEnabledCounters( size );
}

void ::IntrusiveMemoryCounterManagerImplementation::modifyAllEnabledCountersForCurrentThread( size_t oldSize, size_t newSize )
{
	getThreadMemoryCounterPool()->modifyAllEnabledCounters( oldSize, newSize );
}

void ::IntrusiveMemoryCounterManagerImplementation::removeFromAllEnabledCountersForCurrentThread( size_t size )
{
	getThreadMemoryCounterPool()->removeFromAllEnabledCounters( size );
}

std::vector<memcounter::ICountingInterface*> IntrusiveMemoryCounterManagerImplementation::enabledCounters()
{
	return getThreadMemoryCounterPool()->enabledCounters();
}

inline memcounter::ThreadMemoryCounterPool* ::IntrusiveMemoryCounterManagerImplementation::getThreadMemoryCounterPool()
{
//	return createThreadMemoryCounterPool();
	return static_cast<memcounter::ThreadMemoryCounterPool*>( pthread_getspecific(keyThreadMemoryCounterPool_) );
}

memcounter::ThreadMemoryCounterPool* ::IntrusiveMemoryCounterManagerImplementation::createThreadMemoryCounterPool()
{
	// I'm fairly sure I don't need a lock here, because I'm using all local variables which will be on the
	// stack, and each thread has it's own stack. The only shared variable is keyThreadMemoryCounterPool_,
	// and I'm assuming the pthread routines are themselves thread safe.

	// Make sure this thread doesn't already have a pool
	memcounter::ThreadMemoryCounterPool* pThreadPool=static_cast<memcounter::ThreadMemoryCounterPool*>( pthread_getspecific(keyThreadMemoryCounterPool_) );
	if( !pThreadPool )
	{
		// The ThreadMemoryCounterPool for this thread hasn't been created yet
		pThreadPool=new memcounter::ThreadMemoryCounterPool;
		pthread_setspecific(keyThreadMemoryCounterPool_,pThreadPool);
		// I'll keep track of all of these pools so that I can delete them later. All other access is
		// done using pthread_getspecific() so that's all this vector is used for.
		{
			// I think I do need a lock here though
			memcounter::MutexSentry mutexSentry( mutex_ );
			threadPools_.push_back(pThreadPool);
		}
	}

	return pThreadPool;
}

static void* domalloc( IgHook::SafeData<igprof_domalloc_t> &hook, size_t n )
{
	if( memcounter_globallyDisabled || ::RecursiveHookGuard::internalUsage() || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( n );
	else
	{
		::RecursiveHookGuard myGuard; // This should stop recursive calls of this hook

		// Actually request slightly larger amount of memory
		void *originalResult=( *hook.chain )( n+sizeof(::TrackedMemoryBlockHeader) );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with malloc! #####" << std::endl;
			return NULL;
		}

		// Store the size data and an identifier so that free knows there's extra data
		::TrackedMemoryBlockHeader* pHeader=(::TrackedMemoryBlockHeader*) originalResult;
		void* result=(void*)(pHeader+1);
		pHeader->pOriginalPtr=originalResult;
		pHeader->pCountersToNotify=NULL;
		::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;

		pHeader->size=n;
		*pIdentifier=trackedSizeHasBeenStored;

		std::vector<memcounter::ICountingInterface*> enabledCounters=memcounter::IntrusiveMemoryCounterManager::instance().enabledCounters();
		if( !enabledCounters.empty() )
		{
			pHeader->pCountersToNotify=new std::vector<memcounter::ICountingInterface*>( enabledCounters );
			for( std::vector<memcounter::ICountingInterface*>::iterator iCounter=enabledCounters.begin(); iCounter!=enabledCounters.end(); ++iCounter )
			{
				(*iCounter)->add(n);
			}
		}

		return result;
	}
}

static void* docalloc( IgHook::SafeData<igprof_docalloc_t> &hook, size_t num, size_t size )
{
	if( true || memcounter_globallyDisabled || ::RecursiveHookGuard::internalUsage() || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( num, size );
	else
	{
		::RecursiveHookGuard myGuard; // This should stop recursive calls of this hook

		size_t extraHeaderElements=sizeof(::TrackedMemoryBlockHeader)/size;
		if( sizeof(::TrackedMemoryBlockHeader)%size != 0 ) ++extraHeaderElements; // Always round up

		void* originalResult=( *hook.chain )( num+extraHeaderElements, size );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with calloc! #####" << std::endl;
			return NULL;
		}

		// Get a pointer to where the memory after the header elements is
		void* result=(void*)( ((char*)originalResult) + extraHeaderElements*size );

		::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
		::TrackedMemoryBlockHeader* pHeader=((::TrackedMemoryBlockHeader*)result)-1;
		*pIdentifier=trackedSizeHasBeenStored;
		pHeader->size=num*size;
		pHeader->pOriginalPtr=originalResult;
		pHeader->pCountersToNotify=NULL;


		memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( num*size );
		std::vector<memcounter::ICountingInterface*> enabledCounters=memcounter::IntrusiveMemoryCounterManager::instance().enabledCounters();
		if( !enabledCounters.empty() )
		{
			pHeader->pCountersToNotify=new std::vector<memcounter::ICountingInterface*>( enabledCounters );
		}

		return result;
	}
}

static void* dorealloc( IgHook::SafeData<igprof_dorealloc_t> &hook, void *ptr, size_t n )
{
	if( ::RecursiveHookGuard::internalUsage() ) return ( *hook.chain )( ptr, n );
	::RecursiveHookGuard myGuard;

	void* originalPtr;
	size_t addedSize;
	::HeaderIdentifier headerIdentifier;

	// Only change the pointer if it's not null.
	if( ptr==NULL )
	{
		originalPtr=ptr;
		addedSize=0;
		// Set the header identifier to anything that doesn't match a preset
		//headerIdentifier={0,0,0,0};
		headerIdentifier.identifier1=0;
		headerIdentifier.identifier2=0;
		headerIdentifier.identifier3=0;
		headerIdentifier.identifier4=0;
	}
	else
	{
		// Look at the memory just before what the users program thinks the memory
		// block is. If it's some memory that I've added then there will be a header
		// identifier there. If not then it's just a standard memory block and ptr
		// is the correct pointer to it.
		headerIdentifier=*(((HeaderIdentifier*)ptr)-1);

		if( headerIdentifier==sizeHasBeenStored )
		{
			::FixedMemoryBlockHeader* pHeader=((FixedMemoryBlockHeader*)ptr)-1;
			originalPtr=(void*)pHeader;
			addedSize=sizeof(::FixedMemoryBlockHeader);
		}
		else if( headerIdentifier==variableSizeHasBeenStored )
		{
			::VariableMemoryBlockHeader* pHeader=((VariableMemoryBlockHeader*)ptr)-1;
			originalPtr=pHeader->pOriginalPtr;
			addedSize=sizeof(::VariableMemoryBlockHeader);
		}
		else if( headerIdentifier==trackedSizeHasBeenStored )
		{
			::TrackedMemoryBlockHeader* pHeader=((TrackedMemoryBlockHeader*)ptr)-1;
			originalPtr=pHeader->pOriginalPtr;
			addedSize=sizeof(::TrackedMemoryBlockHeader);
		}
		else
		{
			originalPtr=ptr;
			addedSize=0;
		}
	}

	// Request extra memory to store the header at the start of the block
	void* originalResult=( *hook.chain )( originalPtr, n+addedSize );
	if( originalResult==NULL )
	{
		std::cerr << "##### Arghh! Couldn't allocate memory with realloc! #####" << std::endl;
		return NULL;
	}

	void* result; // the return value

	if( headerIdentifier==sizeHasBeenStored )
	{
		::FixedMemoryBlockHeader* pHeader=(FixedMemoryBlockHeader*)originalResult;
		memcounter::IntrusiveMemoryCounterManager::instance().modifyAllEnabledCountersForCurrentThread( pHeader->size, n );
		pHeader->size=n;
		result=(void*)(((FixedMemoryBlockHeader*)originalResult)+1);
	}
	else if( headerIdentifier==variableSizeHasBeenStored )
	{
		::VariableMemoryBlockHeader* pHeader=(VariableMemoryBlockHeader*)originalResult;
		memcounter::IntrusiveMemoryCounterManager::instance().modifyAllEnabledCountersForCurrentThread( pHeader->size, n );
		pHeader->size=n;
		pHeader->pOriginalPtr=originalResult;
		result=(void*)(((VariableMemoryBlockHeader*)originalResult)+1);
	}
	else if( headerIdentifier==trackedSizeHasBeenStored )
	{
		::TrackedMemoryBlockHeader* pHeader=(TrackedMemoryBlockHeader*)originalResult;
		if( pHeader->pCountersToNotify )
		{
			for( std::vector<memcounter::ICountingInterface*>::iterator iCounter=pHeader->pCountersToNotify->begin(); iCounter!=pHeader->pCountersToNotify->end(); ++iCounter )
			{
				(*iCounter)->modify( pHeader->size, n );
			}
		}

		pHeader->size=n;
		pHeader->pOriginalPtr=originalResult;
		result=(void*)(((TrackedMemoryBlockHeader*)originalResult)+1);
	}
	else result=originalResult;

	return result;
}

static void* domemalign( IgHook::SafeData<igprof_domemalign_t> &hook, size_t alignment, size_t size )
{
	if( memcounter_globallyDisabled || ::RecursiveHookGuard::internalUsage() || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( alignment, size );
	else
	{
		::RecursiveHookGuard myGuard;

		// I don't have any programs that use memalign to test with, so I'll warn the user
		std::cerr << " memcounter warning - your program uses memalign which should work but hasn't been tested" << "\n";

		size_t alignedHeaderSize; // This is how many multiples of the alignment, not the actual size

		if( alignment>sizeof(::TrackedMemoryBlockHeader) ) alignedHeaderSize=1;
		else
		{
			alignedHeaderSize=sizeof(::TrackedMemoryBlockHeader)/alignment;
			if( sizeof(::TrackedMemoryBlockHeader)%alignment != 0 ) ++alignedHeaderSize; // Always round up
		}

		void* originalResult=( *hook.chain )( alignment, size+alignment*alignedHeaderSize );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with calloc! #####" << std::endl;
			return NULL;
		}

		// Get a pointer to where the memory after the header elements is
		void* result=(void*)( ((char*)originalResult) + alignment*alignedHeaderSize );
		// Now that I have that, take off the size of the header struct so that any
		// spare space will be at the beginning and the header is right in front of
		// the memory location I pass back to the caller.
		::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
		::TrackedMemoryBlockHeader* pHeader=((::TrackedMemoryBlockHeader*)result)-1;
		*pIdentifier=trackedSizeHasBeenStored;
		pHeader->size=size;
		pHeader->pOriginalPtr=originalResult;
		pHeader->pCountersToNotify=NULL;

		memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( size );
		std::vector<memcounter::ICountingInterface*> enabledCounters=memcounter::IntrusiveMemoryCounterManager::instance().enabledCounters();
		if( !enabledCounters.empty() )
		{
			pHeader->pCountersToNotify=new std::vector<memcounter::ICountingInterface*>( enabledCounters );
		}

		return result;
	}

}

static void* dovalloc( IgHook::SafeData<igprof_dovalloc_t> &hook, size_t size )
{
	if( memcounter_globallyDisabled || ::RecursiveHookGuard::internalUsage() || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( size );
	else
	{
		::RecursiveHookGuard myGuard;

		// I don't have any programs that use valloc to test with, so I'll warn the user
		std::cerr << " memcounter warning - your program uses valloc which should work but hasn't been tested" << "\n";

		long alignment=sysconf(_SC_PAGESIZE);
		size_t alignedHeaderSize; // This is how many multiples of the alignment, not the actual size
		bool useFixedHeader;

		if( alignment>sizeof(::VariableMemoryBlockHeader) )
		{
			alignedHeaderSize=1;
			useFixedHeader=false;
		}
		else
		{
			// FixedMemoryBlockHeader is smaller than VariableMemoryBlockHeader, so if a fixed header
			// can fit I'll use that. It will only work if it fits exactly though.
			if( alignment<=sizeof(::FixedMemoryBlockHeader) && sizeof(::FixedMemoryBlockHeader)%alignment==0 )
			{
				alignedHeaderSize=sizeof(::FixedMemoryBlockHeader)/alignment;
				useFixedHeader=true;
			}
			else
			{
				alignedHeaderSize=sizeof(::VariableMemoryBlockHeader)/alignment;
				if( sizeof(::VariableMemoryBlockHeader)%alignment != 0 ) ++alignedHeaderSize; // Always round up
				useFixedHeader=false;
			}
		}

		void* originalResult=( *hook.chain )( size+alignment*alignedHeaderSize );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with calloc! #####" << std::endl;
			return NULL;
		}

		// Get a pointer to where the memory after the header elements is
		void* result=(void*)( ((char*)originalResult) + alignment*alignedHeaderSize );
		// Now that I have that, take off the size of the header struct so that any
		// spare space will be at the beginning and the header is right in front of
		// the memory location I pass back to the caller.
		if( useFixedHeader )
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::FixedMemoryBlockHeader* pHeader=((::FixedMemoryBlockHeader*)result)-1;
			*pIdentifier=sizeHasBeenStored;
			pHeader->size=size;
		}
		else
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::VariableMemoryBlockHeader* pHeader=((::VariableMemoryBlockHeader*)result)-1;
			*pIdentifier=variableSizeHasBeenStored;
			pHeader->size=size;
			pHeader->pOriginalPtr=originalResult;
		}


		if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
		{
			// Disable memory counting while I do this in case any of my calls create a
			// recursive loop. Any non-zero value will do, using this one to avoid casting.
			pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

			memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( size );

			// Put memory counting back on
			pthread_setspecific( memcounter_threadDisabled, 0 );
		}

		return result;
	}
}

static int dopmemalign( IgHook::SafeData<igprof_dopmemalign_t> &hook, void **ptr, size_t alignment, size_t size )
{
	if( memcounter_globallyDisabled || ::RecursiveHookGuard::internalUsage() || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( ptr, alignment, size );
	else
	{
		::RecursiveHookGuard myGuard;

		// I don't have any programs that use posix_memalign to test with, so I'll warn the user
		std::cerr << " memcounter warning - your program uses posix_memalign which should work but hasn't been tested" << "\n";

		size_t alignedHeaderSize; // This is how many multiples of the alignment, not the actual size
		bool useFixedHeader;

		if( alignment>sizeof(::VariableMemoryBlockHeader) )
		{
			alignedHeaderSize=1;
			useFixedHeader=false;
		}
		else
		{
			// FixedMemoryBlockHeader is smaller than VariableMemoryBlockHeader, so if a fixed header
			// can fit I'll use that. It will only work if it fits exactly though.
			if( alignment<=sizeof(::FixedMemoryBlockHeader) && sizeof(::FixedMemoryBlockHeader)%alignment==0 )
			{
				alignedHeaderSize=sizeof(::FixedMemoryBlockHeader)/alignment;
				useFixedHeader=true;
			}
			else
			{
				alignedHeaderSize=sizeof(::VariableMemoryBlockHeader)/alignment;
				if( sizeof(::VariableMemoryBlockHeader)%alignment != 0 ) ++alignedHeaderSize; // Always round up
				useFixedHeader=false;
			}
		}

		void* originalResult;
		int returnValue=( *hook.chain )( &originalResult, alignment, size+alignment*alignedHeaderSize );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with calloc! #####" << std::endl;
			*ptr=NULL;
			return returnValue;
		}

		// Get a pointer to where the memory after the header elements is
		void* result=(void*)( ((char*)originalResult) + alignment*alignedHeaderSize );
		// Now that I have that, take off the size of the header struct so that any
		// spare space will be at the beginning and the header is right in front of
		// the memory location I pass back to the caller.
		if( useFixedHeader )
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::FixedMemoryBlockHeader* pHeader=((::FixedMemoryBlockHeader*)result)-1;
			*pIdentifier=sizeHasBeenStored;
			pHeader->size=size;
		}
		else
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::VariableMemoryBlockHeader* pHeader=((::VariableMemoryBlockHeader*)result)-1;
			*pIdentifier=variableSizeHasBeenStored;
			pHeader->size=size;
			pHeader->pOriginalPtr=originalResult;
		}


		if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
		{
			// Disable memory counting while I do this in case any of my calls create a
			// recursive loop. Any non-zero value will do, using this one to avoid casting.
			pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

			memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( size );

			// Put memory counting back on
			pthread_setspecific( memcounter_threadDisabled, 0 );
		}

		*ptr=result;
		return returnValue;
	}
}

static void dofree( IgHook::SafeData<igprof_dofree_t> &hook, void *ptr )
{
	if( ::RecursiveHookGuard::internalUsage() ) return ( *hook.chain )( ptr );
	::RecursiveHookGuard myGuard;

	if( ptr==NULL ) return;

	// Get what the original pointer was before my malloc hook changed it
	void* originalPtr;
	size_t originalSize;

	// This is only used if the memory block header is a TrackedMemoryBlockHeader
	std::auto_ptr< std::vector<memcounter::ICountingInterface*> > pCountersToNotify;

	::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)ptr)-1;
	if( *pIdentifier==sizeHasBeenStored )
	{
		::FixedMemoryBlockHeader* pHeader=((::FixedMemoryBlockHeader*)ptr)-1;
		originalSize=pHeader->size;
		originalPtr=(void*)pHeader;
	}
	else if( *pIdentifier==variableSizeHasBeenStored )
	{
		::VariableMemoryBlockHeader* pHeader=((::VariableMemoryBlockHeader*)ptr)-1;
		originalSize=pHeader->size;
		originalPtr=pHeader->pOriginalPtr;
	}
	else if( *pIdentifier==trackedSizeHasBeenStored )
	{
		::TrackedMemoryBlockHeader* pHeader=((::TrackedMemoryBlockHeader*)ptr)-1;
		originalSize=pHeader->size;
		originalPtr=pHeader->pOriginalPtr;
		pCountersToNotify.reset( pHeader->pCountersToNotify );
	}
	else // No identifier found, so this allocation wasn't caught by my malloc hooks
	{
		pIdentifier=NULL;
		originalPtr=ptr;
		originalSize=0;
	}

	( *hook.chain )( originalPtr );

	// Record the free in any active counters. If the memory header was a TrackedMemoryBlockHeader then the
	// counters specified should be notified whether they're active or not.
	if( pCountersToNotify.get()!=NULL )
	{
		for( std::vector<memcounter::ICountingInterface*>::iterator iCounter=pCountersToNotify->begin(); iCounter!=pCountersToNotify->end(); ++iCounter )
		{
			(*iCounter)->remove( originalSize );
		}
	}
	else if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) && originalSize!=0 )
	{
		// Disable memory counting while I do this in case any of my calls create a
		// recursive loop. Any non-zero value will do, using this one to avoid casting.
		pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

		memcounter::IntrusiveMemoryCounterManager::instance().removeFromAllEnabledCountersForCurrentThread( originalSize );

		// Put memory counting back on
		pthread_setspecific( memcounter_threadDisabled, 0 );
	}
}

/** Trapped calls to exit() and _exit().  */
static void doexit( IgHook::SafeData<igprof_doexit_t> &hook, int code )
{
	std::cerr << "*** Custom doexit called ***" << std::endl;
//	memcounter_globallyDisabled=true;
	hook.chain( code );
}

/** Trapped calls to kill().  Dump out profiler data if the signal
 looks dangerous.  Mostly really to trap calls to abort().  */
static int dokill( IgHook::SafeData<igprof_dokill_t> &hook, pid_t pid, int sig )
{
	std::cerr << "*** Custom dokill called ***" << std::endl;
//	memcounter_globallyDisabled=true;
	return hook.chain( pid, sig );
}

/** Trap thread creation to run per-profiler initialisation.  */
static int dopthread_create( IgHook::SafeData<igprof_dopthread_create_t> &hook, pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)( void * ), void *arg )
{
	std::cerr << "*** Custom dopthread_create called ***" << std::endl;
	// Disable memory counting while I do this in case any of my calls create a
	// recursive loop.
	::RecursiveHookGuard myGuard;

	// I have no guarantee the object will persist until the new thread actually starts to run, so
	// I'll "new" it here and delete it in my proxy start routine. I don't care about the arg variable
	// I've been passed, because that's the responsibility of the code that originally creates the
	// thread.
	ThreadCreationArguments* pThreadArgs=new ThreadCreationArguments( start_routine, arg );

	return hook.chain( thread, attr, &proxyThreadStartRoutine, pThreadArgs );
}
