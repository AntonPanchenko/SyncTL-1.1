#include "pch.h"
//#include "Collections.h"
#include <cstring>
#include <stdlib.h>
//#include <stdatomic.h>
#include <atomic>

/*
//Vector<int> tmp_v;
SyncTL::BasicVector tmp_v;
SyncTL::TestSynchronizer<SyncTL::BasicVector> ts(&tmp_v);
//
*/
using namespace SyncTL;


/*
BasicVector tmp_v2;
SyncTL::TestSynchronizer<BasicVector> tv2(&tmp_v2);
//

BasicList tmp_l;
TestSynchronizer<BasicList> tl(&tmp_l);
*/

char* BasicVector::DefaultAllocator::AllocateDataArray(unsigned int entry_size, unsigned int count)
{
	return (char*)malloc(entry_size * count);
}

void BasicVector::DefaultAllocator::FreeDataArray(char* data_array)
{
	free(data_array);
}

BasicVector::Iterator::Iterator(unsigned int index,
								BasicVector* vector):
								m_index(index),
								m_vector(vector)
{}

BasicVector::Iterator::~Iterator()
{}

BasicVector::Iterator BasicVector::Iterator::operator++ ()
{
	Iterator ret_val = *this;
	++m_index;
	return ret_val;
}

BasicVector::Iterator& BasicVector::Iterator::operator++ (int)
{
	++m_index;
	return *this;
}

BasicVector::Iterator BasicVector::Iterator::operator-- ()
{
	Iterator ret_val = *this;
	--m_index;
	return ret_val;
}

BasicVector::Iterator& BasicVector::Iterator::operator-- (int)
{
	--m_index;
	return *this;
}

BasicVector::Iterator& BasicVector::Iterator::operator = (const BasicVector::Iterator& it)
{
	m_vector = it.m_vector;
	m_index = it.m_index;
	return *this;
}

bool BasicVector::Iterator::IsValid() const
{
	return ((m_vector != NULL) && (m_index < m_vector->m_count));
}

BasicVector::BasicVector(BasicReadWriteLock* lock):
	m_entry_size(0),
	m_data_array_size(0),
	m_data(NULL),
	m_count(0),
	m_allocator(NULL),
	m_rw_lock(lock)
	//and BasicVector object will be invalid until copy constructor or assignment operator execution.
	//this is needed for tree iterators.
{}

BasicVector::BasicVector(const BasicVector& another):
	m_entry_size(another.m_entry_size),
	m_data_array_size(0),
	m_data(NULL),
	m_count(0),
	m_allocator(another.m_allocator),
	m_rw_lock(NULL)	//lock by default is not copied because this is strange when access to one collection is denied 
					//because another collection is locked.
{
	if (m_allocator != NULL)
	{
		if ((another.m_data != NULL) && (another.m_entry_size != 0) && (another.m_count != 0))
		{
			ResizeDataArray(another.m_data_array_size);
			//copy data from another
			memcpy(m_data, another.m_data, another.m_data_array_size);
			m_data_array_size = another.m_data_array_size;
			m_count = another.m_count;
		}
	}
}

BasicVector::BasicVector(unsigned int entry_size, 
						 unsigned int n_preallocated, 
						 BasicVector::Allocator* allocator,
						 BasicReadWriteLock* lock):
	m_entry_size(entry_size),
	m_data_array_size(0),
	m_data(NULL),
	m_count(0),
	m_allocator(allocator),
	m_rw_lock(lock)
{
	ASSERT(m_entry_size != 0);
	ASSERT(m_allocator != NULL);
	if (n_preallocated != 0)
	{
		if (n_preallocated < m_allocator_increment)
		{
			n_preallocated = m_allocator_increment;
		}
		ResizeDataArray(n_preallocated);
	}
}

BasicVector::~BasicVector()
{
	if (m_data != NULL)
	{
		if (m_allocator != NULL)
		{
			m_allocator->FreeDataArray(m_data);
		}
	}
}

/*void BasicVector::Init(unsigned int entry_size, 
	unsigned int n_preallocated, 
	BasicVector::Allocator* allocator, 
	BasicReadWriteLock* lock)
{}*/

void BasicVector::ResizeDataArray(unsigned int n_entries)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	if (m_allocator == NULL)
	{
		Throw<Exception>(UTILS_ERROR_NO_ALLOCATOR,
			L"Cannot increase data array because array must be reallocated and there is no allocator",
			EXC_HERE);
	}
	char* new_data_array = m_allocator->AllocateDataArray(m_entry_size, n_entries * m_entry_size);
	if (new_data_array == NULL)
	{
		Throw<Exception>(UTILS_ERROR_NO_ALLOCATED_MEMORY,
			L"Cannot increase data array because allocator could not allocate a new data array",
			EXC_HERE);
	}
	if (m_data != NULL)
	{
		char* src_ptr = m_data;
		char* dst_ptr = new_data_array;
		unsigned int count = n_entries;
		if (count > m_count)
		{
			count = m_count;
		}
		while (count > 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr += m_entry_size;
			dst_ptr += m_entry_size;
			--count;
		}
		m_allocator->FreeDataArray(m_data);
	}
	m_data = new_data_array;
	m_data_array_size = n_entries;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
}

