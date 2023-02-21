#include "pch.h"
//#include "Multithreading.h"

#if defined	(QT_VERSION)
#include <QReadWriteLock>
#endif //(QT_VERSION)

#if defined ( WINDOWS )
#include <Windows.h>
#endif

#include <exception>

using namespace SyncTL;

//typedef unsigned int DWORD;

SyncTL::BasicThread::BasicThread(SyncTL::ThreadPriority priority):
	m_thread(NULL),
	m_priority(priority),
	m_exit_flag(false)
{}

SyncTL::BasicThread::~BasicThread()
{
	WaitForExit();
}

unsigned int /*error code*/ SyncTL::BasicThread::WaitForExit(unsigned int timeout_milliseconds,
													 unsigned int* platform_error)
{
	unsigned int ret_val = UNDEFINED_ERROR;
#ifdef WINDOWS
	unsigned int win_ret_val = WaitForSingleObject(m_thread, timeout_milliseconds);
	switch(win_ret_val)
	{
	case WAIT_OBJECT_0:
		ret_val = ERR_OK;
		break;
	case WAIT_ABANDONED:
		ret_val = THREADING_ERROR_ABANDONED;
		break;
	case WAIT_TIMEOUT:
		ret_val = THREADING_ERROR_TIMEOUT;
		break;
	case WAIT_FAILED:
		if(platform_error != NULL)
		{
			*platform_error = GetLastError();
		}
	}
#endif //WINDOWS
	return ret_val;
}

SyncTL::Thread::~Thread()
{}

unsigned int SyncTL::BasicThread::MapPriorityToPlatformPriority(ThreadPriority tp)
{
#ifdef WINDOWS
	switch (tp)
	{
	case PRIORITY_IDLE: return THREAD_PRIORITY_IDLE;
	case PRIORITY_LOWEST: return THREAD_PRIORITY_LOWEST;
	case PRIORITY_LOW: return THREAD_PRIORITY_BELOW_NORMAL;
	case PRIORITY_NORMAL: return THREAD_PRIORITY_NORMAL;
	case PRIORITY_HIGH: return THREAD_PRIORITY_ABOVE_NORMAL;
	case PRIORITY_HIGHEST: return THREAD_PRIORITY_HIGHEST;
	case PRIORITY_TIME_CRITICAL: return THREAD_PRIORITY_TIME_CRITICAL;
	default: ASSERT(false); return THREAD_PRIORITY_NORMAL;
	}
#endif //WINDOWS
}

unsigned int /*error code*/ SyncTL::Thread::Run(unsigned int* platform_error)
{
	unsigned int ret_val = UNDEFINED_ERROR;
	if (m_thread == NULL)
	{
#ifdef WINDOWS
		m_thread = CreateThread(NULL,
			0,
			ThreadProc,
			this,
			CREATE_SUSPENDED,
			NULL);
#endif //WINDOWS
		if (m_thread == NULL)
		{
			ret_val = THREADING_ERROR_CANNOT_CREATE_THREAD;
			return ret_val;
		}
	}
#ifdef WINDOWS
	unsigned int priority = MapPriorityToPlatformPriority(m_priority);
	BOOL ok = SetThreadPriority(m_thread, priority);
	if(ok == FALSE)
	{
		if(platform_error != NULL)
		{
			*platform_error = GetLastError();
		}
		ret_val = THREADING_ERROR_CANNOT_START_THREAD;
		return ret_val;
	}
	unsigned int tmp_platform_error = ResumeThread(m_thread);
	if(tmp_platform_error == -1)
	{
		if(platform_error != NULL)
		{
			*platform_error = GetLastError();
		}
		ret_val = THREADING_ERROR_CANNOT_START_THREAD;
	} else {
		ret_val = ERR_OK;
	}
#endif //WINDOWS
	return ret_val;
}

unsigned int /*error code*/ SyncTL::Thread::Stop(unsigned int* platform_error)
{
	if (m_thread == NULL)
	{
		return THREADING_ERROR_CANNOT_STOP_NOT_RUNNING;
	}
	m_exit_flag = true;
	return WaitForExit();
}

