#include "memcounter/IntrusiveMemoryCounterManager.h"

#include <iostream>
#include <pthread.h>
#include <vector>

#include "memcounter/MutexSentry.h"
#include "memcounter/IMemoryCounter.h"
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
namespace // Use the unnamed namespace
{
	// If this key is non-zero for a given thread then the memory counting functions will just do
	// the normal behaviour, i.e. pass the calls on to the real malloc etcetera without recording
	// anything.
	pthread_key_t memcounter_threadDisabled;
}
bool memcounter_globallyDisabled=true;
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

	struct VariableMemoryBlockHeader
	{
		void* pOriginalPtr;
		size_t size;
		struct HeaderIdentifier do_not_access_this; // Imperative that this is the last member
	};

	struct FixedMemoryBlockHeader
	{
		size_t size;
		struct HeaderIdentifier do_not_access_this; // Imperative that this is the last member
	};

	/*
	 * To identify which blocks of memory have had the size put at the start of the block and
	 * the pointer incremented, I'm going to put a unique identifier in as well. It's not
	 * foolproof, but the chances of a random memory location having this exact same value are
	 * slim. The value doesn't really matter as long it's unique and unlikely to happen at
	 * random, i.e. not 0;
	 */
	const HeaderIdentifier sizeHasBeenStored={'g','%','z','('}; // random arbitrary bytes
	const HeaderIdentifier variableSizeHasBeenStored={'r','q','q','£'};


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
	protected:
		inline memcounter::ThreadMemoryCounterPool* getThreadMemoryCounterPool();
		memcounter::ThreadMemoryCounterPool* createThreadMemoryCounterPool();

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
		std::cerr << "proxyThreadStartRoutine starting" << std::endl;
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

		std::cerr << "proxyThreadStartRoutine passing to start_routine" << std::endl;

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
	if(false) std::cerr << "Creating memcounter::IntrusiveMemoryCounterManager" << std::endl;
}

memcounter::IntrusiveMemoryCounterManager::~IntrusiveMemoryCounterManager()
{
	if(false) std::cerr << "Destroying memcounter::IntrusiveMemoryCounterManager" << std::endl;
}

::IntrusiveMemoryCounterManagerImplementation::IntrusiveMemoryCounterManagerImplementation()
{
	if(true) std::cerr << "Creating memcounter::IntrusiveMemoryCounterManagerImplementation" << std::endl;

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

	// Turn off memory counting for this thread until the user enables it by explicitly enabling one of
	// the IMemoryCounter interfaces. Note that I want it to be any non-zero value, I'm only using
	// the address of memcounter_globallyDisabled to save ugly casts.
	pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

	// I'm creating the ThreadMemoryCounterPools for each thread when it starts up.  I never get the chance
	// for the main thread however, so I'll do it here since
	createThreadMemoryCounterPool();

	std::cerr << __LINE__ << " - " << __FILE__ << " pthread_self=" << pthread_self() << std::endl;

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
	memcounter_globallyDisabled=false;
}

::IntrusiveMemoryCounterManagerImplementation::~IntrusiveMemoryCounterManagerImplementation()
{
	if(true) std::cerr << "Destroying memcounter::IntrusiveMemoryCounterManagerImplementation" << std::endl;

	// Delete all of the ThreadMemoryCounterPool objects I've created. This destructor should
	// only be called at the end of program execution but I might as well tidy up in case I
	// later put some code in the ThreadMemoryCounterPool destructors.
	for( std::vector<memcounter::ThreadMemoryCounterPool*>::iterator iPool=threadPools_.begin(); iPool!=threadPools_.end(); ++iPool )
	{
		delete *iPool;
	}
}

memcounter::IMemoryCounter* ::IntrusiveMemoryCounterManagerImplementation::createNewMemoryCounter()
{
	if(false) std::cerr << "IntrusiveMemoryCounterManagerImplementation::createNewMemoryCounter() called" << std::endl;

	// Disable memory counting while I do this in case any of my calls create a
	// recursive loop.
	pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

	memcounter::IMemoryCounter* result=getThreadMemoryCounterPool()->createNewMemoryCounter();;

	// Put memory counting back on
	pthread_setspecific( memcounter_threadDisabled, 0 );

	return result;
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
		std::cerr << __LINE__ << " - " << __FILE__ << " pthread_self=" << pthread_self() << std::endl;
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
		std::cerr << __LINE__ << " - " << __FILE__ << " pthread_self=" << pthread_self() << std::endl;
	}

	return pThreadPool;
}