void BasicVector::GetEntry(unsigned int index, char** out_entry) const
{
	ASSERT(m_count <= m_data_array_size);
	ReadSynchronizer sync(m_rw_lock);
	if (index >= m_count)
	{
		Throw<Exception>(UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
			L"Cannot get entry from the vector because entry index is bigger than current vector size",
			EXC_HERE);
	}
	if (out_entry == NULL)
	{
		Throw<Exception>(UTILS_ERROR_INVALID_PARAMETERS,
			L"out_entry value == NULL, cannot return there anything",
			EXC_HERE);
	}
	(*out_entry) = m_data + (index * m_entry_size);
}

char* BasicVector::InsertEntry(unsigned int index, const char* data)
{
	char* ret_val = NULL;
	WriteSynchronizer sync(m_rw_lock);
	//if (index >= m_data_array_size)
	//insertion beyond vector size + 1 is disallowed (what to do with iterators then?)
	if (index > m_count)
	{
		Throw<Exception>(UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
			L"Cannot insert a new entry because insertion index is far beyond the vector size",
			EXC_HERE);
	}
	unsigned int new_size = m_count + 1;
	if (new_size >= m_data_array_size)
	{
		//allocate new array
		if (m_allocator == NULL)
		{
			Throw<Exception>(UTILS_ERROR_NO_ALLOCATOR,
				L"Cannot increase data array because array must be reallocated and there is no allocator",
				EXC_HERE);
		}
		char* array = m_allocator->AllocateDataArray(m_entry_size, m_data_array_size + m_allocator_increment);
		if (array == NULL)
		{
			Throw<Exception>(UTILS_ERROR_NO_ALLOCATED_MEMORY,
				L"Cannot allocate memory for the new vector",
				EXC_HERE);
		}
		const char* src_ptr = m_data;
		char* dst_ptr = array;
		//copy previous entries.. NO MEMCPY!
		if (index > 0)
		{
			unsigned int prev_count = index;
			while (prev_count != 0)
			{
				CopyEntry(src_ptr, dst_ptr);
				src_ptr += m_entry_size;
				dst_ptr += m_entry_size;
				--prev_count;
			}
		}
		//copy new entry
		CopyEntry(data, dst_ptr);
		ret_val = dst_ptr;
		//src_ptr += m_entry_size;
		dst_ptr += m_entry_size;
		//copy the rest of entries
		unsigned int rest_count = m_count - index;
		while (rest_count > 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr += m_entry_size;
			dst_ptr += m_entry_size;
			--rest_count;
		}
		m_allocator->FreeDataArray(m_data);
		m_data = array;
		m_data_array_size += m_allocator_increment;
	} else {
		//move entries above starting from the last one step up.
		char* src_ptr = m_data + ((m_count - 1) * m_entry_size);//m_data + (index * m_entry_size);
		char* dst_ptr = src_ptr + m_entry_size;
		unsigned int rest_count = m_count - index;
		while (rest_count > 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr -= m_entry_size;
			dst_ptr -= m_entry_size;
			--rest_count;
		}
		//copy new entry
		dst_ptr = m_data + (index * m_entry_size);
		CopyEntry(data, dst_ptr);
	}
	++m_count;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return ret_val;
}

