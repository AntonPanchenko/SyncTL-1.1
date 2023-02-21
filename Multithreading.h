#ifndef MULTITHREADING_H_INCLUDED
#define MULTITHREADING_H_INCLUDED

#if ((defined _WIN64) || (defined _WIN32))
#define WINDOWS
#endif	//WIN32 || WIN64

#ifdef WINDOWS
#include <Windows.h>
#endif //WIN32

#ifdef WINDOWS
typedef 	HANDLE Thread_Handle;  //exctly like that because ThreadHandle already exists somewhere
#else
typedef 	unsigned int ThreadHandle;
#endif	//WIDNOWS

#define WAIT_INFINITE	-1

#include "Utils.h"
#include "ErrorReporting.h"
#include "Collections.h"
#include "Synchronization.h"
#include "Timer.h"
#include <climits>

#if defined ( _WIN64 ) || defined ( _WIN32 )
#define WINDOWS
#endif

namespace SyncTL
{

enum
{
	THREADING_ERROR_CANNOT_CREATE_THREAD = THREADING_ERROR_BASE,
	THREADING_ERROR_CANNOT_START_THREAD,
	THREADING_ERROR_CANNOT_DESTROY_THREAD,
	THREADING_ERROR_CANNOT_STOP_NOT_RUNNING,
	THREADING_ERROR_ABANDONED,
	THREADING_ERROR_TIMEOUT,
	THREADING_ERROR_CANNOT_GET_LOCK_FOR_READ,
	THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
	THREADING_ERROR_OPERATION_CANCELLED,
	THREADING_ERROR_MAIN_THREAD_NOT_INITIALIZED,
	THREADING_ERROR_UNSUPPORTED_TYPE_OF_MESSAGE
};

enum ThreadPriority
{
	PRIORITY_IDLE,
	PRIORITY_LOWEST,
	PRIORITY_LOW,
	PRIORITY_NORMAL,
	PRIORITY_HIGH,
	PRIORITY_HIGHEST,
	PRIORITY_TIME_CRITICAL,
	PRIORITY_UNDEFINED
};

typedef BasicList::Entry  ThreadEntry;

class ThreadMessage: public ThreadEntry
{};

class ThreadException: public ThreadMessage, public Exception
{
public:
	ThreadException(const Exception& exc):
		Exception(exc)
	{
		size_t length = wcslen(exc.GetErrorMessage());
		//wchar_t* end = 
		wcscpy_s(m_message_buffer, length, exc.GetErrorMessage());
		//*end = wchar_t(0);
		m_message = m_message_buffer;
		length = strlen(exc.GetFilename());
		//char* fn_end = 
		strcpy_s(m_filename_buffer, length, exc.GetFilename());
		//*fn_end = char(0);
		m_filename = m_filename_buffer;
	}
protected:
	wchar_t m_message_buffer[ERROR_MSG_BUFFER_LENGTH];
	char m_filename_buffer[ERROR_MSG_BUFFER_LENGTH];
};

class UpdateMessage: public ThreadMessage
{};

class BasicThread
{
public:
	BasicThread(ThreadPriority priority = PRIORITY_NORMAL);
	virtual ~BasicThread();
	unsigned int /*error code*/ WaitForExit(unsigned int timeout_milliseconds = WAIT_INFINITE,
											unsigned int* platform_error = NULL);
protected:
	static unsigned int MapPriorityToPlatformPriority(ThreadPriority tp);

	Thread_Handle m_thread;
	ThreadPriority m_priority;
	bool m_exit_flag;
};

class Thread: public BasicThread
{
public:
	Thread(ThreadPriority priority = PRIORITY_NORMAL):
		BasicThread(priority)
	{}
	virtual ~Thread();
	unsigned int /*error code*/ Run(unsigned int* platform_error = NULL);
	unsigned int /*error code*/ Stop(unsigned int* platform_error = NULL);
protected:
	virtual unsigned int /*error code*/ ThreadProc() = 0;
	
#ifdef WINDOWS
	static DWORD WINAPI ThreadProc(LPVOID _this);
#endif //WINDOWS
	//else some other kind of thread proc
};

//so it do not mess with WinAPI PostMesage
#undef PostMessage
//this is just a wrapper, it does not have a thread proc because main thread is already running when some instance
//of this object is created. however it is convenient to use at as an interface to main thread to send messages there
//and they will be processed as long as main thread processes the message queue and responds to the timer.
//Main thread is a singletone.
class MainThread: public BasicThread
{
public:
	static const unsigned int ALL_MESSAGES = INT_MAX;
	static const unsigned int PROCESS_MESSAGES_TIMEOUT = 500;	//milliseconds
	//call this function only once and only on main thread.
	//MainThreadSubclass must have a constructor with ThreadHandle parameter.
	template <class MainThreadSubclass>
	static MainThreadSubclass* Init()
	{
		Thread_Handle mth(NULL);
#ifdef WINDOWS
		mth = ::GetCurrentThread();
//#else
#endif
		MainThreadSubclass* ret_val = new MainThreadSubclass(mth);
		m_this_singleton = ret_val;
		return ret_val;
	}
	static void Deinit();
	static MainThread* GetMainThread();
	unsigned int /*error code*/ PostMessage(ThreadMessage* message);
	//when OnMessage returns anything but ERR_OK, ProcessMessages returns with that error code.
	//pending messages remain in the messages list until process messages will be called again.
	virtual unsigned int /*error code*/ OnMessage(ThreadMessage* tm) = 0;
protected:
	class TimerEvent: public Timer::Event
	{
	public:
		TimerEvent(MainThread* mt):
			m_main_thread(mt)
			{ ASSERT(m_main_thread != NULL); }
		void OnTimer();
	protected:
		MainThread* m_main_thread;
	};

