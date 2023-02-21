#include "pch.h"
//#include "UniformAllocator.h"

#if ((defined WIN32) || (defined WIN64))
#define WINDOWS
#include <Windows.h>
#endif

using namespace SyncTL;

#ifdef WINDOWS
inline char* Alloc(unsigned int size)
{
	return (char*)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
}

inline void Free(char* addr)
{
	VirtualFree(addr, 0, MEM_RELEASE);
}
#else

inline char* Alloc(unsigned int size)
{
	return malloc(size);
}

inline void Free(char* addr)
{
	return free(addr);
}

#endif //WINDOWS

UniformAllocator::UniformAllocator(unsigned int unit_size, unsigned int array_size):
	m_unit_size(unit_size),
	m_array_size(array_size)
{
}

UniformAllocator::~UniformAllocator()
{
	BasicList::Iterator it = m_array_list.Begin();
	while (it.IsValid())
	{
		Array* array = dynamic_cast<Array*>(it.operator BasicList::Entry *());//it.operator UniformAllocator::Array *();
		++it;
		ASSERT(array != NULL);
		m_array_list.Remove(array);
		DeleteArray(array);
	}
}

void* UniformAllocator::Alloc()
{
	//Synchronizer s(&m_cs);
	//SyncTL::LockGuard<true> lock_guard()
	SyncTL::FastLockGuard lock_guard(&m_atomic_flag);
	void* ret_val = NULL;
	BasicList::Iterator it = m_array_list.Begin();
	Array* array = NULL;
	while (it.IsValid())
	{
		Array* current_array = dynamic_cast<Array*>(it.operator BasicList::Entry *());
			//(Array*)(it);
			//it.operator UniformAllocator::Array *();
		ASSERT(current_array != NULL);
		if (current_array->GetFreeCount() > 0)
		{
			array = current_array;
			break;
		}
		++it;
	}
	if (array == NULL)
	{
		Array* new_array = CreateArray();
		if (new_array == NULL)
		{
			Throw<Exception>(ERR_CANNOT_CREATE_ALLOCATOR_ARRAY, L"Cannot create allocator array", EXC_HERE);
		}
		m_array_list.PushBack(new_array);
		array = new_array;
	}
	ret_val = array->Alloc();
	return ret_val;
}

void UniformAllocator::Free(void* addr)
{
	//SyncTL::LockGuard s(&m_cs);
	SyncTL::FastLockGuard lock_guard(&m_atomic_flag);
	ASSERT(addr != NULL);
	Array* array = NULL;
	BasicList::Iterator it = m_array_list.Begin();
	Array* current_array = NULL;
	while (it.IsValid())
	{
		BasicList::Entry* ble = it;
		current_array = dynamic_cast<Array*>(ble);
		//current_array = dynamic_cast<Array*>(it.operator BasicList::Entry *());
		ASSERT(current_array != NULL);
		if (current_array->IsMyAllocation(addr) == true)
		{
			array = current_array;
			break;
		}
		++it;
	}
	if (array != NULL)
	{
		array->Free(addr);
		if (array->GetFreeCount() == m_array_size)
		{
			m_array_list.Remove(it);
			::Free((char*)array);
		}
	}
}

bool UniformAllocator::IsMyAllocation(void* addr) const
{
	//Synchronizer s(const_cast<CriticalSection*>(&m_cs));
	SyncTL::FastLockGuard lock_guard((std::atomic_flag*)&m_atomic_flag);
	BasicList::Iterator it = const_cast<BasicList&>(m_array_list).Begin();
	while (it.IsValid())
	{
		Array* current_array = dynamic_cast<Array*>(it.operator BasicList::Entry *());
		ASSERT(current_array != NULL);
		if (current_array->IsMyAllocation(addr))
		{
			return true;
		}
		++it;
	}
	return false;
}

//these methods are internals. no need for synchronization here
UniformAllocator::Array* UniformAllocator::CreateArray()
{
	char* data = NULL;
	ASSERT(m_unit_size > 0);
	ASSERT(m_array_size > 0);
	data = (char*)::Alloc(sizeof(Array) + ((sizeof(Allocation) + m_unit_size) * m_array_size));
	if (data == NULL)
	{
		Throw<Exception>(ERR_CANNOT_ALLOC, L"Cannot create allocation array", EXC_HERE);
	}
	Array* ret_val = new (data) Array(m_unit_size, m_array_size);
	return ret_val;
}

void UniformAllocator::DeleteArray(Array* array)
{
	ASSERT(array != NULL);
	array->~Array();
	::Free((char*)array);
}

UniformAllocator::Array::Array(unsigned int unit_size, unsigned int count):
	m_unit_size(unit_size),
	m_count(count),
	m_free_count(m_count),
	m_nearest_free_index(0)
{
	ASSERT(m_unit_size > 0);
	ASSERT(m_count > 0);
	char* a_ptr = ((char*)this + sizeof(Array));
	unsigned int index = 0;
	while(index < count)
	{
		new (a_ptr) Allocation();
		++index;
		a_ptr += GetAbsoluteUnitSize();
	}
}

UniformAllocator::Array::~Array()
{
	unsigned int index = 0;
	char* a_ptr = (((char*)this) + sizeof(Array));
	while (index < m_count)
	{
		Allocation* a = (Allocation*)(a_ptr);
		a->~Allocation();
		a_ptr += GetAbsoluteUnitSize();
		++index;
	}
}

void* UniformAllocator::Array::Alloc()
{
	void* ret_val = NULL;
	Allocation* a = NULL;
	if (GetFreeCount() > 0)
	{
		//char* a_ptr = GetArrayStart();
		//a_ptr += (m_nearest_free_index * GetAbsoluteUnitSize());
		//a = (Allocation*)a_ptr;
		a = GetAllocation(m_nearest_free_index);
		ASSERT(a->GetIsFree() == true);
		a->SetIsFree(false);
		ret_val = a->GetDataPtr(); 	//pointer to data right behind the Allocation structure
		bool free_allocation_found = false;
		for (unsigned int index = m_nearest_free_index + 1; 
			((index < m_count) && (free_allocation_found == false)); 
			++index)
		{
			a = GetAllocation(index);
			if (a->GetIsFree() == true)
			{
				m_nearest_free_index = index;
				free_allocation_found = true;
				ASSERT(GetAllocation(m_nearest_free_index)->GetIsFree() == true);
			}
		}
		--m_free_count;
		//if (free_allocation_found = false)
		if(m_free_count == 0)
		{
			m_nearest_free_index = INVALID_FREE_INDEX;
		}
	}
	return ret_val;
}

void UniformAllocator::Array::Free(void* addr)
{
	ASSERT(addr != NULL);
	if (IsMyAllocation(addr) == false)
	{
		Throw<Exception>(ERR_CANNOT_FREE, L"Array: not My allocation", EXC_HERE);
	}
	Allocation* a = GetAllocation((char*)addr);
	a->SetIsFree(true);
	int64_t free_index = (((char*)addr - (char*)GetArrayStart()) / GetAbsoluteUnitSize());
	if ((free_index <= m_nearest_free_index) || (m_nearest_free_index == INVALID_FREE_INDEX))
	{
		ASSERT(GetAllocation(free_index)->GetIsFree() == true);
		m_nearest_free_index = free_index;
	}
	ASSERT(a->GetIsFree() == true);
	ASSERT(GetAllocation(m_nearest_free_index)->GetIsFree() == true);
	++m_free_count;
}