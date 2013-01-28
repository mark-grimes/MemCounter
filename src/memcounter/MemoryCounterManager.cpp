#include "memcounter/MemoryCounterManager.h"

#include <iostream>
#include <pthread.h>
#include <vector>

#include "memcounter/MutexSentry.h"
#include "memcounter/IMemoryCounter.h"
#include "memcounter/ThreadMemoryCounterPool.h"
#include "memcounter/DisablingFunctions.h"

#include <malloc.h>

// The IgHook library
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
bool memcounter_globallyDisabled=false;
void memcounter::enableThisThread()
{
	pthread_setspecific( memcounter_threadDisabled, 0 );
}

void memcounter::disableThisThread()
{
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
		return memcounter::MemoryCounterManager::instance().createNewMemoryCounter();
	}
}


namespace // Use the unnamed namespace
{
	/** @brief Sentry using RAII to block memory counting while I'm doing some internal memory manipulation.
	 *
	 * The memory counting functions check that memcounter_threadDisabled is zero before doing anything, so
	 * to block memory counting I can set that to anything non-zero. I'll use the address of memcounter_globallyDisabled
	 * purely for convenience so that I don't have to do any casting.
	 *
	 * @author Mark Grimes
	 * @date 05/Sep/2011
	 */
	class DisableMemCounterSentry
	{
	public:
		DisableMemCounterSentry() { pthread_setspecific( memcounter_threadDisabled, &memcounter_globallyDisabled ); }
		~DisableMemCounterSentry() { pthread_setspecific( memcounter_threadDisabled, 0 ); }
	};

	/** @brief Implementation of the MemoryCounterManager.
	 *
	 * @author Mark Grimes (mark.grimes@bristol.ac.uk)
	 * @date 31/July/2011
	 */
	class MemoryCounterManagerImplementation : public memcounter::MemoryCounterManager
	{
		friend void* proxyThreadStartRoutine( void *pThreadCreationArguments );;
	public:
		MemoryCounterManagerImplementation();
		~MemoryCounterManagerImplementation();
		memcounter::IMemoryCounter* createNewMemoryCounter();
		virtual void preAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size );
		virtual void postAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size );
		virtual void preModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize );
		virtual void postModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize );
		virtual void preRemoveFromAllEnabledCountersForCurrentThread( void* pointer );
		virtual void postRemoveFromAllEnabledCountersForCurrentThread( void* pointer );
	protected:
		inline memcounter::ThreadMemoryCounterPool* getThreadMemoryCounterPool();
		memcounter::ThreadMemoryCounterPool* createThreadMemoryCounterPool();

		pthread_key_t keyThreadMemoryCounterPool_;
		pthread_mutex_t mutex_;
		std::vector<memcounter::ThreadMemoryCounterPool*> threadPools_; //< @Keep track of the allocated pools so that I can delete them at the end
	}; // end of the MemoryCounterManagerImplementation class

	/*
	 * I *think* creating the only instance in this way is thread safe. It should be created at program/library
	 * invocation when there's only one thread, so I guess so.  I normally use the Meyer's singleton pattern
	 * but I've read somewhere that it's not thread safe, I don't fully understand why not though.
	 */
	MemoryCounterManagerImplementation onlyInstance;


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
		// First disable memory counting for this thread while I set stuff up. I want it set to any non
		// zero value, I'll use the address of memcounter_globallyDisabled purely for convenience.
		// I don't want to use the DisableMemCounterSentry object because I don't necessarily want to
		// ever enable it.
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

		// Now pass on to the function that the caller originally wanted
		return start_routine(pArguments);
	}

} // end of the unnamed namespace

memcounter::MemoryCounterManager& memcounter::MemoryCounterManager::instance()
{
	return onlyInstance;
}

memcounter::MemoryCounterManager::MemoryCounterManager()
{
	if(false) std::cout << "Creating memcounter::MemoryCounterManager" << std::endl;
}

memcounter::MemoryCounterManager::~MemoryCounterManager()
{
	if(false) std::cout << "Destroying memcounter::MemoryCounterManager" << std::endl;
}