DWORD WINAPI SyncTL::Thread::ThreadProc(LPVOID _this)
{
	ASSERT(_this != NULL);
	Thread* t = (Thread*)(_this);
	return t->ThreadProc();
}

void SyncTL::MainThread::Deinit()
{
	if(m_this_singleton != NULL)
	{
		delete m_this_singleton;
	}
}

MainThread* SyncTL::MainThread::GetMainThread()
{
	if(m_this_singleton == NULL)
	{
		Throw<Exception>(THREADING_ERROR_MAIN_THREAD_NOT_INITIALIZED,
			L"Main thread not initialized",
			EXC_HERE);
	}
	return m_this_singleton;
}

unsigned int /*error code*/ SyncTL::MainThread::PostMessage(ThreadMessage* message)
{
	ASSERT(message != NULL);
	/*if(m_messages.LockForWrite() == false)
	{
		throw Exception(THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
			L"Cannot get lock for write to message queue of main thread",
			EXC_HERE);
	}*/
	TemplateWriteSynchronizer<BasicList> sync(&m_messages);
	m_messages.PushBack(message);
	//m_messages.Unlock();
	return ERR_OK;
}

unsigned int /*error code*/ SyncTL::MainThread::ProcessMessages(unsigned int count)
{
	unsigned int ret_val = UNDEFINED_ERROR;
	bool exit_flag = false;
	while(exit_flag == false)
	{
		ThreadMessage* msg = NULL;
		if(m_messages.LockForWrite() == false)
		{
			Throw<Exception>(THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot get lock for write to messages list on main thread",
				EXC_HERE);
		}
		//TemplateWriteSynchronizer<BasicList> sync(&m_messages);
		BasicList::Entry* e = m_messages.PopFront();
		if(e != NULL)
		{
			msg = dynamic_cast<ThreadMessage*>(e);
			ASSERT(msg != NULL);	//posting to m_messages anything but ThreadMessage-s is an error.
			unsigned int msg_count = m_messages.GetCount();
			m_messages.Unlock();
			ThreadMessage* tm = dynamic_cast<ThreadMessage*>(msg);
			if(tm != NULL)
			{
				unsigned int msg_ret_val = OnMessage(tm);
				if(msg_ret_val != ERR_OK)
				{
					exit_flag = true;
					ret_val = msg_ret_val;
				}
			} else {
				OutputDebugMsg(L"unrecognized type of message detected on main thread message queue\n");
			}
			--count;
			if(count == 0)
			{
				exit_flag = true;
				ret_val = ERR_OK;
			}
			if(msg_count == 0)
			{
				exit_flag = true;
				ret_val = ERR_OK;
			}
			delete msg;
		} else {
			exit_flag = true;
			ret_val = ERR_OK;
		}
		m_messages.Unlock();
	}
	return ret_val;
}

void SyncTL::MainThread::TimerEvent::OnTimer()
{
	ASSERT(m_main_thread != NULL);
	m_main_thread->ProcessMessages();
}

SyncTL::MainThread::MainThread(Thread_Handle th):
	m_timer_event(this)
{
	ASSERT(th != NULL);
	m_thread = th;
	m_timer.SetEvent(&m_timer_event);
	m_timer.SetTimeout(PROCESS_MESSAGES_TIMEOUT, false);
	m_timer.Start();
}

SyncTL::MainThread::~MainThread()
{
	m_timer.Stop();
	m_timer.SetEvent(NULL);
}

SyncTL::MainThread* SyncTL::MainThread::m_this_singleton = NULL;

SyncTL::WorkerThread::WorkerThread(SyncTL::ThreadPriority priority):
	SyncTL::Thread(priority),
	//m_worker_list_lock(&m_worker_list),
	//m_message_list_lock(&m_message_list),
	m_exit_flag(false)
{
	m_worker_list.SetLock(&m_worker_list_lock);
	m_message_list.SetLock(&m_message_list_lock);
}

