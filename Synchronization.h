#ifndef SYNCHRONIZATION_H_INCLUDED
#define SYNCHRONIZATION_H_INCLUDED

#if ((defined _WIN64) || (defined _WIN32))
#define WINDOWS
#endif	//_WIN32 || _WIN64

#ifdef WINDOWS
#include <Windows.h>
#include <synchapi.h>
#endif //WIN32

#include "Utils.h"
#include "ErrorReporting.h"
#ifdef QT_VERSION
#include <qglobal.h>
#endif //WT_VERSION

#include <atomic>

namespace SyncTL
{

enum
{
	SYNCHRONIZATION_ERROR_OK = 0,
	SYNCHRONIZATION_ERROR_OPERATION_CANCELLED  = SYNCHRONIZATION_ERROR_BASE,
	SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
	SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
	SYNCHRONIZATION_ERROR_CANNOT_RELEASE_LOCK,
	SYNCHRONIZATION_ERROR_CANNOT_CREATE_LOCK,
	SYNCHRONIZATION_ERROR_CANNOT_CREATE_EVENT,
	SYNCHRONIZATION_ERROR_NO_RW_LOCK
};

enum Synch
{
	WaitInfinite = INT_MAX
};

/*i suppose this must be in debug only too*/
class SynchContext
{
public:
	//all these strings must be in static memory.
	constexpr SynchContext(const char* const file, unsigned int line, const char* const message = "") :
		m_file(file),
		m_line(line),
		m_message(message)
	{}
	const char* const GetFile() const
	{
		return m_file;
	}
	unsigned int GetLine() const
	{
		return m_line;
	}
	const char* const GetMessage() const
	{
		return m_message;
	}
protected:
	const char* const m_file;
	unsigned int m_line;
	const char* const m_message;
};

//constexpr 
const SynchContext& GetSynchContext(const char* const file, unsigned int line, const char* const msg);

#ifdef _DEBUG		\
// warning -no coma before SYNCH_CONTEXT, it already contains a coma
#define SYNCH_CONTEXT const SynchContext& synch_context = GetSynchContext(__FILE__, __LINE__, "") 
#define SYNCH_CONTEXT_IMPL const SynchContext& synch_context
#define SYNCH_CONTEXT_W_MSG(msg) const SynchContext& synch_context = GetSynchContext(__FILE__, __LINE__, msg) 
#define GET_SYNCH_CONTEXT	synch_context
#define M_SYNCH_CONTEXT_REF	const SynchContext& m_synch_contex;


#define LOCK_NAME_TYPE const wchar_t* const
#define LOCK_NAME_MEMBER(var_name)	LOCK_NAME_TYPE m_##var_name
#define LOCK_NAME_CTOR_INIT_LIST_ENTRY(var_name, lock_name)	 m_##var_name(##lock_name)
#define LOCK_NAME_GETTER(var_name) LOCK_NAME_TYPE Get_##var_name() const { return LOCK_NAME_MEMBER; }
#define LOCK_NAME_CTOR_PARAM(default_value) LOCK_NAME_TYPE lock_name = default_value
//#define SET_LOCK_NAME_IMPL(lock_name)	const char* const _lock_name
//#define SET_LOCK_NAME(lock_name)	SET_LOCK_NAME_IMPL = lock_name


//#define
#else	//nothing, empty string.
#define SYNCH_CONTEXT
#define SYNCH_CONTEXT_IMPL
#define SYNCH_CONTEXT_W_MSG(msg)
#define GET_SYNCH_CONTEXT
#define M_SYNCH_CONTEXT_REF
#define SET_LOCK_NAME_IMPL
#define SET_LOCK_NAME
#endif //DEBUG

//later move it to error reporting or utils
/*class DebugObject
{
public:
	DebugObject(const char* const name = "debug object",
		const char* const cont_class = "") :
		m_name(name),
		m_containing_class_name(cont_class)
	{}
	//no virtual destructor? 
	const char* const GetName() const { return m_name; }
	//parent object info (class name)
	const char* GetClassName() const { return m_containing_class_name; }
	const char* const m_name;
	const char* const m_containing_class_name;
};*/

class BasicReadWriteLock
{
public:
	BasicReadWriteLock()
	{}
	virtual ~BasicReadWriteLock()
	{}
	virtual bool LockForRead(SYNCH_CONTEXT) = 0;
	virtual bool LockForWrite(SYNCH_CONTEXT) = 0;
	virtual bool TryLockForRead(unsigned int timeout_millisoconds = 0
		CH(COMA) SYNCH_CONTEXT) = 0;
	virtual bool TryLockForWrite(unsigned int timeout_milliseconds = 0
		CH(COMA) SYNCH_CONTEXT) = 0;
	virtual void Unlock(SYNCH_CONTEXT) = 0;
};

class NoLock : public BasicReadWriteLock
{
public:
	virtual bool LockForRead(SYNCH_CONTEXT) { return true;  }
	virtual bool LockForWrite(SYNCH_CONTEXT) { return true;  }
	virtual bool TryLockForRead(unsigned int timeout_millisoconds = 0
		CH(COMA) SYNCH_CONTEXT) { return true; }
	virtual bool TryLockForWrite(unsigned int timeout_milliseconds = 0
		CH(COMA) SYNCH_CONTEXT) { return true; }
	virtual void Unlock(SYNCH_CONTEXT) {  }
};

enum
{
	SYNCH_WAIT_OK = 0,
	SYNCH_WAIT_TIMEOUT = 1,
	SYNCH_WAIT_ABANDONED = 2,
	SYNCH_WAIT_FAILED = 3
};


class SynchronizationObject
{
public:
	virtual ~SynchronizationObject() {}
	virtual unsigned int /*error code*/ Wait(unsigned int timeout_milliseconds = Synch::WaitInfinite//) = 0;
		//CH(COMA) SYNCH_CONTEXT_W_MSG("aaa")) = 0;
		CH(COMA) SYNCH_CONTEXT) = 0;
	unsigned int MapSystemErrorToError(unsigned int sys_error);
};

class ReleasableSynchronizationObject : public SynchronizationObject
{
public:
	virtual unsigned int /*error code*/ Release(SYNCH_CONTEXT) = 0;
};

class Event: public SynchronizationObject
{
public:
	Event(bool is_manual_reset = true, bool initial_state = false);
	virtual ~Event();
	void SetEvent();
	void ClearEvent();
	unsigned int /*error code*/ Wait(unsigned int timeout_milliseconds = Synch::WaitInfinite
		CH(COMA) SYNCH_CONTEXT_W_MSG(""));
protected:
#ifdef WINDOWS
	typedef HANDLE EventHandle;
	EventHandle m_handle;
#endif //WINDOWS
};

class Mutex: public ReleasableSynchronizationObject
{
public:
	Mutex();
	virtual ~Mutex();
	unsigned int /*error code*/ Wait(unsigned int timeout_milliseconds = Synch::WaitInfinite 
		CH(COMA) SYNCH_CONTEXT);
	unsigned int /*error code*/ Release();
protected:
#ifdef WINDOWS
	typedef HANDLE MutexHandle;
	MutexHandle m_handle;
#endif //WINDOW
};

#if defined WINDOWS

class WindowsSlimReadWriteLock : public BasicReadWriteLock
{
	/*implemented via Slim read-write lock on Windows (starting with Vista)
	and shared_timed_mutex .
	and via atomics*/
public:
	WindowsSlimReadWriteLock();
	virtual ~WindowsSlimReadWriteLock();
	virtual bool LockForRead(SYNCH_CONTEXT);
	virtual bool LockForWrite(SYNCH_CONTEXT);
	virtual bool TryLockForRead(unsigned int timeout_millisoconds = 0 CH(COMA) SYNCH_CONTEXT);
	virtual bool TryLockForWrite(unsigned int timeout_milliseconds = 0 CH(COMA) SYNCH_CONTEXT);
	virtual void Unlock(SYNCH_CONTEXT);
protected:
	enum
	{
		UNLOCKED = 0,
		EXCLUSIVE = 1, 
		SHARED = 2
	};
	SRWLOCK m_lock;
	unsigned int m_locking_type; //used to track how object was locked
};

#else if defined POSIX

class PosixSharedLock : public BasicReadWriteLock
{
public:
	virtual bool LockForRead(SYNCH_CONTEXT);
	virtual bool LockForWrite(SYNCH_CONTEXT);
	virtual bool TryLockForRead(unsigned int timeout_millisoconds = 0, CH(COMA) SYNCH_CONTEXT);
	virtual bool TryLockForWrite(unsigned int timeout_milliseconds = 0, CH(COMA) SYNCH_CONTEXT);
	virtual void Unlock();
};

#endif

//so long this definition to be here. later will be moved out somewhere

#ifdef WINDOWS
typedef WindowsSlimReadWriteLock ReadWriteLock;
#else if defined POSIX
typedef PosixSharedLock ReadWriteLock;
#endif

//class CriticalSection : public ReleasableSynchronizationObject
//{
//public:
//	CriticalSection();
//	virtual ~CriticalSection();
//	//timeout_milliseconds on Windows is ignored because WinAPI functions for critical section dont support it.
//	unsigned int /*error code*/ Wait(unsigned int timeout_milliseconds = Synch::WaitInfinite);
//	unsigned int /*error code*/ Release();
//protected:
//	CRITICAL_SECTION m_cs;
//};

class Synchronizer
{
public:
	Synchronizer(ReleasableSynchronizationObject* so CH(COMA) SYNCH_CONTEXT) : m_so(so)
	{
		ASSERT(m_so != NULL);
		m_so->Wait();
	}
	virtual ~Synchronizer()
	{
		ASSERT(m_so != NULL);
		m_so->Release();
	}
protected:
	ReleasableSynchronizationObject* m_so;
};

class BasicRWSynchronizer
{
public:
	BasicRWSynchronizer(BasicReadWriteLock* rwl):
		m_rw_lock(rwl)
	{}
	virtual ~BasicRWSynchronizer()
	{
		if (m_rw_lock != NULL)
		{
			m_rw_lock->Unlock();
		}
	}
protected:
	BasicReadWriteLock* m_rw_lock;
};

class ReadSynchronizer: public BasicRWSynchronizer
{
public:
	ReadSynchronizer(BasicReadWriteLock* rwl) :
		BasicRWSynchronizer(rwl)
	{
		if (m_rw_lock != NULL)
		{
			bool ok = m_rw_lock->LockForRead();
			if (ok == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
					L"Cannot lock for read",
					EXC_HERE);
			}
		}
	}
};