void BasicVector::RemoveEntry(unsigned int index)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	if (index >= m_count)
	{
		Throw<Exception>(UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
			L"Cannot remove entry from the vector because entry index is bigger than current vector size",
			EXC_HERE);
	}
	//if new size is less then current size - allocation size
	unsigned int new_size_in_entries = (m_count - 1);
	//allocator increment is on terms of entries.
	ASSERT(m_allocator_increment != NULL);
	unsigned int new_size_in_allocator_increments = new_size_in_entries / m_allocator_increment;
	new_size_in_allocator_increments += 1;// m_allocator_increment;
	unsigned int current_size_in_allocator_increments = m_data_array_size / m_allocator_increment;
	if (new_size_in_allocator_increments < current_size_in_allocator_increments)
	{
		//allocate new array
		if (m_allocator == NULL)
		{
			Throw<Exception>(UTILS_ERROR_NO_ALLOCATOR,
				L"Cannot decrease data array because array must be reallocated and there is no allocator",
				EXC_HERE);
		}
		ASSERT(new_size_in_allocator_increments != 0);
		unsigned int new_data_array_size = new_size_in_allocator_increments * m_allocator_increment;
		char* new_array = m_allocator->AllocateDataArray(m_entry_size, new_data_array_size);
		if (new_array == NULL)
		{
			Throw<Exception>(UTILS_ERROR_NO_ALLOCATED_MEMORY,
				L"Cannot decrease data array because allocator cannot allocate a new data array",
				EXC_HERE);
		}
		//copy previous entries to new array
		char* src_ptr = m_data;
		char* dst_ptr = new_array;
		int prev_count = index - 1;
		while (prev_count >= 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr += m_entry_size;
			dst_ptr += m_entry_size;
			--prev_count;
		}
		/*do
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr += m_entry_size;
			dst_ptr += m_entry_size;
			--prev_count;
		} while (prev_count >= 0);
		*/
		DeinitEntry(src_ptr);
		//skip entry being removed
		src_ptr += m_entry_size;
		//copy the rest of entries to new array
		int rest_count = m_count - index - 1;		//no - 1, 1 is already subtracted in copy prev.
		//do
		while (rest_count > 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			src_ptr += m_entry_size;
			dst_ptr += m_entry_size;
			--rest_count;
		}// while (rest_count > 0);
		m_allocator->FreeDataArray(m_data);
		m_data = new_array;
		m_data_array_size = new_data_array_size;
	} else {	//copy the rest of entries one step lower.
		char* dst_ptr = m_data + (index * m_entry_size);
		char* src_ptr = dst_ptr + m_entry_size;
		DeinitEntry(dst_ptr);
		unsigned int copy_count = m_count - index - 1;	// -2;
		while (copy_count != 0)
		{
			CopyEntry(src_ptr, dst_ptr);
			--copy_count;
			dst_ptr += m_entry_size;
			src_ptr += m_entry_size;
		}
	}
	--m_count;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
}

void BasicVector::Clear()
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	for (unsigned int index = 0; index < GetCount(); ++index)
	{
		char* entry_ptr = m_data + (index * m_entry_size);
		DeinitEntry(entry_ptr);
	}
	m_allocator->FreeDataArray(m_data);
	m_data = NULL;	//so ResizeDataArray have nothing to copy
	ResizeDataArray(m_allocator_increment);	//to the same state as it was after construction.
	m_count = 0;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
}

BasicVector& BasicVector::operator = (const BasicVector& another)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock destination vector for write",
				EXC_HERE);
		}
	}*/
	/*if (another.m_rw_lock != NULL)
	{
		if (another.m_rw_lock->LockForRead() == false)
		{
			m_rw_lock->Unlock();
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
				L"Cannot lock source vector for read",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer this_sync(m_rw_lock);
	ReadSynchronizer another_sync(another.m_rw_lock);
	if ((another.m_data != NULL) && (another.m_data_array_size != 0) && (another.m_count != 0))
	{
		ResizeDataArray(another.m_data_array_size);
		//copy data from another
		memcpy(m_data, another.m_data, another.m_data_array_size);
		m_data_array_size = another.m_data_array_size;
		m_count = another.m_count;
		m_entry_size = another.m_entry_size;
	} else {
		//another is invalid. set this to NULL state
		if (m_allocator != NULL)
		{
			m_allocator->FreeDataArray(m_data);
		}
		m_data = NULL;
		m_data_array_size = 0;
		m_count = 0;
		m_entry_size = 0;
	}
	/*if (another.m_rw_lock != NULL)
	{
		another.m_rw_lock->Unlock();
	}
	if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return *this;
}

BasicVector::Iterator BasicVector::Begin()
{
	/*if ((m_rw_lock != NULL) && (m_rw_lock->TryLockForRead() == false))
	{
		throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
			L"vector is locked and cannot be read",
			EXC_HERE);
	}*/
	TryReadSynchronizer sync(m_rw_lock);
	return Iterator(0, this);
}

BasicVector::Iterator BasicVector::Last()
{
	/*if ((m_rw_lock != NULL) && (m_rw_lock->TryLockForRead() == false))
	{
		throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
			L"vector is locked and cannot be read",
			EXC_HERE);
	}*/
	TryReadSynchronizer sync(m_rw_lock);
	if (m_count == 0)
	{
		return Iterator(0, this);
	}
	return Iterator(m_count - 1, this);
}

bool BasicVector::LockForRead()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForRead();
	}
	return true;
}

bool BasicVector::LockForWrite()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForWrite();
	}
	return true;
}

void BasicVector::Unlock()
{
	if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}
}

void BasicVector::CopyEntry(const char* src, char* dst)
{
	memcpy(dst, src, m_entry_size);
}

void BasicVector::DeinitEntry(char* entry)
{
	//nothing
}

void BasicStack::PushFront(const char* data)
{
	ASSERT(data != NULL);
	//if caller passes here an invalid data ptr, it's caller's fault.
	InsertEntry(0, data);
}

void BasicStack::PushBack(const char* data)
{
	ASSERT(data != NULL);
	//if caller passes here an invalid data ptr, it's caller's fault.
	InsertEntry(m_count, data);
}

void BasicStack::PopFront()
{
	RemoveEntry(0);
}

void BasicStack::PopBack()
{
	RemoveEntry(m_count - 1);
}

