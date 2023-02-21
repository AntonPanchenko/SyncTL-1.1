#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

#include "Collections.h"
#include "Synchronization.h"

namespace SyncTL
{

	enum
	{
		TIMER_ERROR_STATIC_MEMBERS_NON_NULL = TIMER_ERROR_BASE,
		TIMER_ERROR_TIMER_VECTOR_NOT_EMPTY,
		TIMER_ERROR_CANNOT_LOCK_TIMER_VECTOR,
		TIMER_ERROR_NOT_FOUND_IN_VECTOR,
		TIMER_ERROR_CANNOT_CREATE_TIMER,
		TIMER_ERROR_CANNOT_DESTROY_TIMER
	};

	class Timer
	{
	public:
		class Event
		{
		public:
			Event() {}
			virtual ~Event() {}
			virtual void OnTimer() = 0;
		};

		//this static methods are needed to initialize static members such as timer vector.
		//first call StaticInit, then create instances of timer, then when all instances are deleted, call StaticDeinit.
		//the best practice is to call this functions on main thread.
		static unsigned int /*error code*/ StaticInit();
		//at the moment of StaticDeinit call all timers must be destroyed and timer list must be empty, otherwise it returns error.
		static unsigned int /*error code*/ StaticDeinit();
		Timer();
		virtual ~Timer();
		unsigned int /*error code*/ SetTimeout(unsigned int timeout_milliseconds, unsigned int is_once);
		unsigned int /*error code*/ Start();
		unsigned int /*error code*/ Stop();
		void SetEvent(Event* event)
		{
			m_event = event;
		}
	protected:
#ifdef WINDOWS
		typedef UINT_PTR TimerID;
		static VOID CALLBACK TimerProcedure(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
#else
		typedef unsigned int TimerID;
#endif //WINDOWS
		struct TimerEntry
		{
			TimerEntry() :
				m_timer_id(NULL),
				m_timer(NULL)
			{}
			TimerID m_timer_id;
			Timer* m_timer;
		};
		static const unsigned int TIMER_VECTOR_PREALLOC = 0x7F;
		static unsigned int /*error code*/ AddToTimerVector(Timer* timer, TimerID timer_id);
		static unsigned int /*error code*/ RemoveFromTimerVector(TimerID timer_id);
		static Timer* FindInTimerVector(TimerID timer_id);
		typedef Vector<TimerEntry> TimerVector;
		static TimerVector* m_timer_vector;
		//static QtReadWriteLock* m_timer_vector_lock; //to elimitnate dependency on Qt in non-qt projects
		static SyncTL::ReadWriteLock* m_timer_vector_lock;
		//SyncTL::SRWReadWriteLock m_timer_vector_lock;
		
		TimerID m_timer_id;
		Event* m_event;
		unsigned int m_timeout_milliseconds;
		unsigned int m_is_once;
	};

	class Timeout
	{
	public:
		Timeout(unsigned int timeout);
		bool IsElapsed();
	protected:
		unsigned int m_start_time;
		unsigned int m_timeout;
	};

} //end namespace SyncTL

#endif //TIMER_H_INCLUDED