#include "pch.h"
//#include "Timer.h"

#pragma warning (disable: 4100)

using namespace SyncTL;

ReadWriteLock* Timer::m_timer_vector_lock = NULL;
ReadWriteLock *init_deinit_lock;
Timer::TimerVector* Timer::m_timer_vector = NULL;
//QtReadWriteLock* Timer::m_timer_vector_lock = NULL;
//QtReadWriteLock init_deinit_lock;


class Initializer
{
public:
	Initializer();
	virtual ~Initializer();
};

Initializer::Initializer()
{
	Timer::StaticInit();
}

Initializer::~Initializer()
{
	Timer::StaticDeinit();
}

Initializer initializer;

unsigned int /*error code*/ Timer::StaticInit()
{
	//bool ok = init_deinit_lock.LockForWrite();
	//SyncTL::LockGuard<true> guard(&init_deinit_lock);
	init_deinit_lock = new ReadWriteLock();
	SyncTL::LockGuard<true> guard(init_deinit_lock);
	/*if (ok == false)
	{
		return SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
	}*/
	if((m_timer_vector_lock != NULL) || (m_timer_vector != NULL))
	{
		return TIMER_ERROR_STATIC_MEMBERS_NON_NULL;
	}
	m_timer_vector_lock = new ReadWriteLock();
	m_timer_vector = new TimerVector(TIMER_VECTOR_PREALLOC, BasicVector::GetDefaultAllocator(), m_timer_vector_lock);
	//init_deinit_lock.Unlock();
	return ERR_OK;
}

unsigned int /*error code*/ Timer::StaticDeinit()
{
	unsigned int ret_val = NO_ERROR;
	//SyncTL::LockGuard<true> guard(&init_deinit_lock);
	{
		SyncTL::LockGuard<true> guard(init_deinit_lock);
		/*bool ok = init_deinit_lock.LockForWrite();
		if(ok == false)
		{
			return SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
		}*/
		if (m_timer_vector != NULL)
		{
			SyncTL::LockGuard<true> guard(m_timer_vector->GetLock());
			/*ok = m_timer_vector->LockForWrite();
			if(ok)
			{*/
			if (m_timer_vector->GetCount() == 0)
			{
				m_timer_vector->Unlock();
				m_timer_vector->SetLock(nullptr);
				delete m_timer_vector;
				m_timer_vector = NULL;
				delete m_timer_vector_lock; //this was that lock that 
				m_timer_vector_lock = NULL;
				guard.Detach();
				ret_val = ERR_OK;
			}
			else {
				ret_val = TIMER_ERROR_TIMER_VECTOR_NOT_EMPTY;
				//m_timer_vector->Unlock();
			}
			/* } else {
				ret_val = TIMER_ERROR_CANNOT_LOCK_TIMER_VECTOR;
			}*/
		}
		//init_deinit_lock.Unlock();
		{
			//guard.Detach();
		}
	}
	delete init_deinit_lock;
	return ret_val;
}

Timer::Timer():
	m_timer_id(NULL),
	m_event(NULL),
	m_timeout_milliseconds(0),
	m_is_once(false)
{
}

Timer::~Timer()
{
	//if last timer, destroy timer vector and timer vector lock
	if(m_timer_id != NULL)
	{
#ifdef WINDOWS
		KillTimer(NULL, m_timer_id);
#endif //WINDOWS
		if(m_timer_vector != NULL)
		{
			RemoveFromTimerVector(m_timer_id);
		}
	}
}

unsigned int /*error code*/ Timer::SetTimeout(unsigned int timeout_milliseconds, unsigned int is_once)
{
	unsigned int ret_val = UNDEFINED_ERROR;
	m_timeout_milliseconds = timeout_milliseconds;
	m_is_once = is_once;
	if(m_timer_id != NULL)
	{
		ret_val = Stop();
		if(ret_val == ERR_OK)
		{
			ret_val = Start();
		}
	} else {
		ret_val = ERR_OK;
	}
	return ret_val;
}