class TryReadSynchronizer: public BasicRWSynchronizer
{
public:
	TryReadSynchronizer(BasicReadWriteLock* rwl, unsigned int timeout_milliseconds = 0) :
		BasicRWSynchronizer(rwl)
	{
		if (m_rw_lock != NULL)
		{
			bool ok = m_rw_lock->TryLockForRead(timeout_milliseconds);
			if (ok == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
					L"Cannot lock for read",
					EXC_HERE);
			}
		}
	}
};

class WriteSynchronizer: public BasicRWSynchronizer
{
public:
	WriteSynchronizer(BasicReadWriteLock* rwl) :
		BasicRWSynchronizer(rwl)
	{
		if (m_rw_lock != NULL)
		{
			bool ok = m_rw_lock->LockForWrite();
			if (ok == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
					L"Cannot lock for write",
					EXC_HERE);
			}
		}
	}
};

class TryWriteSynchronizer: public BasicRWSynchronizer
{
public:
	TryWriteSynchronizer(BasicReadWriteLock* rwl, unsigned int timeout_milliseconds = 0) :
		BasicRWSynchronizer(rwl)
	{
		if (m_rw_lock != NULL)
		{
			bool ok = m_rw_lock->TryLockForWrite(timeout_milliseconds);
			if (ok == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
					L"Cannot lock for write",
					EXC_HERE);
			}
		}
	}
};