char* BasicStack::Top()
{
	/*if ((m_rw_lock != NULL) && (m_rw_lock->TryLockForRead() == false))
	{
		throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
			L"vector is locked for read",
			EXC_HERE);
	}*/
	TryReadSynchronizer sync(m_rw_lock);
	if (m_count > 0)
	{
		ASSERT(m_data != NULL);
		return m_data + ((m_count - 1) * m_entry_size);
	} else {
		return NULL;
	}
}

BasicList::BasicList(BasicReadWriteLock* lock):
	m_head(NULL),
	m_last(NULL),
	m_count(0),
	m_rw_lock(lock)
{
	int i = 0;
	int j = i;
}

BasicList::~BasicList()
{
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->LockForWrite();	//cannot throw exceptions here,
	}*/
	WriteSynchronizer sync(m_rw_lock);
	if (m_count != 0)
	{
		ASSERT(m_head != NULL);
		Entry* current_entry = m_head;
		bool exit_flag = false;
		while (exit_flag == false)
		{
			Entry* next_entry = current_entry->m_next;
			current_entry->Remove();
			current_entry = next_entry;
			if (current_entry == NULL)
			{
				exit_flag = true;
			}
		}
	}
#ifdef _DEBUG
	else {
		ASSERT(m_head == NULL);
		ASSERT(m_last == NULL);
	}
#endif //_DEBUG
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
}

void BasicList::Entry::Remove()
{
	/*if (m_list != NULL)
	{
		if (m_list->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock the list for write",
				EXC_HERE);
		}
	}*/
	//TemplateWriteSynchronizer<BasicList> sync(m_list);
	TemplateWriteSynchronizer<BasicReadWriteLock> sync(m_list->m_rw_lock);
	if(m_list == NULL)
	{
		Throw<Exception>(UTILS_ERROR_NOT_IN_COLLECTION,
			L"Cannot remove entry from the list because this entry is not in list",
			EXC_HERE);
	}
	if ((m_prev == NULL) && (m_next == NULL) && (m_list->GetCount() > 1))
	{
		Throw<Exception>(UTILS_ERROR_NOT_IN_COLLECTION,
			L"Cannot remove entry from the list because this entry looks like to be the last in the list but it is not (list->GetCount() > 1)",
			EXC_HERE);
	}
	if (m_prev != NULL)
	{
		if (m_next != NULL)
		{
			m_next->m_prev = m_prev;
			m_prev->m_next = m_next;
		}
		else {
			m_prev->m_next = NULL;
		}
	}
	else {
		if (m_next != NULL)
		{
			m_next->m_prev = NULL;
		}
	}
	--m_list->m_count;
	/*if (m_list != NULL)
	{
		m_list->Unlock();
	}*/
}

bool BasicList::LockForRead()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForRead();
	}
	return true;
}

bool BasicList::LockForWrite()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForWrite();
	}
	return true;
}

void BasicList::Unlock()
{
	if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}
}

BasicList::Entry* BasicList::InternalInsert(BasicList::Entry* entry,
	BasicList::Entry* before_this_entry /*may be NULL, this means PushBack*/)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot lock a list for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	if ((entry->m_prev != NULL) || (entry->m_next != NULL) || (entry->m_list != NULL))
	{
		//add unlock here
		Throw<Exception>(UTILS_ERROR_CANNOT_INSERT_ALREADY_INSERTED,
			L"Cannot insert a new list entry because it is already inserted somewhere",
			EXC_HERE);
	}
	if (before_this_entry == NULL)
	{
		if (m_last == NULL)
		{	//this means the list is still empty
			ASSERT(m_head == NULL);
			ASSERT(m_count == 0);
			m_head = entry;
			m_last = entry;
			++m_count;
			if (m_rw_lock != NULL)
			{
				m_rw_lock->Unlock();
			}
			entry->m_list = this;
			return entry;
		}
		else {
			//this means PushBack
			ASSERT(m_count != 0);
			ASSERT(m_head != NULL);
			m_last->m_next = entry;
			entry->m_prev = m_last;
			m_last = entry;
			++m_count;
			entry->m_list = this;
			if (m_rw_lock != NULL)
			{
				m_rw_lock->Unlock();
			}
			return entry;
		}
	}
	ASSERT(before_this_entry != NULL);	//already checked it just above
	Entry* prev_entry = before_this_entry->m_prev;
	if (prev_entry != NULL)
	{
		prev_entry->m_next = entry;
		entry->m_prev = prev_entry;
	}
	before_this_entry->m_prev = entry;
	entry->m_next = before_this_entry;
	++m_count;
	entry->m_list = this;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return entry;
}

