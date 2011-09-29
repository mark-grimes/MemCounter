PROJECT(marksMemoryAnalyser C CXX)
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
FIND_PACKAGE(Threads)


# Default to "release with debug info", i.e. "-O2 -g".
IF(NOT CMAKE_CONFIGURATION_TYPE AND NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Build type: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF()

# Flags we need.
ADD_DEFINITIONS(-D__STDC_FORMAT_MACROS)
ADD_DEFINITIONS(-D__STDC_LIMIT_MACROS)
IF(${CMAKE_SYSTEM_NAME} MATCHES Linux)
  ADD_DEFINITIONS(-D_GNU_SOURCE)
  SET(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS_INIT} -Wl,-z,nodefs
      CACHE STRING "Flags used by the linker during the creation of dll's.")
ENDIF()

# Use ANSI mode for C and stricter compile options with GCC.
SET(CMAKE_C_FLAGS "${CMAKE_ANSI_FLAGS} ${CMAKE_C_FLAGS}")
SET(CMAKE_REQUIRED_FLAGS ${CMAKE_ANSI_FLAGS})
#IF(CMAKE_COMPILER_IS_GNUCC)
#  ADD_DEFINITIONS(-ansi -pedantic -W -Wall -Wno-long-long -Werror)
#ENDIF()


IF(UNIX)
  SET(marksMemoryAnalyser_LIBS ${marksMemoryAnalyser_LIBS} ${CMAKE_DL_LIBS})
ENDIF()

# Change the executable script to have the correct path to the library
CONFIGURE_FILE( "${PROJECT_SOURCE_DIR}/bin/marksMemoryAnalyser.in"
                "${PROJECT_BINARY_DIR}/bin/marksMemoryAnalyser" @ONLY )

# Build targets.
INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/include)
ADD_LIBRARY(marksMemoryAnalyser SHARED
			src/hook.cc
			src/profile.cc
			src/hooksAndEntryPoints.cc
			src/memcounter/MemoryCounterManager.cpp
			src/memcounter/ThreadMemoryCounterPool.cpp
			src/memcounter/TrackingMemoryCounter.cpp
			src/memcounter/MallinfoMemoryCounter.cpp
            )

#SET_TARGET_PROPERTIES(marksMemoryAnalyser PROPERTIES LINK_FLAGS -Wl,-z,nodefs)
TARGET_LINK_LIBRARIES(marksMemoryAnalyser ${marksMemoryAnalyser_LIBS} ${CMAKE_THREAD_LIBS_INIT})
INSTALL(TARGETS marksMemoryAnalyser
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib)
INSTALL( FILES "${PROJECT_BINARY_DIR}/bin/marksMemoryAnalyser" DESTINATION bin
	PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE )
INSTALL(FILES include/memcounter/IMemoryCounter.h DESTINATION include/memcounter)

ADD_EXECUTABLE(simpleTest test/simpleTest.cc)
TARGET_LINK_LIBRARIES(simpleTest ${CMAKE_DL_LIBS})
