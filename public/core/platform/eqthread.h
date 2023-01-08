//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Engine threads
//////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifdef PLAT_POSIX

#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <stdint.h>

#endif // PLAT_POSIX

namespace Threading
{

enum ThreadPriority_e
{
	TP_LOWEST = 0,
	TP_BELOW_NORMAL,
	TP_NORMAL,
	TP_ABOVE_NORMAL,
	TP_HIGHEST
};

typedef unsigned int (*threadfunc_t)(void *);

#ifdef _WIN32
typedef HANDLE				SignalHandle_t;
typedef CRITICAL_SECTION	MutexHandle_t;
#else
struct SignalHandle_t
{
	pthread_cond_t 	cond;
	pthread_mutex_t mutex;
	int 	waiting; // number of threads waiting for a signal
	bool 	manualReset;
	bool 	signaled; // is it signaled right now?
};
typedef pthread_mutex_t		MutexHandle_t;
#endif // _WIN32

static constexpr const int WAIT_INFINITE = -1;
static constexpr const int DEFAULT_THREAD_STACK_SIZE = 256 * 1024;

#ifdef Yield
#	undef Yield
#endif // Yield

//
// Thread creation/destroying and other operations
//

uintptr_t ThreadCreate( threadfunc_t fnThread, void* pThreadParams, ThreadPriority_e nPriority,
								  const char* pszThreadName, int nStackSize = DEFAULT_THREAD_STACK_SIZE, bool bSuspended = false );

void ThreadDestroy( uintptr_t threadHandle );
void ThreadWait( uintptr_t threadHandle );
void Yield();
				   
uintptr_t GetCurrentThreadID();
void SetCurrentThreadName(const char* name);

uintptr_t ThreadGetID(uintptr_t handle);
void SetThreadName(uintptr_t threadId, const char* name);
void GetThreadName(uintptr_t threadId, char* name, int maxLength);

//
// Signal creation/destroying and usage
//

void SignalCreate( SignalHandle_t& handle, bool bManualReset );
void SignalDestroy( SignalHandle_t& handle );
void SignalRaise( SignalHandle_t& handle );
void SignalClear( SignalHandle_t& handle );
bool SignalWait( SignalHandle_t& handle, int nTimeout = WAIT_INFINITE);

//
// Mutex creation/destroying and locking/unlocking
//

void MutexCreate( MutexHandle_t& handle );
void MutexDestroy( MutexHandle_t& handle );
bool MutexLock( MutexHandle_t& handle, bool bBlocking );
void MutexUnlock( MutexHandle_t& handle );

//
// Atomic pointers and integers
//

int IncrementInterlocked(int& value );
int DecrementInterlocked(int& value );
int AddInterlocked(int& value, int i );
int SubtractInterlocked(int& value, int i );
int ExchangeInterlocked( int& value, int exchange );
int CompareExchangeInterlocked(int& value, int comparand, int exchange );

// helper classes as a C++

//----------------------------------------------------------------------------------------
//	CEqMutex provides a C++ wrapper to the low level system mutex functions.  A mutex is an
//	object that can only be locked by one thread at a time.  It's used to prevent two threads
//	from accessing the same piece of data simultaneously.
//----------------------------------------------------------------------------------------
class CEqMutex
{
public:
					CEqMutex()						{ MutexCreate( m_nHandle ); }
					~CEqMutex()						{ MutexDestroy( m_nHandle ); }

	bool			Lock( bool blocking = true )	{ return MutexLock( m_nHandle, blocking ); }
	void			Unlock()						{ MutexUnlock( m_nHandle ); }

private:
	MutexHandle_t	m_nHandle;

					CEqMutex( const CEqMutex& s ) {}
	void			operator=( const CEqMutex& s ) {}
};

//----------------------------------------------------------------------------------------
//	CScopedMutex is a helper class that automagically locks a mutex when it's created
//	and unlocks it when it goes out of scope.
//----------------------------------------------------------------------------------------
class CScopedMutex
{
public:
	CScopedMutex( CEqMutex &m, bool blocking = true ) : m_pMutex(&m) 
	{ m_pMutex->Lock(blocking); }

	~CScopedMutex()								
	{ m_pMutex->Unlock(); }

private:
	CEqMutex*	m_pMutex;	// NOTE: making this a reference causes a TypeInfo crash
};

//----------------------------------------------------------------------------------------
//	CEqSignal is a C++ wrapper for the low level system signal functions.  A signal is an object
//	that a thread can wait on for it to be raised.  It's used to indicate data is available or that
//	a thread has reached a specific point.
//----------------------------------------------------------------------------------------

class CEqSignal
{
public:
	static const int	WAIT_INFINITE = -1;

			CEqSignal( bool manualReset = false )	{ SignalCreate( m_nHandle, manualReset ); }
			~CEqSignal()							{ SignalDestroy( m_nHandle ); }

	void	Raise() { SignalRaise( m_nHandle ); }
	void	Clear() { SignalClear( m_nHandle ); }

	// Wait returns true if the object is in a signalled state and
	// returns false if the wait timed out. Wait also clears the signalled
	// state when the signalled state is reached within the time out period.
	bool	Wait( int timeout = WAIT_INFINITE ) { return SignalWait( m_nHandle, timeout ); }

private:
	SignalHandle_t		m_nHandle;