::MemoryCounterManagerImplementation::MemoryCounterManagerImplementation()
{
	if(true) std::cout << "Creating memcounter::MemoryCounterManagerImplementation" << std::endl;

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

	// I'm creating the ThreadMemoryCounterPools for each thread when it starts up.  I never get the chance
	// for the main thread however, so I'll do it here since
	createThreadMemoryCounterPool();

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

	// Now that everything is setup, enable the hooks
	memcounter_globallyDisabled=false;
	if(false) std::cout << "memcounter_globallyDisabled=" << memcounter_globallyDisabled << std::endl;
}

::MemoryCounterManagerImplementation::~MemoryCounterManagerImplementation()
{
	if(true) std::cout << "Destroying memcounter::MemoryCounterManagerImplementation" << std::endl;

	// Delete all of the ThreadMemoryCounterPool objects I've created. This destructor should
	// only be called at the end of program execution but I might as well tidy up in case I
	// later put some code in the ThreadMemoryCounterPool destructors.
	for( std::vector<memcounter::ThreadMemoryCounterPool*>::iterator iPool=threadPools_.begin(); iPool!=threadPools_.end(); ++iPool )
	{
		delete *iPool;
	}
}

memcounter::IMemoryCounter* ::MemoryCounterManagerImplementation::createNewMemoryCounter()
{
	if(false) std::cout << "MemoryCounterManagerImplementation::createNewMemoryCounter() called" << std::endl;

	// Disable memory counting while I do this.
	::DisableMemCounterSentry countingDisabledWhileInScope;

	return getThreadMemoryCounterPool()->createNewMemoryCounter();;
}

void ::MemoryCounterManagerImplementation::preAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size )
{
	getThreadMemoryCounterPool()->preAddToAllEnabledCounters( pointer, size );
}

void ::MemoryCounterManagerImplementation::postAddToAllEnabledCountersForCurrentThread( void* pointer, size_t size )
{
	getThreadMemoryCounterPool()->postAddToAllEnabledCounters( pointer, size );
}

void ::MemoryCounterManagerImplementation::preModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize )
{
	getThreadMemoryCounterPool()->preModifyAllEnabledCounters( oldPointer, newPointer, newSize );
}

void ::MemoryCounterManagerImplementation::postModifyAllEnabledCountersForCurrentThread( void* oldPointer, void* newPointer, size_t newSize )
{
	getThreadMemoryCounterPool()->postModifyAllEnabledCounters( oldPointer, newPointer, newSize );
}

void ::MemoryCounterManagerImplementation::preRemoveFromAllEnabledCountersForCurrentThread( void* pointer )
{
	getThreadMemoryCounterPool()->preRemoveFromAllEnabledCounters( pointer );
}

void ::MemoryCounterManagerImplementation::postRemoveFromAllEnabledCountersForCurrentThread( void* pointer )
{
	getThreadMemoryCounterPool()->postRemoveFromAllEnabledCounters( pointer );
}

inline memcounter::ThreadMemoryCounterPool* ::MemoryCounterManagerImplementation::getThreadMemoryCounterPool()
{
//	return createThreadMemoryCounterPool();
	return static_cast<memcounter::ThreadMemoryCounterPool*>( pthread_getspecific(keyThreadMemoryCounterPool_) );
}

memcounter::ThreadMemoryCounterPool* ::MemoryCounterManagerImplementation::createThreadMemoryCounterPool()
{
	// I'm fairly sure I don't need a lock here, because I'm using all local variables which will be on the
	// stack, and each thread has it's own stack. The only shared variable is keyThreadMemoryCounterPool_,
	// and I'm assuming the pthread routines are themselves thread safe.

	// Make sure this thread doesn't already have a pool
	memcounter::ThreadMemoryCounterPool* pThreadPool=static_cast<memcounter::ThreadMemoryCounterPool*>( pthread_getspecific(keyThreadMemoryCounterPool_) );
	if( !pThreadPool )
	{
		std::cout << __LINE__ << " - " << __FILE__ << " pthread_self=" << pthread_self() << std::endl;
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
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( n );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preAddToAllEnabledCountersForCurrentThread( NULL, n );
		void *result=( *hook.chain)( n );
		memcounter::MemoryCounterManager::instance().postAddToAllEnabledCountersForCurrentThread( result, n );
		return result;
	}
}