SyncTL::WorkerThread::~WorkerThread()
{
	m_message_list.LockForWrite();
	while(m_message_list.IsEmpty() == false)
	{
		MessageList::Entry* entry = m_message_list.PopFront();
		ASSERT(entry != NULL);
		delete entry;
	}
	m_message_list.Unlock();
	if(m_message_list_lock.TryLockForWrite() == false)
	{
		m_message_list.Unlock();
	}
	m_message_list.SetLock(NULL);	//because lock will be destroyed before m_message_list and m_message_list destuctor will fail
	if(m_worker_list_lock.TryLockForWrite() == false)
	{
		m_worker_list.Unlock();
	}
	m_worker_list.SetLock(NULL);
	Stop();
	WaitForExit(INFINITE);
	//worker deletion is up to those who added them here
}

unsigned int /*error code*/ SyncTL::WorkerThread::Stop(unsigned int* platform_error)
{
	m_message_list_not_empty.SetEvent();
	return Thread::Stop(platform_error);
}

unsigned int /*error code*/ SyncTL::WorkerThread::AddWorker(SyncTL::Worker* worker)
{
	ASSERT(worker != NULL);
	ASSERT(worker->m_thread == NULL);
	ASSERT(worker->m_at_work == false);
	unsigned int ret_val = UNDEFINED_ERROR;
	/*if (m_worker_list.LockForWrite() == false)
	{
		return THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
	}*/
	TemplateWriteSynchronizer<BasicList> sync(&m_worker_list);
	m_worker_list.PushFront(worker);
	worker->m_thread = this;
	//m_worker_list.Unlock();
	ret_val = ERR_OK;
	return ret_val;
}

unsigned int /*error code*/ SyncTL::WorkerThread::RemoveWorker(WorkerIterator it)
{
	ASSERT(it.IsValid());
	Worker* worker = dynamic_cast<Worker*>(it.operator BasicList::Entry *());
	ASSERT(worker->m_thread != NULL);
	ASSERT(worker->m_at_work == false);
	unsigned int ret_val = UNDEFINED_ERROR;
	/*if (m_worker_list.LockForWrite() == false)
	{
		return THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
	}*/
	TemplateWriteSynchronizer<BasicList> sync(&m_worker_list);
	m_worker_list.Remove(it);
	worker->m_thread = NULL;
	//m_worker_list.Unlock();
	ret_val = ERR_OK;
	return ret_val;
}

unsigned int /*error code*/ SyncTL::WorkerThread::RemoveWorker(SyncTL::Worker* worker)
{
	ASSERT(worker != NULL);
	ASSERT(worker->m_thread != NULL);
	ASSERT(worker->m_at_work == false);
	unsigned int ret_val = UNDEFINED_ERROR;
	/*if (m_worker_list.LockForWrite() == false)
	{
		return THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
	}*/
	TemplateWriteSynchronizer<BasicList> sync(&m_worker_list);
	m_worker_list.Remove(worker);
	worker->m_thread = NULL;
	//m_worker_list.Unlock();
	ret_val = ERR_OK;
	return ret_val;
}

unsigned int /*error code*/ SyncTL::WorkerThread::PostMessageToWorker(SyncTL::WorkerMessage* wm)
{
	ASSERT(wm != NULL);
	/*bool ok = m_message_list.LockForWrite();
	if(false)
	{
		throw Exception(THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
			L"Cannot get lock for write to post to the main thread message list",
			EXC_HERE);
	}*/
	TemplateWriteSynchronizer<BasicList> sync(&m_message_list);
	m_message_list.PushBack(wm);
	m_message_list_not_empty.SetEvent();
	//m_message_list.Unlock();
	return ERR_OK;
}

bool SyncTL::WorkerThread::IsWorkerAdded(SyncTL::Worker* worker, WorkerIterator* out_it) const
{
	bool ret_val (false);
	WorkerThread* nonconst_this = ((WorkerThread*)this);
	/*bool ok = nonconst_this->m_message_list.LockForRead();
	if(false)
	{
		throw Exception(THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
			L"Cannot get lock for read to find worker on worker thread",
			EXC_HERE);
	}*/
	TemplateReadSynchronizer<MessageList> sync(&(nonconst_this->m_worker_list));
	for(WorkerIterator it = nonconst_this->m_worker_list.Begin(); it.IsValid(); ++ it)
	{
		Worker* w = GetWorker(it);
		ASSERT(w != NULL);
		if(w == worker)
		{
			ret_val = true;
			if(out_it != NULL)
			{
				*out_it = it;
			}
			break;
		}
	}
	//nonconst_this->m_message_list.Unlock();
	return ret_val;
}