BasicList::Entry* BasicList::InternalRemove(BasicList::Entry* entry)
{
	ASSERT(entry != NULL);
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"cannot lock a list for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	if(entry->m_list != this)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
			L"Cannot remove entry because it's m_list != this",
			EXC_HERE);
	}
	if ((entry->m_prev == NULL) && (entry->m_next == NULL) && ((entry != m_head) && (entry != m_last)))
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
			L"Cannot remove entry because it is not in list",
			EXC_HERE);
	}
	Entry* prev = entry->m_prev;
	Entry* next = entry->m_next;
	if (prev != NULL)
	{
		if (next != NULL)
		{
			next->m_prev = prev;
			prev->m_next = next;
		}
		else {
			//this may be the last
			if (entry != m_last)
			{
				Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
					L"Cannot remove entry because it is not in list",
					EXC_HERE);
			}
			prev->m_next = NULL;
			m_last = prev;
		}
	}
	else {
		//this may be the first
		if (entry != m_head)
		{
			Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
				L"Cannot remove entry because it is not in list",
				EXC_HERE);
		}
		if (next != NULL)
		{
			next->m_prev = NULL;
			m_head = next;
		}  //else this was the only one entry in the list.
		else {
			ASSERT(m_count == 1);
			ASSERT((m_head == m_last) && (m_head == entry));
			m_head = NULL;
			m_last = NULL;
		}
	}
	entry->m_list = NULL;
	--m_count;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return next;	//may be NULL if removed entry was the last.
}

void BasicList::Iterator::Advance(bool forward)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->TryLockForRead() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
				L"List cannot be locked for read",
				EXC_HERE);
		}
	}*/
	TryReadSynchronizer sync(m_rw_lock);
	if (m_entry == NULL)
	{
		Throw<Exception>(UTILS_ERROR_NULL_ITERATOR,
			L"Cannot move an iterator because iterator does not point to any element",
			EXC_HERE);
	}
	Entry* next_entry = NULL;
	if (forward)
	{
		next_entry = m_entry->m_next;
	}
	else {
		next_entry = m_entry->m_prev;
	}
	//now it's OK for iterator to reach end. it just becomes invalid and caller have to chech it.
	m_entry = next_entry;
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
}

bool BasicTree::Entry::ChildrenIterator::Advance(bool forward)
{
	BasicTree::Entry* next = NULL;
	if (forward)
	{
		next = m_child->m_next_sibling;
	} else {
		next = m_child->m_prev_sibling;
	}
	if (next != NULL)
	{
		m_child = next;
		return true;
	} else {
		return false;
	}
}

BasicTree::Entry::~Entry()
{
	//bool exit_flag = false;
	/*while (exit_flag == false)
	{
		//TODO: use iterator here to traverse all children.
	}*/
}

bool BasicTree::Entry::AddChild(BasicTree::Entry* new_child, BasicTree::Entry* child_before)
{
	if (new_child == NULL)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_INSERT_NULL_ENTRY,
			L"Cannot add a child entry to the entry because the child entry parameter == NULL",
			EXC_HERE);
		return false;
	}
	if ((new_child->m_prev_sibling != NULL) || (new_child->m_next_sibling != NULL))
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_INSERT_ALREADY_INSERTED,
			L"Cannot add a child entry to the entry because new child entry is already added somewhere",
			EXC_HERE);
		return false;
	}
	if (m_first_child == NULL)
	{	//this is the case when entry has no children.
		ASSERT(m_last_child == NULL);
		ASSERT(m_children_count == 0);
		m_first_child = new_child;
		m_last_child = new_child;
		new_child->m_parent = this;
	} else {
		Entry* prev_child_entry = NULL;
		if (child_before != NULL)
		{
			prev_child_entry = child_before->m_prev_sibling;
		}
		if (prev_child_entry == NULL)
		{
			prev_child_entry = m_last_child;
		}
		//here some children must be, so assert see below.
		ASSERT(prev_child_entry != NULL);
		ASSERT(m_children_count != 0);
		prev_child_entry->m_next_sibling = new_child;
		new_child->m_prev_sibling = prev_child_entry;
		if (child_before != NULL)
		{
			child_before->m_prev_sibling = new_child;
			new_child->m_next_sibling = child_before;
		} else {
			new_child->m_next_sibling = NULL;
			m_last_child = new_child;
		}
		new_child->m_parent = this;
	}
	++m_children_count;
	return true;
}

BasicTree::Entry* /*next child*/ BasicTree::Entry::RemoveChild(BasicTree::Entry* child)
{
	if (child == NULL)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
			L"Cannot remove a child from the tree entry because child == NULL",
			EXC_HERE);
	}
	if (child->m_parent != this)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
			L"Cannot remova a child from the tree entry because it's parent != this",
			EXC_HERE);
	}
	Entry* prev_sibling = child->m_prev_sibling;
	Entry* next_sibling = child->m_next_sibling;
	if (prev_sibling != NULL)
	{
		if (next_sibling != NULL)
		{	//somewhere in the middle
			prev_sibling->m_next_sibling = next_sibling;
			next_sibling->m_prev_sibling = prev_sibling;
		} else {	//in the end
			prev_sibling->m_next_sibling = NULL;
			m_last_child = prev_sibling;
		}
	} else {	//in the beginning
		if (next_sibling != NULL)
		{
			next_sibling->m_prev_sibling = NULL;
			m_first_child = next_sibling;
		} else {
			//this was the last child of the entry.
			ASSERT(m_children_count == 1);
			m_first_child = NULL;
			m_last_child = NULL;
		}
	}
	child->m_parent = NULL;
	child->m_prev_sibling = NULL;
	child->m_next_sibling = NULL;
	ASSERT(m_children_count > 0);
	--m_children_count;
	return next_sibling;
}