static void* docalloc( IgHook::SafeData<igprof_docalloc_t> &hook, size_t n, size_t m )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( n, m );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preAddToAllEnabledCountersForCurrentThread( NULL, n*m );
		void *result=( *hook.chain)( n, m );
		memcounter::MemoryCounterManager::instance().postAddToAllEnabledCountersForCurrentThread( result, n*m );
		return result;
	}
}

static void* dorealloc( IgHook::SafeData<igprof_dorealloc_t> &hook, void *ptr, size_t n )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( ptr, n );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preModifyAllEnabledCountersForCurrentThread( ptr, NULL, n );
		void *result=( *hook.chain)( ptr, n );
		memcounter::MemoryCounterManager::instance().postModifyAllEnabledCountersForCurrentThread( ptr, result, n );
		return result;
	}
}

static void* domemalign( IgHook::SafeData<igprof_domemalign_t> &hook, size_t alignment, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( alignment, size );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preAddToAllEnabledCountersForCurrentThread( NULL, size );
		void *result=( *hook.chain)( alignment, size );
		// I think I should probably round "size" up to the nearest alignment size, but I'll ignore that for now.
		memcounter::MemoryCounterManager::instance().postAddToAllEnabledCountersForCurrentThread( result, size );
		return result;
	}
}

static void* dovalloc( IgHook::SafeData<igprof_dovalloc_t> &hook, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( size );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preAddToAllEnabledCountersForCurrentThread( NULL, size );
		void *result=( *hook.chain)( size );
		// I think I should probably round "size" up to the nearest page size, but I'll ignore that for now.
		memcounter::MemoryCounterManager::instance().postAddToAllEnabledCountersForCurrentThread( result, size );
		return result;
	}
}

static int dopmemalign( IgHook::SafeData<igprof_dopmemalign_t> &hook, void **ptr, size_t alignment, size_t size )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) return ( *hook.chain)( ptr, alignment, size );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preAddToAllEnabledCountersForCurrentThread( *ptr, size );
		int result=( *hook.chain)( ptr, alignment, size );
		// I think I should probably round "size" up to the nearest alignment size, but I'll ignore that for now.
		memcounter::MemoryCounterManager::instance().postAddToAllEnabledCountersForCurrentThread( *ptr, size );
		return result;
	}
}

static void dofree( IgHook::SafeData<igprof_dofree_t> &hook, void *ptr )
{
	if( memcounter_globallyDisabled || pthread_getspecific(memcounter_threadDisabled) ) ( *hook.chain)( ptr );
	else
	{
		::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.
		memcounter::MemoryCounterManager::instance().preRemoveFromAllEnabledCountersForCurrentThread( ptr );
		hook.chain( ptr );
		memcounter::MemoryCounterManager::instance().postRemoveFromAllEnabledCountersForCurrentThread( ptr );
	}
}

/** Trapped calls to exit() and _exit().  */
static void doexit( IgHook::SafeData<igprof_doexit_t> &hook, int code )
{
//	std::cerr << "*** Custom doexit called ***" << std::endl;
	memcounter_globallyDisabled=true;
	hook.chain( code );
}

/** Trapped calls to kill().  Dump out profiler data if the signal
 looks dangerous.  Mostly really to trap calls to abort().  */
static int dokill( IgHook::SafeData<igprof_dokill_t> &hook, pid_t pid, int sig )
{
//	std::cerr << "*** Custom dokill called ***" << std::endl;
	memcounter_globallyDisabled=true;
	return hook.chain( pid, sig );
}

/** Trap thread creation to run per-profiler initialisation.  */
static int dopthread_create( IgHook::SafeData<igprof_dopthread_create_t> &hook, pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)( void * ), void *arg )
{
	std::cout << "*** Custom dopthread_create called ***" << std::endl;
	::DisableMemCounterSentry countingDisabledWhileInScope; // Disable memory counting while I do this.

	// I have no guarantee the object will persist until the new thread actually starts to run, so
	// I'll "new" it here and delete it in my proxy start routine. I don't care about the arg variable
	// I've been passed, because that's the responsibility of the code that originally creates the
	// thread.
	ThreadCreationArguments* pThreadArgs=new ThreadCreationArguments( start_routine, arg );

	int result=hook.chain( thread, attr, &proxyThreadStartRoutine, pThreadArgs );

	return result;
}
