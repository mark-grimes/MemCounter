//#include "hooksAndEntryPoints.h"

#include "memcounter/MemoryCounterManager.h"

#include "macros.h"

bool memcounter_globallyDisabled=true;

using memcounter::IMemoryCounter;

extern "C"
{
	VISIBLE IMemoryCounter* createNewMemoryCounter( void )
	{
		return memcounter::MemoryCounterManager::instance().createNewMemoryCounter();
	}
}

//void* domalloc( IgHook::SafeData<igprof_domalloc_t> &hook, size_t n )
//{
//	if( memcounter_globallyDisabled ) return ( *hook.chain)( n );
//	else
//	{
//		memcounter_globallyDisabled=true;
//		void *result=( *hook.chain)( n );
//		memcounter::MemoryCounterManager::instance().addToAllEnabledCountersForCurrentThread( result, n );
//		memcounter_globallyDisabled=false;
//		return result;
//	}
//}
//
//static void* docalloc( IgHook::SafeData<igprof_docalloc_t> &hook, size_t n, size_t m )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		return ( *hook.chain)( n, m );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		void *result=( *hook.chain)( n, m );
//		MemoryCounterImplementation::instance().add( result, n*m );
//		MemoryCounterImplementation::instance().enable();
//		return result;
//	}
//}
//
//static void* dorealloc( IgHook::SafeData<igprof_dorealloc_t> &hook, void *ptr, size_t n )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		return ( *hook.chain)( ptr, n );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		void *result=( *hook.chain)( ptr, n );
//		MemoryCounterImplementation::instance().modify( ptr, result, n );
//		MemoryCounterImplementation::instance().enable();
//		return result;
//	}
//}
//
//static void* domemalign( IgHook::SafeData<igprof_domemalign_t> &hook, size_t alignment, size_t size )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		return ( *hook.chain)( alignment, size );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		void *result=( *hook.chain)( alignment, size );
//		// I think I should probably round "size" up to the nearest alignment size, but I'll ignore that for now.
//		MemoryCounterImplementation::instance().add( result, size );
//		MemoryCounterImplementation::instance().enable();
//		return result;
//	}
//}
//
//static void* dovalloc( IgHook::SafeData<igprof_dovalloc_t> &hook, size_t size )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		return ( *hook.chain)( size );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		void *result=( *hook.chain)( size );
//		// I think I should probably round "size" up to the nearest page size, but I'll ignore that for now.
//		MemoryCounterImplementation::instance().add( result, size );
//		MemoryCounterImplementation::instance().enable();
//		return result;
//	}
//}
//
//static int dopmemalign( IgHook::SafeData<igprof_dopmemalign_t> &hook, void **ptr, size_t alignment, size_t size )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		return ( *hook.chain)( ptr, alignment, size );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		int result=( *hook.chain)( ptr, alignment, size );
//		// I think I should probably round "size" up to the nearest alignment size, but I'll ignore that for now.
//		MemoryCounterImplementation::instance().add( *ptr, size );
//		MemoryCounterImplementation::instance().enable();
//		return result;
//	}
//}
//
//static void dofree( IgHook::SafeData<igprof_dofree_t> &hook, void *ptr )
//{
//	if( memcounter_globallyDisabled || !MemoryCounterImplementation::instance().isEnabled() )
//	{
//		( *hook.chain)( ptr );
//	}
//	else
//	{
//		MemoryCounterImplementation::instance().disable();
//		MemoryCounterImplementation::instance().remove( ptr );
//		( *hook.chain)( ptr );
//		MemoryCounterImplementation::instance().enable();
//	}
//}
//
///** Trapped calls to exit() and _exit().  */
//static void doexit( IgHook::SafeData<igprof_doexit_t> &hook, int code )
//{
//	std::cerr << "*** Custom doexit called ***" << std::endl;
//	memcounter_globallyDisabled=true;
////	MemoryCounterImplementation::instance().disable();
////	MemoryCounterImplementation::instance().dumpContents();
//	hook.chain( code );
//}
//
///** Trapped calls to kill().  Dump out profiler data if the signal
// looks dangerous.  Mostly really to trap calls to abort().  */
//static int dokill( IgHook::SafeData<igprof_dokill_t> &hook, pid_t pid, int sig )
//{
//	std::cerr << "*** Custom dokill called ***" << std::endl;
//	memcounter_globallyDisabled=true;
//	return hook.chain( pid, sig );
//}
//
///** Trap thread creation to run per-profiler initialisation.  */
//static int dopthread_create( IgHook::SafeData<igprof_dopthread_create_t> &hook, pthread_t *thread, const pthread_attr_t *attr, void * (*start_routine)( void * ), void *arg )
//{
//	std::cerr << "*** Custom dopthread_create called ***" << std::endl;
//	return hook.chain( thread, attr, start_routine, arg );
//}