static void* domalloc( IgHook::SafeData<igprof_domalloc_t> &hook, size_t n )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( n );
	else
	{

		// Actually request slightly larger amount of memory
		void *originalResult=( *hook.chain )( n+sizeof(::FixedMemoryBlockHeader) );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with malloc! #####" << std::endl;
			return NULL;
		}

		// Store the size data and an identifier so that free knows there's extra data
		::FixedMemoryBlockHeader* pHeader=(::FixedMemoryBlockHeader*) originalResult;
		void* result=(void*)(pHeader+1);
		::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;

		pHeader->size=n;
		*pIdentifier=sizeHasBeenStored;


		if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
		{
			// Disable memory counting while I do this in case any of my calls create a
			// recursive loop. Any non-zero value will do, using this one to avoid casting.
			pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

			memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( n );

			// Put memory counting back on
			pthread_setspecific( memcounter_threadDisabled, 0 );
		}

		return result;
	}
}

static void* docalloc( IgHook::SafeData<igprof_docalloc_t> &hook, size_t num, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( num, size );
	else
	{

		// I first need to figure out how many more elements I need to allocate to fit
		// a MemoryBlockHeader struct in.
		size_t extraHeaderElements;
		bool useFixedHeader;

		if( size>sizeof(::VariableMemoryBlockHeader) )
		{
			extraHeaderElements=1;
			useFixedHeader=false;
		}
		else
		{
			// FixedMemoryBlockHeader is smaller than VariableMemoryBlockHeader, so if a fixed header
			// can fit I'll use that. It will only work if it fits exactly though.
			if( size<=sizeof(::FixedMemoryBlockHeader) && sizeof(::FixedMemoryBlockHeader)%size==0 )
			{
				extraHeaderElements=sizeof(::FixedMemoryBlockHeader)/size;
				useFixedHeader=true;
			}
			else
			{
				extraHeaderElements=sizeof(::VariableMemoryBlockHeader)/size;
				if( sizeof(::VariableMemoryBlockHeader)%size != 0 ) ++extraHeaderElements; // Always round up
				useFixedHeader=false;
			}
		}

		void* originalResult=( *hook.chain )( num+extraHeaderElements, size );
		if( originalResult==NULL )
		{
			std::cerr << "##### Arghh! Couldn't allocate memory with calloc! #####" << std::endl;
			return NULL;
		}

		// Get a pointer to where the memory after the header elements is
		void* result=(void*)( ((char*)originalResult) + extraHeaderElements*size );
		// Now that I have that, take off the size of the header struct so that any
		// spare space will be at the beginning and the header is right in front of
		// the memory location I pass back to the caller.
		if( useFixedHeader )
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::FixedMemoryBlockHeader* pHeader=((::FixedMemoryBlockHeader*)result)-1;
			*pIdentifier=sizeHasBeenStored;
			pHeader->size=num*size;
		}
		else
		{
			::HeaderIdentifier* pIdentifier=((::HeaderIdentifier*)result)-1;
			::VariableMemoryBlockHeader* pHeader=((::VariableMemoryBlockHeader*)result)-1;
			*pIdentifier=variableSizeHasBeenStored;
			pHeader->size=num*size;
			pHeader->pOriginalPtr=originalResult;
		}


		if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
		{
			// Disable memory counting while I do this in case any of my calls create a
			// recursive loop. Any non-zero value will do, using this one to avoid casting.
			pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

			memcounter::IntrusiveMemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( num*size );

			// Put memory counting back on
			pthread_setspecific( memcounter_threadDisabled, 0 );
		}

		return result;
	}
}