/*if parent == NULL, then root is parent. if child == NULL, then the first child of parent.*/
BasicTree::ChildrenIterator BasicTree::GetChildrenIterator(unsigned int flags,
														BasicTree::Entry* parent,
													   BasicTree::Entry* child,
													   bool is_first)
{
	Entry* actual_parent = parent;
	if (actual_parent == NULL)
	{
		actual_parent = m_root;
	}
	if (actual_parent == NULL)
	{	//OK, empty tree
		return ChildrenIterator();	//return invalid iterator.
	}
	if (child == NULL)
	{
		if (is_first)
		{
			child = actual_parent->m_first_child;
		} else {
			child = actual_parent->m_last_child;
		}
	}
	return ChildrenIterator(this, actual_parent, child, flags);
}

BasicTree::TopLevelIterator BasicTree::GetTopLevelIterator(bool is_begin, Entry* basic_entry)
{
	//find nearest top level
	if (basic_entry == NULL)
	{
		basic_entry = m_root;
	}
	ChildrenIterator ch_it = GetChildrenIterator(0, /*NULL*/basic_entry, NULL, is_begin);
	Entry* entry = NULL;
	while (ch_it.IsValid())
	{
		entry = ch_it.GetCurrentChild();
		ASSERT(entry != NULL);
		if (entry->GetChildrenCount() == 0)
		{
			break;
		}
		if (is_begin)
		{
			++ch_it;
		} else {
			--ch_it;
		}
	}
	//create ret_val
	TopLevelIterator ret_val;
	if (entry != NULL)
	{
		ret_val.m_it = ch_it;
		ret_val.SetValid(true);
	} else {
		ret_val.SetValid(false);
	}
	return ret_val;
}

//this method adds entry as a child to parent. entry may have it's own children.
BasicTree::Entry* BasicTree::AddEntry(BasicTree::Entry* entry, BasicTree::Entry* parent, BasicTree::Entry* child_before)
{
	if (entry == NULL)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_INSERT_NULL_ENTRY,
			L"Cannot insert a new entry to the tree because new entry == NULL",
			EXC_HERE);
	}
	if (parent == NULL)
	{
		if (m_root != NULL)
		{
			Throw<Exception>(UTILS_ERROR_CANNOT_INSERT_ROOT_ALREADY_IS_SET,
				L"Cannot insert a new entry because this was an attempt to set a root while root already exitts",
				EXC_HERE);
		} else {
			m_root = entry;
			entry->m_parent = NULL;
		}
	} else {
		bool ok = parent->AddChild(entry, child_before);
		ASSERT(ok == true);
	}
	return entry;
}

//this method just cuts off the branch
BasicTree::Entry* BasicTree::RemoveEntry(BasicTree::Entry* entry)
{
	if (entry == NULL)
	{
		Throw<Exception>(UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
			L"Cannot remove a child entry from this entry because entry == NULL",
			EXC_HERE);
	}
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForWrite() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE,
				L"Cannot get lock for write",
				EXC_HERE);
		}
	}*/
	WriteSynchronizer sync(m_rw_lock);
	BasicTree::Entry* ret_val = NULL;
	if (entry == m_root)
	{
		ASSERT(entry->GetParent() == NULL);
		ASSERT(entry->m_prev_sibling == NULL);
		ASSERT(entry->m_next_sibling == NULL);
		m_root = NULL;
		ret_val = entry;
	}
	else {
		Entry* parent = entry->GetParent();
		ASSERT(parent != NULL);
		ret_val = parent->RemoveChild(entry);
	}
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return ret_val;
}

bool BasicTree::LockForRead()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForRead();
	} else {
		return true;
	}
}

bool BasicTree::LockForWrite()
{
	if (m_rw_lock != NULL)
	{
		return m_rw_lock->LockForWrite();
	} else {
		return true;
	}
}

void BasicTree::Unlock()
{
	if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}
}