unsigned int /*error code*/ Timer::Start()
{
	if(m_timer_id != NULL)
	{
		return TIMER_ERROR_CANNOT_CREATE_TIMER;
	}
#ifdef WINDOWS
	m_timer_id = SetTimer(NULL, 0, m_timeout_milliseconds, TimerProcedure);
	if(m_timer_id == NULL)
	{
		return TIMER_ERROR_CANNOT_CREATE_TIMER;
	}
#endif //WINDOWS
	return AddToTimerVector(this, m_timer_id);
}

unsigned int /*error code*/ Timer::Stop()
{
	if(m_timer_id == NULL)
	{
		return TIMER_ERROR_CANNOT_DESTROY_TIMER;
	}
#ifdef WINDOWS
	bool ok = KillTimer(NULL, m_timer_id);
	if(ok == false)
	{
		TIMER_ERROR_CANNOT_DESTROY_TIMER;
	}
#endif //WINDOWS
	unsigned int ret_val = RemoveFromTimerVector(m_timer_id);
	m_timer_id = NULL;
	return ret_val;
}

#ifdef WINDOWS
VOID CALLBACK Timer::TimerProcedure(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time)
{
	Timer* _this = FindInTimerVector(timer_id);
	if(_this != NULL)
	{
		if(_this->m_event != NULL)
		{
			_this->m_event->OnTimer();
		}
		if(_this->m_is_once)
		{
			_this->Stop();
		}
	}
}
#endif //WINDOWS

unsigned int /*error code*/ Timer::AddToTimerVector(Timer* timer, Timer::TimerID timer_id)
{
	ASSERT(timer != NULL);
	ASSERT(timer_id != NULL);
	unsigned int ret_val = UNDEFINED_ERROR;
	//bool ok = m_timer_vector->LockForWrite();
	SyncTL::LockGuard<true> guard(m_timer_vector->GetLock());
	/*if (ok)
	{*/
		TimerEntry te;
		te.m_timer = timer;
		te.m_timer_id = timer_id;
		m_timer_vector->PushBack(te);
		//m_timer_vector->Unlock();
		ret_val = ERR_OK;
	//}
	return ret_val;
}

unsigned int /*error code*/ Timer::RemoveFromTimerVector(Timer::TimerID timer_id)
{
	ASSERT(timer_id != NULL);
	unsigned int ret_val = UNDEFINED_ERROR;
	SyncTL::LockGuard<true> guard(m_timer_vector->GetLock());
	/*bool ok = m_timer_vector->LockForWrite();
	if(ok)
	{*/
		bool found = false;
		for(TimerVector::Iterator it = m_timer_vector->Begin(); it.IsValid(); ++ it)
		{
			TimerEntry* te = it.operator Timer::TimerEntry *();
			ASSERT(te != NULL);
			if(te->m_timer_id == timer_id)
			{
				m_timer_vector->RemoveEntry(it);
				ret_val = ERR_OK;
				found = true;
				break;
			}
		}
		//m_timer_vector->Unlock();
		if(found == false)
		{
			ret_val = TIMER_ERROR_NOT_FOUND_IN_VECTOR;
		}
	//}
	return ret_val;
}

Timer* Timer::FindInTimerVector(Timer::TimerID timer_id)
{
	Timer* ret_val = NULL;
	SyncTL::LockGuard<true> guard(m_timer_vector->GetLock());
	/*bool ok = m_timer_vector->LockForWrite();
	if(ok)
	{*/
		for(TimerVector::Iterator it = m_timer_vector->Begin(); it.IsValid(); ++ it)
		{
			TimerEntry* te = it.operator Timer::TimerEntry *();
			ASSERT(te != NULL);
			if(te->m_timer_id = timer_id)
			{
				ret_val = te->m_timer;
				break;
			}
		}
		//m_timer_vector->Unlock();
	//}
	return ret_val;
}

Timeout::Timeout(unsigned int timeout):
	m_start_time(GetTickCount()),
	m_timeout(timeout)
{
}
bool Timeout::IsElapsed()
{
	unsigned int now = GetTickCount();
	unsigned int elapsed = now - m_start_time;
	if (elapsed >= m_timeout)
	{
		return true;
	}
	else {
		return false;
	}
}