template <class Synchronizeable>
class BasicTemplateSynchronizer
{
public:
	BasicTemplateSynchronizer(Synchronizeable* object) :
		m_object(object)
	{
		//ASSERT(m_object != NULL);
	}
	virtual ~BasicTemplateSynchronizer()
	{
		//ASSERT(m_object != NULL);
		if (m_object != NULL)
		{
			m_object->Unlock();
		}
	}
protected:
	Synchronizeable* m_object;
};

/*
template <class Synchronizeable>
class TestSynchronizer : public BasicTemplateSynchronizer<Synchronizeable>
{
public:
	typedef BasicTemplateSynchronizer<Synchronizeable> BaseClass; //this works
	TestSynchronizer(Synchronizeable* s):
		BasicTemplateSynchronizer(s)
	{
		//m_object = s;
		BaseClass::m_object->LockForRead();
	}
};*/

template <typename Synchronizeable>
class TemplateReadSynchronizer : public virtual BasicTemplateSynchronizer<Synchronizeable>
{
public:
	typedef BasicTemplateSynchronizer<Synchronizeable> BaseClass;
	TemplateReadSynchronizer(Synchronizeable* object) :
		BasicTemplateSynchronizer<Synchronizeable>(object)
	{
		if (BaseClass::m_object != NULL)
		{
			BaseClass::m_object->LockForRead();
		}
	}
};