bool BasicTree::ChildrenIterator::Advance(bool forward)
{
	enum
	{
		FIRST_CHILD,
		NEXT_CHILD,
		PARENT
	};
	ASSERT(m_tree != NULL);
	/*if (m_tree->m_rw_lock != NULL)
	{
		if (m_tree->m_rw_lock->LockForRead() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
				L"cannot lock the tree for read",
				EXC_HERE);
		}
	}*/
	ReadSynchronizer sync(m_tree->m_rw_lock);
	bool from_above = false;
	if ((m_prev_entry != NULL) &&
		(m_prev_entry->m_parent == m_current_entry))
	{
		from_above = true;
	}
	//if came to m_current_entry from above
	if (from_above)
	{
		//	if m_current_entry has another children
		Entry* next_child = NULL;
		//note that here prev entry is the entry above, the child entry where iterator just returned from.
		if (forward)
		{
			if (m_flags & RETURN_TO_PARENTS)
			{
				ASSERT(m_prev_entry != NULL);
				next_child = m_prev_entry->m_next_sibling;
			} else {
				next_child = m_current_entry->m_next_sibling;
			}
		} else {
			if (m_flags & RETURN_TO_PARENTS)
			{
				ASSERT(m_prev_entry != NULL);
				next_child = m_prev_entry->m_prev_sibling;
			} else {
				next_child = m_current_entry->m_prev_sibling;
			}
		}
		//     goto next child
		if (next_child != NULL)
		{
			m_prev_entry = m_current_entry;
			m_current_entry = next_child;
			return true;
		} else {	//  else 
			//     if(return to parent)
			if (m_flags & RETURN_TO_PARENTS)
			{	//        return to parent
				if (m_current_entry->m_parent != NULL)
				{
					m_prev_entry = m_current_entry;
					m_current_entry = m_current_entry->m_parent;
					return true;
				} else {
					//exit
					return false;
				}
			} else {
				//     else find nearest grandparent with entries I havent seen yet
				Entry* next_unseen_child = NULL;
				Entry* grandparent = FindGrandparentWithNextUnseenChild(forward, &next_unseen_child);
				if (next_unseen_child != NULL)
				{	//    if found, goto grandparent's next child
					m_prev_entry = m_current_entry;
					m_current_entry = next_unseen_child;
					return true;
				}
				else {	//  else exit
					return false;
				}
			} 
		}
	}
	else {
		//else //from below, see current entry for the first time
		//  if m_current_entry has children
		Entry* next_unseen_child = NULL;
		if (forward)
		{
			if (m_current_entry->m_first_child != NULL)
			{
				next_unseen_child = m_current_entry->m_first_child;
			}
		} else {
			if (m_current_entry->m_last_child != NULL)
			{
				next_unseen_child = m_current_entry->m_last_child;
			}
		}
		if (next_unseen_child != NULL)
		{
			m_current_entry = next_unseen_child;
			return true;
		} else {
			if (m_flags & RETURN_TO_PARENTS)
			{
				m_prev_entry = m_current_entry;
				m_current_entry = m_current_entry->m_parent;
				return true;
			}
			else {
				//goto grandparent with next unseen child
				Entry* grandparent = FindGrandparentWithNextUnseenChild(forward, &next_unseen_child);
				if (next_unseen_child != NULL)
				{
					m_prev_entry = m_current_entry;
					m_current_entry = next_unseen_child;
					return true;
				}
				else {
					return false;
				}
			}
		}
	}
	/*if (m_tree->m_rw_lock != NULL)
	{
		m_tree->m_rw_lock->Unlock();
	}*/
}

BasicTree::Entry* BasicTree::ChildrenIterator::FindGrandparentWithNextUnseenChild(bool forward, Entry** unseen)
{	//no need for locking here, this is protected internal method. all locking is above.
	ASSERT(unseen != NULL);
	Entry* nearest_grandparent = m_current_entry->m_parent;
	Entry* child_in_current_entry_direction = m_current_entry;
	Entry* next_unseen_child = NULL;
	bool exit_flag = false;
	while (exit_flag == false)
	{
		if (nearest_grandparent == NULL)
		{//this was root
			return NULL;
		}
		//has unseen children?
		next_unseen_child = NULL;
		if (forward)
		{
			next_unseen_child = child_in_current_entry_direction->m_next_sibling;
		} else {
			next_unseen_child = child_in_current_entry_direction->m_prev_sibling;
		}
		if (next_unseen_child != NULL)
		{
			exit_flag = true;
		}
		child_in_current_entry_direction = nearest_grandparent;
		nearest_grandparent = nearest_grandparent->m_parent;
	}
	*unseen = next_unseen_child;
	return nearest_grandparent;
}

bool BasicTree::TopLevelIterator::Advance(bool forward)
{
	/*if (m_rw_lock != NULL)
	{
		if (m_rw_lock->LockForRead() == false)
		{
			throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
				L"cannot get lock for read",
				EXC_HERE);
		}
	}*/
	ReadSynchronizer sync(m_rw_lock);
	ASSERT(m_it.GetCurrentChild() != NULL);
	ASSERT(m_it.GetCurrentChild()->GetChildrenCount() == 0);	//because this is TOP LEVEL iterator
	Entry* next_top_level = NULL;
	//bool exit_flag = false;
	BasicTree::ChildrenIterator it = m_it;
	while (true)	//(exit_flag == false)
	{
		if (it.Advance(forward) == false)
		{
			return false;
		}
		Entry* entry = it.GetCurrentChild();
		if (entry->GetChildrenCount() == 0)	//ok, this is top level
		{
			m_it = it;
			return true;
		}
	}
	/*if (m_rw_lock != NULL)
	{
		m_rw_lock->Unlock();
	}*/
	return false;	//no more children found
}

