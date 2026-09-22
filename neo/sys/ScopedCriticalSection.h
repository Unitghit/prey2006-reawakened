#ifndef __SCOPED_CRITICAL_SECTION_H__
#define __SCOPED_CRITICAL_SECTION_H__

// Release the lock when a recoverable engine error unwinds the caller.
class idScopedCriticalSection {
public:
	explicit idScopedCriticalSection( int index = CRITICAL_SECTION_ZERO ) : index( index ) { Sys_EnterCriticalSection( index ); }
	~idScopedCriticalSection() { Sys_LeaveCriticalSection( index ); }
private:
	idScopedCriticalSection( const idScopedCriticalSection & );
	idScopedCriticalSection &operator=( const idScopedCriticalSection & );
	int index;
};

#endif