template <typename Synchronizeable>
class TemplateWriteSynchronizer: public virtual BasicTemplateSynchronizer<Synchronizeable>
{
public:
	typedef BasicTemplateSynchronizer<Synchronizeable> BaseClass;
	TemplateWriteSynchronizer(Synchronizeable* object) :
		BasicTemplateSynchronizer<Synchronizeable>(object)
	{
		if (BaseClass::m_object != NULL)
		{
			BaseClass::m_object->LockForWrite();
		}
	}
};

template <class Synchronizeable>
class TemplateRWSynchronizer : public TemplateReadSynchronizer<Synchronizeable>,
							   public TemplateWriteSynchronizer<Synchronizeable>
{
public:
	//typedef TemplateReadSynchronizer<Synchronizeable> ReadSyncBaseClass;
	//typedef TemplateWriteSynchronizer<Synchronizeable> WriteSyncBaseClass;
	TemplateRWSynchronizer(Synchronizeable* object):
		BasicTemplateSynchronizer<Synchronizeable>(object),
		TemplateWriteSynchronizer<Synchronizeable>(object),	//because this lock is exclusive it comes first.
		TemplateReadSynchronizer<Synchronizeable>(object)
		
	{}
};

template <bool is_exclusive>
class LockGuard
{
public:
	LockGuard(BasicReadWriteLock* rw_lock CH(COMA) SYNCH_CONTEXT) :
		m_rw_lock(rw_lock)
	{
		if (m_rw_lock == NULL)
		{
			Throw<Exception>(SYNCHRONIZATION_ERROR_NO_RW_LOCK,
				L"no rw lock provided for lock guard",
				EXC_HERE);
		}
		if (is_exclusive)
		{
			if (m_rw_lock->LockForWrite(GET_SYNCH_CONTEXT) == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_NO_RW_LOCK,
					L"cannot lock for write",
					EXC_HERE);
			}
		}
		else {
			if (m_rw_lock->LockForRead(GET_SYNCH_CONTEXT) == false)
			{
				Throw<Exception>(SYNCHRONIZATION_ERROR_NO_RW_LOCK,
					L"cannot lock for read",
					EXC_HERE);
			}
		}
	}
	virtual ~LockGuard()
	{
		if (m_rw_lock != NULL)
		{
			m_rw_lock->Unlock();
		}
	}
	void Detach()	//not sure is it a great idea
	{
		m_rw_lock = NULL;
	}
protected:
	BasicReadWriteLock* m_rw_lock;
};

//_Yield has moved to Utils.h

class FastLockGuard
{
public:
	FastLockGuard(std::atomic_flag* atomic)	//no need for timeouts here - this is FAST spin lock.
		:m_atomic(atomic)
	{
		if (m_atomic != NULL)
		{
			//while (m_atomic->test_and_set(std::memory_order_acquire) == false)	//this thorws exception _INVALID_MEMORY_ORDER in _Atomic_storage::store
			while (m_atomic->test_and_set(std::memory_order_relaxed) == false)
			{ /*do nothing*/
				_Yield();
			}
		}
	}
	~FastLockGuard()
	{
		if (m_atomic != NULL)
		{
			m_atomic->clear(std::memory_order_relaxed);
			//m_atomic->clear(std::memory_order_acquire);	//this thorws exception _INVALID_MEMORY_ORDER in _Atomic_storage::store
		}
	}
protected:
	std::atomic_flag* m_atomic;
};

} //end namespace SyncTL

#endif //SYNCHRONIZATION_H_INCLUDED