//so far here. later move it to synchronization
//caller should come with its flags and wait while all they are set.
bool /*true - success, false - timeout*/atomic_wait_for_bitmask(std::atomic<unsigned int>& atomic_int, 
	unsigned int bitmask, 
	unsigned int bitmask_to_set, 
	time_t timeout = INFINITE)
{
	bool ret_val = false;
	bool exit_flag = false;
	time_t wait_start = std::time(nullptr);
	while (exit_flag == false)
	{
		/*unsigned int a_value = atomic_int.load();
		if ((a_value & bitmask) == 0)
		{
			atomic_int.store(bitmask_to_set);	//not much good, better atomic test and set probably
			*/
		//unsigned int a_value = atomic_int.fetch_and(bitmask);
		unsigned int a_value = atomic_int.fetch_or(bitmask);
		if(a_value & bitmask)
		{
			ret_val = true;
			exit_flag = true;
		}
		else {
			std::this_thread::yield();
			time_t now = std::time(nullptr);
			if ((now - wait_start) >= timeout)
			{
				ret_val = false;
				exit_flag = true;
			} //else wait on
		}
	}
	return ret_val;
}

void atomic_set_bitmask(std::atomic<unsigned int>& atomic_int, unsigned int bitmask)// , time_t timeout)
{
	std::atomic_fetch_or(&atomic_int, bitmask);
}

//clear the bits that set in bitmask to 1
void atomic_clear_bitmask(std::atomic<unsigned int>& atomic_int, unsigned int bitmask)
{
	std::atomic_fetch_and(&atomic_int, ~bitmask);
}

SyncTL::MichaelScottQueue::MichaelScottQueue():
	m_head(nullptr),
	m_tail(nullptr),
	m_count(0),
	m_lock(UNLOCKED)
{
}

/*not responsible for entries deallocation. caller allocated and should delete.*/
SyncTL::MichaelScottQueue::~MichaelScottQueue()
{
	//ok if head and tail are not null. probably the entries stored in this list
	//are used somewhere else too.
	//ASSERT(m_head == nullptr);
	//ASSERT(m_tail == nullptr);
}

//insert to tail
bool /*is success*/MichaelScottQueue::Push(SyncTL::MichaelScottQueue::Entry* entry, 
	time_t timeout,
	bool add_next)
{
	bool ret_val = false;
	if (atomic_wait_for_bitmask(m_lock, TAIL_LOCK | COUNT_LOCK, TAIL_LOCK | COUNT_LOCK, timeout))	//must be test and set
	{
		Entry* last = m_tail;
		//Entry* first = m_head;
		if (last != nullptr)
		{
			ASSERT(m_count != 0);
			last->m_next = entry;
			m_tail = entry;
			//m_head = entry;
		} else {
			ASSERT(m_tail == nullptr);
			ASSERT(m_count == 0);
			m_head = entry;
			//ehtry may have another entries linked to it via m_next.. well, let's allow that..
			Entry* new_tail = entry;
			if (add_next)
			{
				for (Entry* another_new_tail = new_tail->m_next;
					another_new_tail != NULL;
					another_new_tail = another_new_tail->m_next)
				{
					new_tail = another_new_tail;
				}
			}
			m_tail = entry;
		}
		++m_count;
		ret_val = true;
		atomic_clear_bitmask(m_lock, TAIL_LOCK | COUNT_LOCK);
	}
	return ret_val;
}

//remove from head
SyncTL::MichaelScottQueue::Entry* /*null if queue is empty*/SyncTL::MichaelScottQueue::Pop(time_t timeout)
{
	Entry* ret_val = nullptr;
	if (atomic_wait_for_bitmask(m_lock, HEAD_LOCK | COUNT_LOCK, HEAD_LOCK | COUNT_LOCK, timeout))	//must be test and set
	{
		ret_val = m_tail;
		switch (m_count)
		{
		case 0:
			break;
		case 1:
			ret_val = m_tail;
			ASSERT(ret_val->m_next == nullptr);
			ASSERT(m_head == m_tail);
			--m_count;
			m_head = nullptr;
			m_tail = nullptr;
			break;
		default:
			ret_val = m_head;
			Entry* next = ret_val->m_next;
			ASSERT(next != nullptr);
			m_head = next;
			if (next->m_next == nullptr)
			{
				//ASSERT(m_count == 1);
				m_tail = m_head;
			}
			--m_count;
		}
		atomic_clear_bitmask(m_lock, HEAD_LOCK | COUNT_LOCK);
	}
	return ret_val;
}