static void* dorealloc( IgHook::SafeData<igprof_dorealloc_t> &hook, void *ptr, size_t n )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( ptr, n );
	else
	{
		void* result; // the return value

		if( ptr==NULL )
		{
			// If the original ptr is NULL, realloc delegates to malloc which will be
			// caught by my hook and already have the details put at the start of
			// the block. My malloc hook will also have already reported the memory
			// allocation to any active counters, so I can return straight from here.
			result=( *hook.chain )( ptr, n );
		}
		else
		{
			void* originalPtr;
			size_t originalSize;

			// Only change the pointer if it's not null.
			::HeaderIdentifier* pIdentifier=((HeaderIdentifier*)ptr)-1;
			if( *pIdentifier==sizeHasBeenStored )
			{
				::FixedMemoryBlockHeader* pHeader=((FixedMemoryBlockHeader*)ptr)-1;
				originalSize=pHeader->size;
				originalPtr=(void*)pHeader;
			}
			else if( *pIdentifier==variableSizeHasBeenStored )
			{
				::VariableMemoryBlockHeader* pHeader=((VariableMemoryBlockHeader*)ptr)-1;
				originalSize=pHeader->size;
				originalPtr=pHeader->pOriginalPtr;
			}
			else
			{
				originalPtr=ptr;
				originalSize=0;
			}

			// Request extra memory to store the header at the start of the block
			void* originalResult=( *hook.chain )( originalPtr, n+sizeof(::FixedMemoryBlockHeader) );
			if( originalResult==NULL )
			{
				std::cerr << "##### Arghh! Couldn't allocate memory with realloc! #####" << std::endl;
				return NULL;
			}

			// Store the size data and an identifier so that free knows there's extra data
			::FixedMemoryBlockHeader* pHeader=(FixedMemoryBlockHeader*)originalResult;
			result=(void*)(((FixedMemoryBlockHeader*)originalResult)+1);
			pIdentifier=((HeaderIdentifier*)result)-1;

			pHeader->size=n;
			*pIdentifier=sizeHasBeenStored;

			if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
			{
				// Disable memory counting while I do this in case any of my calls create a
				// recursive loop. Any non-zero value will do, using this one to avoid casting.
				pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

				memcounter::IntrusiveMemoryCounterManager::instance().modifyAllEnabledCountersForCurrentThread( originalSize, n );

				// Put memory counting back on
				pthread_setspecific( memcounter_threadDisabled, 0 );
			}
		}

		return result;
	}
}

static void* domemalign( IgHook::SafeData<igprof_domemalign_t> &hook, size_t alignment, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( alignment, size );
	else
	{
		// I don't have any programs that use memalign to test with, so I'll warn the user
		std::cerr << " memcounter warning - you're program uses memalign which should work but hasn't been tested" << "\n";

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

static void* dovalloc( IgHook::SafeData<igprof_dovalloc_t> &hook, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( size );
	else
	{
		// I don't have any programs that use valloc to test with, so I'll warn the user
		std::cerr << " memcounter warning - you're program uses valloc which should work but hasn't been tested" << "\n";

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
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain )( ptr, alignment, size );
	else
	{
		// I don't have any programs that use posix_memalign to test with, so I'll warn the user
		std::cerr << " memcounter warning - you're program uses posix_memalign which should work but hasn't been tested" << "\n";

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
	if( ptr==NULL ) return;

	// Get what the original pointer was before my malloc hook changed it
	void* originalPtr;
	size_t originalSize;

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
	else // No identifier found, so this allocation wasn't caught by my malloc hooks
	{
		originalPtr=ptr;
		originalSize=0;
	}

	// Pass on to the proper free function
	( *hook.chain )( originalPtr );

	// Record the free in any active counters
	if( !memcounter_globallyDisabled && !pthread_getspecific(memcounter_threadDisabled) )
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
	pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled );

	// I have no guarantee the object will persist until the new thread actually starts to run, so
	// I'll "new" it here and delete it in my proxy start routine. I don't care about the arg variable
	// I've been passed, because that's the responsibility of the code that originally creates the
	// thread.
	ThreadCreationArguments* pThreadArgs=new ThreadCreationArguments( start_routine, arg );

	int result=hook.chain( thread, attr, &proxyThreadStartRoutine, pThreadArgs );

	// Put memory counting back on
	pthread_setspecific( memcounter_threadDisabled, 0 );

	return result;
}
