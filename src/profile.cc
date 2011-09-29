#include "profile.h"

#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * I stripped this file to the bare minimum so that hook.cc can compile and run. The other
 * functionality has either been dropped or moved to other files.
 * Mark Grimes 31/Jul/2011
 */

/** Internal printf()-like debugging utility.  Produces output if
 $IGPROF_DEBUGGING environment variable is set to any value.  */
void igprof_debug( const char *format, ... )
{
	static const char *debugging=0;//igprof_getenv( "IGPROF_DEBUGGING" );
	char msgbuf[1024];
	char *msg=msgbuf;
	int left=sizeof(msgbuf);
	int out=0;
	int len;

	if( debugging )
	{
		timeval tv;
		gettimeofday( &tv, 0 );
		len=snprintf( msg, left, "*** IgProf(%lu, %.3f): ", (unsigned long)getpid(), tv.tv_sec + 1e-6 * tv.tv_usec );
		ASSERT( len < left );
		left-=len;
		msg+=len;
		out+=len;

		va_list args;
		va_start( args, format );
		len=vsnprintf( msg, left, format, args );
		va_end( args );

		out+=(len > left ? left : len);
		write( 2, msgbuf, out );
	}
}