						CEqSignal( const CEqSignal & s ) {}
	void				operator=( const CEqSignal & s ) {}
};

//----------------------------------------------------------------------------------------
//	CEqInterlockedInteger is a C++ wrapper for the low level system interlocked integer
//	routines to atomically increment or decrement an integer.
//----------------------------------------------------------------------------------------
class CEqInterlockedInteger
{
public:
						CEqInterlockedInteger() : m_nValue( 0 ) {}

	// atomically increments the integer and returns the new value
	int					Increment() { return IncrementInterlocked( m_nValue ); }

	// atomically decrements the integer and returns the new value
	int					Decrement() { return DecrementInterlocked( m_nValue ); }

	// atomically adds a value to the integer and returns the new value
	int					Add( int v ) { return AddInterlocked( m_nValue, v); }

	// atomically subtracts a value from the integer and returns the new value
	int					Sub( int v ) { return SubtractInterlocked( m_nValue, v); }

	// returns the current value of the integer
	int					GetValue() const { return m_nValue; }

	// sets a new value, Note: this operation is not atomic
	void				SetValue( int v ) { m_nValue = v; }

private:
	int					m_nValue;
};

/*----------------------------------------------------------------------------------------
CEqThread is an abstract base class, to be extended by classes implementing the
CEqThread::Run() method.

	class CMyThread : public CEqThread {
	public:
		virtual int Run()
		{
			// run thread code here
			return 0;
		}
		// specify thread data here
	};

	CMyThread thread;
	thread.Start( "myThread" );

A worker thread is a thread that waits in place (without consuming CPU)
until work is available. A worker thread is implemented as normal, except that, instead of
calling the Start() method, the StartWorker() method is called to start the thread.
Note that the ThreadCreate function does not support the concept of worker threads.

	class CMyWorkerThread : public CMyThread
	{
	public:
		virtual int Run()
		{
			// run thread code here
			return 0;
		}
		// specify thread data here
	};

	CMyWorkerThread thread;
	thread.StartThread( "myWorkerThread" );

	// main thread loop
	for ( ; ; ) {
		// setup work for the thread here (by modifying class data on the thread)
		thread.SignalWork();           // kick in the worker thread
		// run other code in the main thread here (in parallel with the worker thread)
		thread.WaitForThread();        // wait for the worker thread to finish
		// use results from worker thread here
	}

In the above example, the thread does not continuously run in parallel with the main Thread,
but only for a certain period of time in a very controlled manner. Work is set up for the
Thread and then the thread is signalled to process that work while the main thread continues.
After doing other work, the main thread can wait for the worker thread to finish, if it has not
finished already. When the worker thread is done, the main thread can safely use the results
from the worker thread.

Note that worker threads are useful on all platforms but they do not map to the SPUs on the PS3.
------------------------------------------------------------------------------------------
*/

class CEqThread
{
public:
					CEqThread();
	virtual			~CEqThread();

	const char *	GetName()				const { return m_szName.GetData(); }
	uintptr_t		GetThreadHandle()		const { return m_nThreadHandle; }
	uintptr_t		GetThreadID()			const { return ThreadGetID(m_nThreadHandle); }
	bool			IsRunning()				const { return m_bIsRunning; }
	bool			IsTerminating()			const { return m_bIsTerminating; }

	// Thread Start/Stop/Wait
	bool			StartThread( const char* name,
								 ThreadPriority_e priority = TP_NORMAL,
								 int stackSize = DEFAULT_THREAD_STACK_SIZE );

	bool			StartWorkerThread( const char* name,
									   ThreadPriority_e priority = TP_NORMAL,
									   int stackSize = DEFAULT_THREAD_STACK_SIZE );

	void			StopThread( bool wait = true );

	// This can be called from multiple other threads. However, in the case
	// of a worker thread, the work being "done" has little meaning if other
	// threads are continuously signalling more work.
	void			WaitForThread();

	//------------------------
	// Worker Thread
	//------------------------

	// Signals the thread to notify work is available.
	// This can be called from multiple other threads.
	void			SignalWork();

	// Returns true if the work is done without waiting.
	// This can be called from multiple other threads. However, the work
	// being "done" has little meaning if other threads are continuously
	// signalling more work.
	bool			IsWorkDone();

protected:
	// The routine that performs the work.
	virtual int		Run();

private:
	EqString		m_szName;
	uintptr_t		m_nThreadHandle;

	bool			m_bIsWorker;
	bool			m_bIsRunning;

	volatile bool	m_bIsTerminating;
	volatile bool	m_bMoreWorkToDo;

	CEqSignal		m_SignalWorkerDone;
	CEqSignal		m_SignalMoreWorkToDo;
	CEqMutex		m_SignalMutex;

	static int		ThreadProc( CEqThread* thread );

	// you can't do any copy and assignment operations
					CEqThread( const CEqThread& s ) {}
	void			operator = ( const CEqThread& s ) {}
};

class CEqException
{
	static const int ERROR_BUFFER_LENGTH = 2048;
public:
	CEqException(const char* text = "");
	const char* GetErrorString() const { return s_szError; }

protected:
	int	GetErrorBufferSize() const { return ERROR_BUFFER_LENGTH; }

private:
	char s_szError[ERROR_BUFFER_LENGTH];
};

};