unsigned int /*error code*/ SyncTL::WorkerThread::ThreadProc()
{
	unsigned int ret_val = UNDEFINED_ERROR;
	bool wait_for_messages(false);
	while(m_exit_flag == false)
	{
		//get message
		WorkerMessage* wm = NULL;
		bool ok = m_message_list_lock.LockForWrite();
		bool locked = false;
		if(ok == false)
		{
			Exception* exc = new Exception(THREADING_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot get lock for write to worker thread message list",
				EXC_HERE);
			PostExceptionToMainThread(exc);
		}
		locked = true;
		//wm = dynamic_cast<WorkerMessage*>(m_message_list.PopFront());
		BasicList::Entry* ble = m_message_list.PopFront();
		if(ble != NULL)
		{
			wm = dynamic_cast<WorkerMessage*>(ble);
		}
		if(wm != NULL)
		{
			Worker* worker = wm->m_dest_worker;
			ASSERT(worker != NULL);
			ASSERT(worker->m_thread == this);
			//send it to appropriate worker
			unsigned int worker_ret_val = UNDEFINED_ERROR;
			try
			{
				worker_ret_val = worker->InternalExecute(wm);
				ExitMessage* em = dynamic_cast<ExitMessage*>(wm);
				if(em != NULL)
				{
					m_exit_flag = true;
				}
			} CATCH(Exception, exc)//catch (Exception exc)
			{
				PostExceptionToMainThread(new Exception(*exc));
				m_exit_flag = true;
			}
			catch (std::exception& exc)
			{
				wchar_t msg[0x7f];
				memset(msg, 0, sizeof(msg));
				mbstowcs(msg, exc.what(), strlen(exc.what()));
				PostExceptionToMainThread(new Exception(UTILS_ERROR_STD_EXCEPTION,
					msg,
					EXC_HERE));
			}
			catch ( ... )
			{
				PostExceptionToMainThread(new Exception(UNDEFINED_ERROR,
					L"Unknown error on worker thread",
					EXC_HERE));
				m_exit_flag = true;
			}
			if(wm->m_delete_after_completion)
			{
				delete wm;
			}
			if(worker_ret_val != ERR_OK)
			{
				if(worker_ret_val == THREADING_ERROR_OPERATION_CANCELLED)
				{
					ret_val = ERR_OK;
				}/* else if (worker_ret_val == CH_ERR_NOWHERE_TO_ADVANCE)
				{
				} */else {
					PostExceptionToMainThread(new Exception(worker_ret_val,
						L"Error on worker thread",
						EXC_HERE));
					ret_val = worker_ret_val;
				}
				m_exit_flag = true;	//it is assumed that worker will report errors in some other way.
			}
		} else {
			//sleep until messages will come.
			wait_for_messages = true;
			m_message_list_not_empty.ClearEvent();
		}
		if(locked)
		{
			m_message_list_lock.Unlock();
		}
		if(wait_for_messages)
		{
			m_message_list_not_empty.Wait(INFINITE);
		}
	}
	return ret_val;
}

MainThread* SyncTL::GetMainThread()
{
	static MainThread* main_thread(MainThread::GetMainThread());
	return main_thread;
}

void SyncTL::PostExceptionToMainThread(Exception* exc)
{
	ThreadException* th_exc = new ThreadException(*exc);
	ASSERT(th_exc != NULL);
	PostMessageToMainThread(th_exc);
}

void SyncTL::PostMessageToMainThread(ThreadMessage* msg /*msg will be deleted on main thread*/)
{
	ASSERT(msg != NULL);
	//GetMainThreadMessageList()->PushBack(msg);
	GetMainThread()->PostMessage(msg);
}