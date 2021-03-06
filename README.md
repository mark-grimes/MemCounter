This file last modified on 06/Jun/2014

MemCounter
==========
A simple memory counter. Other tools like igprof and valgrind do a much a better job, but
I found them too slow and obscured what I wanted with too much information. This is
actually built on top of ighook which is the foundation of igprof anyway
(http://igprof.sourceforge.net).

Note that this counter requires you to modify your code, because it's only intended to
profile particular parts of code. The speed overhead is quite small, but there is a small
memory overhead for *EACH* dynamic memory allocation in your program. So the overhead
will be significant for a program with lots of small allocations, and small for a program
with very few large allocations.

Licence is whatever igprof's is. No warranty whatsoever.

Any questions to Mark Grimes (mark.grimes@bristol.ac.uk).


Git branches
------------
Currently I have two branches

    master
    implementTrackedMemory

_master_ is the default implementation that adds up any memory requested and subtracts
the memory when _free_ is called only while the IMemoryCounters are active. This means
that if your program allocates memory while the IMemoryCounter is active, and passes the
memory on to, say, a framework function which frees it when the counter is disabled it
will look like you have a memory leak.

_implementTrackedMemory_ remembers which IMemoryCounters were active when the memory was
allocated and informs them when the memory is released, regardless of whether they are
active or not. You will need to use this branch if your code passes memory on to
framework functions.

At some point I'll reconcile them into one branch, but I haven't gotten around to it yet.


Installation
------------
CMake is required for the build. It has lots of options but a basic set of instructions
is (from the project source directory):

    mkdir build
    cd build
    cmake ..  # argument is the project source directory
    make
    make install

You can change the install prefix with

    cmake -DCMAKE_INSTALL_PREFIX=/my/install/directory ..

when you invoke the cmake command. 


Modifying your code
-------------------
All control is through the memcounter::IMemoryCounter interface. To get an instance of
one of these classes there is a symbol in the library you need to load by hand. The
symbol is a function that takes no arguments and returns an IMemoryCounter pointer. To
get the symbol do something like the following code:

    // Somewhere at the start...
    #include "[install directory]/include/memcounter/IMemoryCounter.h"

    //
    // Somewhere before the code you want to profile...
    //

    // Define a function pointer that will point to the symbol
    IMemoryCounter* (*createNewMemoryCounter)( void );
    // Define a pointer to the IMemoryCounter that I will get
    IMemoryCounter* pMyMemoryCounter=NULL;

    // Try and load the symbol. If the program has been invoked with the
    // intrusiveMemoryAnalyser command the library will already be in memory.
    // If it was invoked normally then this call will return NULL, so be prepared
    // for that.
    if( void *sym = dlsym(0, "createNewMemoryCounter") )
    {
        createNewMemoryCounter = __extension__(IMemoryCounter*(*)(void)) sym;
        pMyMemoryCounter=createNewMemoryCounter();
    }
    else
    {
        std::cerr << "Couldn't get the profiler symbol. Did you invoke your program with intrusiveMemoryAnalyser?" << std::endl;
        // Program will continue without profiling.
    }

    //
    // Just before the section of code that you want to profile...
    //
    if( pMyMemoryCounter ) pMyMemoryCounter->enable(); // Wrap in an if in case the library wasn't loaded
    myFunctionCallThatIWantToProfile();
    if( pMyMemoryCounter ) pMyMemoryCounter->disable();

    //
    // It's up to you to output the information in a way you want, e.g.
    //
    if( pMyMemoryCounter ) // Wrap with "if" in case profiling is off
    {
        std::cout << "myFunctionCallThatIWantToProfile still has " << pMyMemoryCounter->currentSize() << " bytes retained in "
            << pMyMemoryCounter->currentNumberOfAllocations() << " allocations. The maximum was " << pMyMemoryCounter->maximumSize()
            << " and " << pMyMemoryCounter->maximumNumberOfAllocations() << " allocations." << std::endl;
    }
	
You can have several IMemoryCounters running at any time, so you can create many with the
createNewMemoryCounter symbol and have them analysing different parts of code. You can
switch them on and off at will, so you can have one for a particular class and switch it
on just for any method calls and follow memory for the life of the object.


Invoking the analyser
---------------------
The profiling will only be on if your program is started with the intrusiveMemoryAnalyser
program, like so:

    [install prefix]/bin/intrusiveMemoryAnalyser myProgramToAnalyse myArgument1 myArgument2

If your program is started normally then the dlsym(0, "createNewMemoryCounter") won't
work and the program will run as normal, provided you don't try and use the uninitialised
pMyMemoryCounter pointer.

This currently *ONLY WORKS WITH THE STANDARD MALLOC ALLOCATORS*, e.g. glibc. If you're
using a non-standard allocator like jemalloc switch back to glibc while profiling. The
counter counts what the program requests, not what the allocator allocates so the results
would be the same anyway.


A note about threading
----------------------
This should work with threading, although this is the first thread aware program I've
written so be wary. Each thread has its own pool of counters though, so if a portion
of analysed code starts a new thread any memory allocated in that thread will not be
counted in the original thread. The new thread will have to get an IMemoryCounter
object, call "enable" on it and call the counter information methods itself.



A note about the code
---------------------
The following files are from ighook and nothing to do with me. Ighook is part of the
rather good igprof available from igprof.sourceforge.net

    include/hook.h
    include/macros.h
    include/profile.h
    src/hook.cc
    src/profile.cc

Most of the functionality is in memcounter/IntrusiveMemoryCounterManager, so that's the
best place to start.

The program works by using ighook to create proxy functions for malloc, free and the
like. When your program makes a call to any of these (or the C++ new and delete which
are built on top of them) the memcounter intercepts the call and records what it was in
any enabled counters.

Of course, free doesn't know what the size of the block to be free'd is. The underlying
implementation works that out, but rather than digging that deep memcounter stores the
size of every allocation at the start of the memory block. It then tells your program
that the memory starts just past the stored size. When it comes to free the memory, the
proxy free function changes the pointer back and passes the original pointer onto the
actual free function. This is where the memory overhead comes from.