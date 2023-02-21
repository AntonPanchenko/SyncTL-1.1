#pragma once

#ifndef ERROR_REPORTING_H_INCLUDED
#define ERROR_REPORTING_H_INCLUDED

//temporary
#include <iostream>
#include <thread>
#include "Utils.h"

/*in multithreaded lockfree environmnent everything I deal with must have a name
(threads, synchronization objects, e.t.c.)
because the only way to gigure out what's going on there is logging.*/
class ObjectIdentity
{
public:
	enum
	{ MAX_SERIALIZATION_SIZE = 0xFF};	/*take it inot account while inventing names*/
	ObjectIdentity(const char* const obj_ptr,	/*must be prepared for any kind of objects*/
		const char* const name,
		const char* const description) :
		m_object_ptr(obj_ptr),
		m_name(name),
		m_description(description)
	{}
	const char* const GetObjeectPtr() const { return m_object_ptr; }
	const char* const GetName() const { return m_name; }
	const char* const GetDescription() const { return m_description; }
protected:
	const char* const m_object_ptr;	//it is not required for identity to be at the beginning of the object.
						//moreover, multiple inheritance, inclusion, e. t. c.
	const char* const m_name;
	const char* const m_description;
};

class Exception
{
public:
	constexpr Exception(unsigned int error_code,
		const wchar_t* const message,
		const char* const filename,
		unsigned int line,
		unsigned int system_error_code = 0) :
		m_error_code(error_code),
		m_message(message),
		m_filename(filename),
		m_line(line),
		m_system_error_code(system_error_code)
	{}
	virtual ~Exception()
	{
		std::wcout << L"BYE Exception\n";
	}
	constexpr inline unsigned int GetErrorCode() const
	{
		return m_error_code;
	}
	constexpr inline const wchar_t* GetErrorMessage() const
	{
		return m_message;
	}
	constexpr inline const char* GetFilename() const	//macros _LINE_ creates ansi string
	{
		return m_filename;
	}
	constexpr inline unsigned int GetLine() const
	{
		return m_line;
	}
	constexpr inline unsigned int GetSystemErrorCode() const
	{
		return m_system_error_code;
	}
protected:
	unsigned int m_error_code;
	const wchar_t* m_message;
	const char* m_filename;
	unsigned int m_line;
	unsigned int m_system_error_code;
};

//reason for this to be here?
extern const wchar_t* ERR_MSG_CANNOT_LOAD_IMAGE; 

#define HERE		__FILE__, __LINE__
//this is to use in Exception constructor
#define EXC_HERE	HERE	//__FILE__, __LINE__

#if defined(NOGUI)
//to log. later.
#define ReportMessage
#define ReportError
#else
#define ReportMessage ShowMessageDialog
#define ReportError ShowErrorDialog
#endif

const wchar_t* GetErrorMessage(unsigned int error_code);
//both ShowMessageDialog functons are modal
unsigned int /*return code*/ _ShowMessageDialog(unsigned int error_code, const wchar_t* details = 0);
unsigned int /*return code*/ _ShowMessageDialog(const wchar_t* message, const wchar_t* details = 0);
unsigned int /*return code*/ _ShowErrorDialog(const wchar_t* message, const Exception& exc);

void InternalOutputDebugMsg(const wchar_t* msg);
void InternalOutputDebugMsg(const char* msg);

#ifdef _DEBUG
#define OutputDebugMsg(msg)	InternalOutputDebugMsg(msg)
#else
#define OutputDebugMsg(msg)
#endif //DEBUG_

//the code below is required to pass exception by address, not by value. exception itself is static.
template <class Exception>
inline void Throw(unsigned int code, const wchar_t* const message, const char* const file, unsigned int line)
{
	thread_local Exception exc(code, message, file, line); //must be separate for each thread (thread-local?)
	throw &exc;
	//throw Exception (code, message, file, line);
}

#define THROW Throw<Exception>

#define CATCH(Exc, name) catch (Exc* name)
//#define CATCH(Exc, name) catch (Exc name)

class DebugOut
{
public:
	class Lock
	{};
	class Unlock
	{};
	DebugOut()
	{
		ATOMIC_FLAG_INIT(m_atomic_flag);
		//m_atomic_flag = ATOMIC_FLAG_INIT;
	}
	DebugOut& operator << (const char* const msg)
	{
		OutputDebugMsg(msg);
		std::wcout << msg << L"\n";
		return *this;
	}
	DebugOut& operator << (const wchar_t* const msg)
	{
		OutputDebugMsg(msg);
		std::wcout << msg << L"\n";
		return *this;
	}
	DebugOut& operator << (unsigned int i)
	{
		char str [(sizeof(i) << 3) + 1];
		_itoa_s(i, str, 10);
		OutputDebugMsg(str);
		std::wcout << str << L"\n";
		return *this;
	}
	DebugOut& operator << (void* ptr)
	{
		constexpr unsigned int str_size = (sizeof(ptr) << 3) + 1;
		char str[str_size];
		_ui64toa_s((unsigned long long)ptr, str, str_size, 16);
		OutputDebugMsg(str);
		std::wcout << str << L"\n";
		return *this;
	}
	DebugOut& operator << (Lock& lock)
	{
		while (m_atomic_flag.test_and_set())
		{
			_Yield();
		}
		return *this;
		//std::atomic_flag f = ATOMIC_FLAG_INIT;
	}
	DebugOut& operator << (Unlock& unlock)
	{
		m_atomic_flag.clear();
		return *this;
	}
	DebugOut& operator << (const ObjectIdentity& id)
	{
		char out_msg[ObjectIdentity::MAX_SERIALIZATION_SIZE];
		memset(out_msg, 0, sizeof(out_msg));
		sprintf_s(out_msg, ObjectIdentity::MAX_SERIALIZATION_SIZE, "object at address: %x - name: %s , description: %s\n",
			id.GetObjeectPtr(),
			id.GetName(),
			id.GetDescription());
		OutputDebugMsg(out_msg);
	}
protected:
	std::atomic_flag m_atomic_flag;
};

DebugOut& PrintCallStack(DebugOut& dbgout);

#endif //ERROR_REPORTING_H_INCLUDED