	MainThread(Thread_Handle th);
	virtual ~MainThread();
	//this method is to be called periodically in order to process incoming messages.
	//whenever OnMessage returns anything nut ERR_OK, ProcessMessages returns with that error.
	unsigned int /*error code*/ ProcessMessages(unsigned int count = ALL_MESSAGES);

	Timer m_timer;
	TimerEvent m_timer_event;
	//this is separate message queue besides of those provided by the target OS.
	//another threads can post their messages here.
	BasicList m_messages;
	static MainThread* m_this_singleton;
};

class WorkerThread;
class Worker;

class WorkerMessage: public BasicList::Entry
{
	friend class WorkerThread;
public:
	WorkerMessage(Worker* dest, bool delete_after_completion = true):
		m_dest_worker(dest),
		m_ret_val(UNDEFINED_ERROR),
		m_delete_after_completion(delete_after_completion)
	{
		ASSERT(m_dest_worker != NULL);
	}
	virtual ~WorkerMessage()
	{};
	unsigned int GetRetVal() const
		{ return m_ret_val;	}
	void SetRetVal(unsigned int ret_val)
		{ m_ret_val = ret_val; }
protected:
	Worker* m_dest_worker;
	unsigned int m_ret_val;
	bool m_delete_after_completion;
};

class ExitMessage: public WorkerMessage
{
public:
	ExitMessage(Worker* worker, bool delete_after_completion):
		WorkerMessage(worker, delete_after_completion)
	{}
};

class Worker: public BasicList::Entry
{
	friend class WorkerThread;
public:
	Worker() :
		m_thread(NULL),
		m_at_work(false)
		{}
	virtual ~Worker()
		{ ASSERT(m_at_work == false); }
	virtual unsigned int /*error code*/ Execute(WorkerMessage* wm) = 0;
protected:
	//this method is to be called from worker. it calls Execute. reimplement Execute.
	inline unsigned int InternalExecute(WorkerMessage* wm)
	{
		ASSERT(m_at_work == false);
		m_at_work = true;
		unsigned int ret_val = Execute(wm);
		m_at_work = false;
		return ret_val;
	}
	WorkerThread* m_thread;
	bool m_at_work;
};

//typedef List<WorkerMessage> MessageList;
//because messages will be of different types
typedef BasicList MessageList;

/*this class throws exceptions instead of returning error codes because it is crossplatform and there
are many error codes. I better put system error code and message in exception instead of mapping system error codes
to My own error codes. class Exception fits pretty well for this.
when message is posted, worker thread is responsible for it's deletion.*/
class WorkerThread: public Thread
{
public:
	typedef BasicList::Iterator WorkerIterator;
	WorkerThread(ThreadPriority priority);
	virtual ~WorkerThread();
	unsigned int /*error code*/ Stop(unsigned int* platform_error = NULL);
	unsigned int /*error code*/ AddWorker(Worker* worker);
	unsigned int /*error code*/ RemoveWorker(WorkerIterator it);
	unsigned int /*error code*/ RemoveWorker(Worker* worker);
	unsigned int /*error code*/ PostMessageToWorker(WorkerMessage* wm);
	bool IsWorkerAdded(Worker* worker, WorkerIterator* out_it = NULL) const;
protected:
	unsigned int /*error code*/ ThreadProc();
	inline Worker* GetWorker(WorkerIterator& it) const
		{ return dynamic_cast<Worker*>(it.operator BasicList::Entry *()); }
	BasicList m_worker_list;
	ReadWriteLock m_worker_list_lock;
	MessageList m_message_list;
	ReadWriteLock m_message_list_lock;
	bool m_exit_flag;
	Event m_message_list_not_empty;
};

/*messages (and exceptions) will be deleted on main thread*/
MainThread* GetMainThread();
void PostExceptionToMainThread(Exception* exc);
void PostMessageToMainThread(ThreadMessage* msg);

} //end namespace SyncTL

#endif //MULTITHREADING_H_INCLUDED