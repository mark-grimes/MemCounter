#include "memcounter/IMemoryCounter.h"

#include <dlfcn.h>
#include <vector>
#include <iostream>


namespace // Use the unnamed namespace
{
	/** @brief Dummy class that I can instantiate to check memory tracing
	 *
	 */
	class DummyClass
	{
	public:
		float a,b,c;
		int d;
	};

} // end of the unnamed namespace

int main( int argc, char* argv[] )
{
	using memcounter::IMemoryCounter;

	// My current build settings throw errors if argc or argv aren't used, so
	// put in some dummy uses to shut the compiler up.
	if( argc>0 )
	{
		argv[0]=argv[0];
	}

	IMemoryCounter* (*createNewMemoryCounter)( void );

	if( void *sym = dlsym(0, "createNewMemoryCounter") )
	{
		createNewMemoryCounter = __extension__(IMemoryCounter*(*)(void)) sym;
	}
	else
	{
		std::cout << "Couldn't get the symbol" << std::endl;
		return -1;
	}

	IMemoryCounter* pMemoryCounter=createNewMemoryCounter();
	if( !pMemoryCounter )
	{
		std::cout << "The memory counter pointer is NULL" << std::endl;
		return -2;
	}

	pMemoryCounter->enable();

	std::cout << "Managed to get a pointer to the memory counter okay" << std::endl;

	std::cout << "Size of dummy class is " << sizeof(DummyClass) << std::endl;

	std::vector<DummyClass*> dummyClassPointers( 10 ); // Create a vector with 10 NULL entries
	for( std::vector<DummyClass*>::iterator iPointer=dummyClassPointers.begin(); iPointer!=dummyClassPointers.end(); ++iPointer )
	{
		*iPointer=new DummyClass;
	}

	DummyClass blah;
	blah.a=8;

	std::cout << "\n" << "Memory contents after " << dummyClassPointers.size() << " creations of DummyClass:" << std::endl;
	pMemoryCounter->dumpContents();

	for( std::vector<DummyClass*>::iterator iPointer=dummyClassPointers.begin(); iPointer!=dummyClassPointers.end(); ++iPointer )
	{
		delete *iPointer;
	}

	std::cout << "\n" << "Memory contents after " << dummyClassPointers.size() << " deletions of DummyClass:" << std::endl;
	pMemoryCounter->dumpContents();

	return 0;
}
