#ifndef COLLECTIONS_H_INCLUDED
#define COLLECTIONS_H_INCLUDED

#pragma warning (disable: 4100)

#include "ErrorReporting.h"
#include "Utils.h"
#include "Synchronization.h"

//Here is collections similar to those in Qt or STL. I decieded not to use any side collections in chess core
//so it is independent to anything.

namespace SyncTL
{

enum
{
	UTILS_ERROR_OK = 0,
	UTILS_ERROR_NOT_IN_COLLECTION = UTILS_ERROR_BASE + 1,
	UTILS_ERROR_INVALID_PARAMETERS,
	UTILS_ERROR_NULL_ITERATOR,
	UTILS_ERROR_ITERATOR_REACHED_END,
	UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
	UTILS_ERROR_NO_ALLOCATOR,
	UTILS_ERROR_NO_ALLOCATED_MEMORY,
	UTILS_ERROR_CANNOT_INSERT_ALREADY_INSERTED,
	UTILS_ERROR_CANNOT_INSERT_INVALID_ITERATOR,
	UTILS_ERROR_CANNOT_REMOVE_NOT_IN_COLLECTION,
	UTILS_ERROR_CANNOT_INSERT_NULL_ENTRY,
	UTILS_ERROR_CANNOT_INSERT_ROOT_ALREADY_IS_SET,
	UTILS_ERROR_CANNOT_INSERT_INVALID_PARENT
};

class BasicVector
{
public:
	class Allocator
	{
	public:
		//this methods may throw exceptions.
		virtual char* AllocateDataArray(unsigned int entry_size, unsigned int count) = 0;
		virtual void FreeDataArray(char* data_array) = 0;
	};

	class DefaultAllocator: public Allocator
	{
	public:
		virtual char* AllocateDataArray(unsigned int entry_size, unsigned int count);
		virtual void FreeDataArray(char* data_array);
	};

	static Allocator* GetDefaultAllocator()
	{
		static DefaultAllocator def_allocator;
		return &def_allocator;
	}

	class Iterator
	{
	public:
		Iterator(unsigned int index = 0, 
			BasicVector* vector = NULL);
		virtual ~Iterator();
		Iterator operator ++ ();
		Iterator& operator ++ (int);
		Iterator operator -- ();
		Iterator& operator -- (int);
		Iterator& operator = (const BasicVector::Iterator& it);
		bool operator == (const BasicVector::Iterator& another) const
			{ return ((m_vector== another.m_vector) && (m_index == another.m_index)); }
		bool operator != (const BasicVector::Iterator& another) const
			{ return ((m_vector != another.m_vector) || (m_index != another.m_index)); }
		char* Data() const
			{ return m_vector->m_data + (m_index * m_vector->m_entry_size); }
		bool IsValid() const;
		unsigned int GetIndex() const
			{ return m_index; }
	protected:
		BasicVector* m_vector;
		unsigned int m_index;
	};
	//this constructor creates a vector that is unusable until user will pass entry size and allocator to the vector.
	//this is needed for BasicTree::iterator whilch internally uses stack.
	//if caller created a vector with this constructor, then caller must call Init method.
	BasicVector(BasicReadWriteLock* lock = NULL);
	BasicVector(const BasicVector& another);
	//this constructor calls Init internally.
	BasicVector(unsigned int entry_size, 
			    unsigned int n_preallocated, 
				Allocator* allocator = GetDefaultAllocator(), 
				BasicReadWriteLock* lock = NULL);
	virtual ~BasicVector();
	//void Init(unsigned int entry_size, unsigned int n_preallocated, Allocator* allocator, BasicReadWriteLock* lock = NULL);
	void ResizeDataArray(unsigned int n_entries);
	//here it is assumed that data size is equal to m_entry_size. if it is not, it is up to caller.
	unsigned int GetEntrySize() const
		{ return m_entry_size; }
	void GetEntry(unsigned int index, char** out_entry) const;
	char* /*ptr to data in vector*/ InsertEntry(unsigned int index, const char* data);
	void RemoveEntry(unsigned int index);
	void RemoveEntry(Iterator& it)
		{ RemoveEntry(it.GetIndex()); }
	void Clear();
	char* operator [] (unsigned int index) const
		{
			char* ret_val = NULL;
			GetEntry(index, &ret_val);
			return ret_val;
		}
	BasicVector& operator = (const BasicVector& another);
	unsigned int GetCount() const
		{ return m_count; }
	Iterator Begin();
	//no End(), instead use Iterator::IsValid().
	Iterator Last();	//returns iterator to the last element.
	bool LockForRead();
	bool LockForWrite();
	void Unlock();
	inline void SetLock(BasicReadWriteLock* lock)
		{ m_rw_lock = lock;	}
	inline BasicReadWriteLock* GetLock() const
		{ return m_rw_lock;	}
protected:
	//this CopyEntry implementation does just byte copy of one entry to another.
	//descendants may reimplement this method in order to call copy constructors
	virtual void CopyEntry(const char* src, char* dst);
	//this method is a placeholder for destructor call. in thes implementation it does nothing
	//as long as this class treats stored data as just an array of bytes.
	virtual void DeinitEntry(char* entry);
	const unsigned int m_allocator_increment = 4;//debug only, will be 64;	//in terms of entries, not bytes
	unsigned int m_entry_size;
	unsigned int m_data_array_size;	//in terms of entries, not bytes.
	char* m_data;
	unsigned int m_count;
	Allocator* m_allocator;
	BasicReadWriteLock* m_rw_lock;
};

template <class DataType>
class Vector: public BasicVector
{
public:
	class Iterator : public BasicVector::Iterator
	{
	public:
		Iterator(unsigned int index = 0,
				 BasicVector* vector = NULL) :
				 BasicVector::Iterator(index, vector)
		{}
		operator DataType*()
		{
			char* ret_val = Data();
			return reinterpret_cast<DataType*>(ret_val);
		}
	};
	enum
	{
		DEFAULT_PREALLOCATED = 0xff
	};
	Vector(BasicReadWriteLock* lock = NULL):
		BasicVector(lock)
		{}
	#define VECTOR_DEFAULT_PARAMS	0xff, BasicVector::GetDefaultAllocator(), (BasicReadWriteLock*)NULL
	Vector(unsigned int n_preallocated, Allocator* allocator, BasicReadWriteLock* lock) :
		BasicVector(sizeof(DataType), n_preallocated, allocator, lock)
		{}
	//this methods throw exceptions
	DataType& operator [] (unsigned int index) const
	{
		char* data = BasicVector::operator[](index);
		if(data != NULL)
		{
			return reinterpret_cast<DataType&>(*data);
		} else {
			Throw<Exception>(UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
				L"Cannot get vector entry because index is bigger than vetcor size",
				EXC_HERE);
			
		}
	}
	void Insert(unsigned int index, const DataType* data)
		{ BasicVector::InsertEntry(index, (const char*)data); }
	//
	Iterator Begin()
		{ return Iterator(0, this);	}
	Iterator Last()
	{
		if (m_count == 0)
		{
			return Iterator(0, this);
		}
		return Iterator(m_count - 1, this);
	}
	Iterator /*iterator to just inserted entry*/ InsertEntry(unsigned int index, const DataType& entry)
	{
		BasicVector::InsertEntry(index, (char*)&entry);
		return Iterator(index, this);
	}
	Iterator PushFront(const DataType& data)
	{
		BasicVector::InsertEntry(0, (char*)&data);
		return Iterator(0, this);
	}
	Iterator PushBack(const DataType& data)
	{
		unsigned int index = GetCount();
		BasicVector::InsertEntry(index, (char*)&data);
		return Iterator(index, this);
	}
	void PopFront()
		{ RemoveEntry(0); }
	void PopBack()
		{ RemoveEntry(GetCount() - 1); }
	DataType* Front() const
	{
		if (m_count == NULL)
		{
			return NULL;
		} else {
			DataType* ret_val = NULL;
			GetEntry(0, (char**)&ret_val);
			return ret_val;
		}
	}
	DataType* Back() const
	{
		if (m_count == NULL)
		{
			return NULL;
		} else {
			DataType* ret_val = NULL;
			GetEntry(m_count - 1, (char**)(&ret_val));
			return ret_val;
		}
	}
protected:
	void CopyEntry(const char* src, char* dst)
	{
		DataType* dt_dst = reinterpret_cast<DataType*>(dst);
		const DataType* dt_src = reinterpret_cast<const DataType*>(src);
		//const DataType* dt_src = reinterpret_cast<const DataType*>(src);
		//(*dt_dst) = (DataType(*dt_src));
		new (dt_dst)DataType(*(const_cast<DataType*>(dt_src)));
	}
	void DeinitEntry(char* entry)
	{
		((DataType*)entry)->~DataType();
	}
};

template <class CharType>
class String : public Vector<CharType>
{
	typedef Vector<CharType> BaseClass;
public:
	#define STRING_DEFAULT_PARAMS BasicVector::GetDefaultAllocator(), NULL
	String(const CharType* str, BaseClass::Allocator* allocator, BasicReadWriteLock* lock):
		Vector<CharType>::Vector(lock)
	{
		unsigned int length = TStrLen(str);
		unsigned int index = 0;
		while (index < length)
		{
			BaseClass::PushBack(str[index]);
			++index;
		}
	}
	const CharType* GetCString() const
	{
		return m_data; 
	}
protected:
	CharType* m_data;
};

typedef String<wchar_t>  WString;
typedef String<char>	 AString;

class BasicStack : public BasicVector
{
public:
	BasicStack(BasicReadWriteLock* lock):
		BasicVector(lock)
	{}
	BasicStack(unsigned int entry_size, unsigned int n_preallocated, Allocator* allocator, BasicReadWriteLock* lock = NULL) :
		BasicVector(entry_size, n_preallocated, allocator, lock)
		{}
	void PushFront(const char* data);
	void PushBack(const char* data);
	void PopFront();
	void PopBack();
	char* Top();
};

template <class DataType>
class Stack : public BasicStack
{
public:
	class Iterator : public BasicStack::Iterator
	{
	public:
		Iterator(unsigned int index, Stack* stack):
			BasicStack::Iterator(index, stack)
		{}
		Iterator(const BasicVector::Iterator& src):
			BasicStack::Iterator(src)
		{}
		operator DataType*()
		{
			char* ret_val = Data();
			return reinterpret_cast<DataType*>(ret_val);
		}
	};

	Stack(BasicReadWriteLock* lock = NULL):
		BasicStack(lock)
	{}
	Stack(unsigned int n_preallocated, Allocator* allocator, BasicReadWriteLock* lock = NULL) :
		BasicStack(sizeof(DataType), n_preallocated, allocator, lock)
	{}
	void PushFront(const DataType& data)
		{ BasicStack::PushFront((const char*)&data); }
	void PushBack(const DataType& data)
		{ BasicStack::PushBack((const char*)&data); }
	//pop front and back are in BasicStack, no need to overload them here.
	DataType& GetTop()
	{
		DataType* top = (DataType*)(Top());
		if (top != NULL)
		{
			DataType& ret_val = *top;
			return ret_val;
		}
		else {
			ASSERT(GetCount() == 0);
			return *((DataType*)NULL);
		}
	}
	DataType& operator [] (unsigned int index)
	{
		DataType* ptr = NULL;
		if (index < m_count)
		{
			ptr = (DataType*)(m_data + (index * m_entry_size));
		}
		else {
			Throw<Exception>(UTILS_ERROR_INDEX_BIGGER_THAN_ARRAY_SIZE,
				L"Cannot access an element in stack, index >= m_count",
				EXC_HERE);
		}
		return *ptr;
	}
};

/*BasicList is not responsible for memory management for it's entries. entries are created and destroyed by the caller.*/
class BasicList
{
public:
	class Entry
	{
		friend class BasicList;
	public:
		Entry() :
			m_prev(NULL),
			m_next(NULL),
			m_list(NULL)
		{}
		virtual ~Entry()
		{
			if(m_list != NULL)
			{
				Remove();
			}
		}
		virtual void Remove();
		virtual BasicReadWriteLock* GetLock() { return nullptr; };
	protected:
		Entry* m_prev;
		Entry* m_next;
		BasicList* m_list;
	};

	template <class Lock = NoLock>
	class EntryWithLock : public Entry
	{
	public:
		//probably some stuff for lock initialization ?
		virtual BasicReadWriteLock* GetLock() { return &m_lock; }
	protected:
		Lock m_lock;
	};

	class Iterator
	{
		friend class BasicList;
	public:
		Iterator(Entry* entry = nullptr,
			/*do i still need to pass lock here?
			if rw_lock is not passed, use the lock in entry. otherwise use this rw_lock.*/
			BasicReadWriteLock* rw_lock = nullptr):
			m_entry(entry),
			m_rw_lock(rw_lock)
		{}
		bool operator == (const Iterator& another) const
			{ return (m_entry == another.m_entry); }
		bool operator != (const Iterator& another) const
			{ return (m_entry != another.m_entry); }
		operator Entry* ()
			{ return m_entry; }
		Iterator& operator ++ ()
		{
			Advance(true);
			return *this;
		}
		Iterator operator ++ (int i)
		{
			Iterator ret_val = *this;
			Advance(true);
			return ret_val;
		}
		Iterator& operator -- ()
		{
			Advance(false);
			return *this;
		}
		Iterator operator -- (int i)
		{
			Iterator ret_val = *this;
			Advance(false);
			return ret_val;
		}
		bool IsValid() const
		{
			return (m_entry != NULL);
		}
	protected:
		//this method throws exceptions
		void Advance(bool forward);
		Entry* m_entry;
		BasicReadWriteLock* m_rw_lock;
	};

	BasicList(BasicReadWriteLock* lock = NULL);
	virtual ~BasicList();
	inline unsigned int GetCount() const
		{ return m_count; }
	//all addition methods return iterator to the just added element.
	inline Iterator Insert(Entry* entry, Iterator& before_here)
	{
		Entry* next = before_here.m_entry;
		return InternalInsert(entry, next);
	}
	inline Iterator PushFront(Entry* entry)
	{
		return InternalInsert(entry, m_head);
	}
	inline Iterator PushBack(Entry* entry)
	{
		return InternalInsert(entry, NULL/*m_last*/);
	}
	//this methods return just removed entry (those passed in parameter).
	inline Entry* Remove(Entry* entry)
	{
		return InternalRemove(entry);
	}
	inline Entry* Remove(Iterator& it)
	{
		Entry* entry = it.m_entry;
		return InternalRemove(entry);
	}
	inline Entry* PopFront()
	{
		if(GetCount() == 0)
		{
			return NULL;
		}
		Entry* ret_val = m_head;
		InternalRemove(m_head);
		return ret_val;
	}
	inline Entry* PopBack()
	{
		if(GetCount() == 0)
		{
			return NULL;
		}
		Entry* ret_val = m_last;
		return InternalRemove(m_last); 
		return ret_val;
	}
	inline bool IsEmpty() const
		{ return GetCount() == 0; }
	inline Iterator Begin()
	{
		if (m_head != NULL)
		{
			return Iterator(m_head);
		}
		else {
			return Iterator();	//invalid, it's ok.
		}
	}
	inline void Clear()
	{
		while (m_count != 0)
		{
			PopBack();
		}
	}
	bool LockForRead();
	bool LockForWrite();
	void Unlock();
	void SetLock(BasicReadWriteLock* lock)
		{ m_rw_lock = lock;	}
protected:
	//returns pointer to the entry in the list
	Entry* InternalInsert(Entry* entry, Entry* before_this_entry /*may be NULL, this means PushBack*/);
	//returns pointer to the entry next to the just removed entry or NULL if removed entry was the last.
	Entry* InternalRemove(Entry* entry);

	//these must be changed quickly, taking a lock for a short time
	//these must be protected with atomic
	Entry* m_head;
	Entry* m_last;	//this member is added here due to optimization reasons so PushBack to work faster.
	unsigned int m_count;
	BasicReadWriteLock* m_rw_lock;
	//AtomicReadWriteLock* m_rw_lock;
};

template <class DataType>
class ListEntry : public BasicList::Entry
{
public:
	ListEntry(const DataType& data):
		m_data(data)
	{}
	ListEntry(DataType&& data)  :
		m_data(std::move(data))
	{
		//std::move();
	}
	Entry& operator = (const DataType& data)
		{ m_data = data; }
	Entry& operator = (const ListEntry& another)
		{ m_data = another.m_data; }
	Entry& operator = (ListEntry&& another)
		{ m_data = std::move(another.m_data); }
	operator DataType& ()
		{ return GetData(); }
	DataType& GetData()
		{ return m_data; }
protected:
	DataType m_data;
};

template <class DataType>
class List : public BasicList
{
public:
	class Iterator;
	typedef ListEntry<DataType> Entry;

	class Iterator : public BasicList::Iterator
	{
	public:
		Iterator(Entry* entry = NULL):
			BasicList::Iterator(entry)
			{}
		Iterator(const BasicList::Iterator& src):
			BasicList::Iterator(src)
			{}
		operator DataType* () const
		{
			if (m_entry == NULL)
			{
				return NULL;
			}
			Entry* entry = reinterpret_cast<Entry*>(m_entry);
			if (entry != NULL)
			{
				return &(entry->GetData());
			}
			return NULL;
		}
		DataType* GetData() const
		{
			if (m_entry == NULL)
			{
				return NULL;
			}
			Entry* entry = reinterpret_cast<Entry*>(m_entry);
			if (entry != NULL)
			{
				return &(entry->GetData());
			}
			return NULL;
		}
	};

	List(BasicReadWriteLock* lock = NULL) :
		BasicList(lock)
	{}
};

class BasicTree
{
public:
	//this allocator is needed here because tree itself is not responsible for memory and is not going on to
	//manage memory for tree iterator (it needs to keep the stack of parent entries). Allocator is the way to
	//delegate memory management to BasicTree user.
	class Allocator: public BasicStack::Allocator
	{
	public:
		Allocator(unsigned int stack_frame_size, unsigned int entry_size) :
			m_stack_frame_size(stack_frame_size),
			m_entry_size(entry_size)
		{}
		virtual char* AllocateDataArray(unsigned int entry_size, unsigned int count) = 0;
		virtual void FreeDataArray(char* data_array) = 0;
		virtual void* AllocateStackFrame() = 0;
		virtual void* AllocateEntry() = 0;
		virtual void FreeStackFrame(void* frame) = 0;
		virtual void FreeEntry(void* entry) = 0;
	protected:
		unsigned int m_stack_frame_size;
		unsigned int m_entry_size;
	};

	class ChildrenIterator;
	class Entry
	{
	public:
		friend class BasicTree::ChildrenIterator;
		friend class BasicTree;
		class ChildrenIterator
		{
		public:
			ChildrenIterator(const Entry* parent = NULL, const Entry* child = NULL) :
				m_parent((Entry*)parent),
				m_child((Entry*)child),
				m_rw_lock(nullptr)
			{}
			bool IsValid() const
				{ return ((m_parent != NULL) && (m_child != NULL));	}
			Entry* GetParent() const
				{ return m_parent; }
			Entry* GetChild() const
				{ return m_child; }
			bool Advance(bool forward);
			ChildrenIterator& operator ++ ()
			{
				if (Advance(true) == false)
				{
					m_child = NULL;	//this makes iterator invalid;
				}
				return *this;
			}
			ChildrenIterator operator ++ (int)
			{
				ChildrenIterator ret_val = *this;
				if (Advance(false) == false)
				{
					m_child = NULL;
				}
				return ret_val;
			}
			ChildrenIterator operator -- ()
			{
				if (Advance(false) == false)
				{
					m_child = NULL;
				}
				return *this;
			}
			ChildrenIterator operator -- (int)
			{
				ChildrenIterator ret_val = *this;
				if (Advance(false) == false)
				{
					m_child = NULL;
				}
				return ret_val;
			}
		protected:
			Entry* m_parent;
			Entry* m_child;
			BasicReadWriteLock* m_rw_lock;	//this is those lock that belongs to the tree
		};

		Entry() :
			m_parent(NULL),
			m_first_child(NULL),
			m_last_child(NULL),
			m_children_count(0),
			m_prev_sibling(NULL),
			m_next_sibling(NULL)
		{}
		//destructor destroys all children as well.
		virtual ~Entry();
		unsigned int GetChildrenCount() const
			{ return m_children_count; }
		Entry* GetParent() const
			{ return m_parent; }
		/*child_before is a child entry before which a new_child will be inserted. may be NULL, 
		then new entry is pushed back to the end of the children list.*/
		bool AddChild(Entry* new_child, Entry* child_before = NULL);
		Entry* /*next child*/ RemoveChild(Entry* child);
		ChildrenIterator Begin() const
			{ return ChildrenIterator(this, m_first_child);	}
		Entry* GetNextSibling() const
			{ return m_next_sibling; }
		Entry* GetPrevSibling() const
			{ return m_prev_sibling; }
		Entry* GetFirstChild() const
			{ return m_first_child;	}
		Entry* GetLastChild() const
			{ return m_last_child; }
		//sibling setters are for internal use
		void SetPrevSibling(Entry* e)
			{ m_prev_sibling = e; }
		void SetNextSibling(Entry* e)
			{ m_next_sibling = e; }
	protected:
		Entry* m_parent;
		Entry* m_first_child;
		Entry* m_last_child;
		unsigned int m_children_count;
		//double linked list of children
		Entry* m_prev_sibling;
		Entry* m_next_sibling;
	};

	//this iterator moves through children and their children recursively. 
	//(no real recursion e.g. calling fuction from the same function, is here)
	//NOTE: on tree modification iterator may become unusable, so don't store it for a long time.
	//use it only as a temporary object on stack.
	class ChildrenIterator
	{
	public:
		friend class BasicTree;
		enum
		{
			INVALID = 0x1,
			RETURN_TO_PARENTS = 0x2		//this means when iterator traverses the tree it goes throuh an entry (parent)
										//and then goes to its children. when this flag is set, iterator returns to parent
										//entry again after it passed through all children.
		};
		const unsigned int m_n_preallocated = 64;
		//all parameters here have default values == NULL so that iterator can be created as a local variable 
		//on stack an then be assigned to real iterator via GetChildrenIterator.
		ChildrenIterator(BasicTree* tree = NULL,
						 Entry* parent = NULL,
						 Entry* child = NULL /*if child == NULL, then first child*/,
						 unsigned int flags = 0):
						 m_tree(tree),
						 m_current_entry(NULL),
						 m_prev_entry(NULL),
						 m_flags(flags)
		{
			if ((m_tree == NULL) || (parent == NULL))
			{
				m_flags |= INVALID;
			}
			Entry* actual_child = child;
			if (actual_child == NULL)
			{
				if (parent != NULL)
				{
					actual_child = parent->m_first_child;
				}
			}
			if (actual_child == NULL)
			{
				m_flags |= INVALID;
			}
			m_current_entry = actual_child;
		}
		ChildrenIterator(const ChildrenIterator& another):
			m_tree(another.m_tree),
			m_current_entry(another.m_current_entry),
			m_prev_entry(another.m_prev_entry),
			m_flags(another.m_flags)
		{
			if (m_tree == NULL)
			{
				m_flags |= INVALID;
			}

		}
		virtual ~ChildrenIterator()
		{}
		ChildrenIterator& operator = (const ChildrenIterator& another)
		{
			//bool unlock_this(false);
			//bool unlock_another(false);
			//unsigned int error = UTILS_ERROR_OK;
			//const wchar_t* msg = NULL;
			BasicReadWriteLock* this_rw_lock = NULL;
			BasicReadWriteLock* another_rw_lock = NULL;
			if (another.m_tree != NULL)
			{
				another_rw_lock = another.m_tree->m_rw_lock;
			}
			if (m_tree != NULL)
			{
				this_rw_lock = m_tree->m_rw_lock;
			}
			ReadSynchronizer read_synchronizer(another_rw_lock);
			WriteSynchronizer write_synchronizer(this_rw_lock);
			/*if ((another.m_tree != NULL) && (another.m_tree->m_rw_lock != NULL))
			{
				if (another.m_tree->m_rw_lock->LockForRead())
				{
					unlock_another = true;
				} else {
					error = SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ;
					msg = L"cannot lock source tree for read";
				}
			}
			if ((m_tree != NULL) && (m_tree->m_rw_lock != NULL))
			{
				if (m_tree->m_rw_lock->LockForWrite())
				{
					unlock_this = true;
				} else {
					error = SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_WRITE;
					msg = L"cannot lock destination tree for write";
				}
			}*/
			m_tree = another.m_tree;
			m_current_entry = another.m_current_entry;
			m_prev_entry = another.m_prev_entry;
			m_flags = another.m_flags;
			if (m_tree == NULL)
			{
				m_flags |= INVALID;
			}
			return *this;
			/*if (unlock_this)
			{
				if (m_tree != NULL)
				{
					ASSERT(m_tree->m_rw_lock != NULL);
					m_tree->m_rw_lock->Unlock();
				}
			}
			if (unlock_another)
			{
				if (another.m_tree != NULL)
				{
					ASSERT(another.m_tree->m_rw_lock != NULL);
					another.m_tree->m_rw_lock->Unlock();
				}
			}*/
			/*if (error != UTILS_ERROR_OK)
			{
				ASSERT(msg != NULL);
				throw Exception(error, msg, EXC_HERE);
			}*/
		}
		bool IsValid() const
			{ return ((m_flags & INVALID) == 0); }
		bool Advance(bool forward = true);
		ChildrenIterator& operator ++ ()
		{
			if (Advance(true) == false)
			{
				m_flags |= INVALID;
			}
			else {
				m_flags &= (~INVALID);
			}
			return *this;
		}
		ChildrenIterator operator ++ (int i)
		{
			ChildrenIterator ret_val = *this;
			if (Advance(true) == false)
			{
				m_flags |= INVALID;
			} else {
				m_flags &= (~INVALID);
			}
			return ret_val;
		}
		ChildrenIterator& operator -- ()
		{
			if (Advance(false) == false)
			{
				m_flags |= INVALID;
			} else {
				m_flags &= (~INVALID);
			}
			return *this;
		}
		ChildrenIterator operator -- (int i)
		{
			ChildrenIterator ret_val = *this;
			if (Advance(false) == false)
			{
				m_flags |= INVALID;
			} else {
				m_flags &= (~INVALID);
			}
			return ret_val;
		}
		Entry* GetParent() const
		{
			return m_current_entry->GetParent();
		}
		Entry* GetCurrentChild() const
		{
			return m_current_entry;
		}
		unsigned int GetFlags() const
		{
			return m_flags; 
		}
		bool GetFlag(unsigned int mask) const
		{
			return (m_flags & mask); 
		}
		void SetFlag(unsigned int mask, bool value)
		{
			if (value)
			{
				m_flags |= mask;
			} else {
				m_flags &= (~mask);
			}
		}
	protected:
		Entry* FindGrandparentWithNextUnseenChild(bool forward, Entry** unseen);

		BasicTree* m_tree;
		Entry* m_current_entry;
		Entry* m_prev_entry;
		unsigned int m_flags;
	};

	//this is iterator to iterate througn tree leaves, not coming back to parent entries
	class TopLevelIterator
	{
		friend class BasicTree;
		enum
		{
			INVALID = 1
		};
	public:
		TopLevelIterator():
			m_flags(0),
			m_rw_lock(NULL)
			{}
		TopLevelIterator(const ChildrenIterator& ch_it) :
			m_it(ch_it),
			m_flags(0),
			m_rw_lock(NULL)
			{
				if (ch_it.m_tree != NULL)
				{
					m_rw_lock = ch_it.m_tree->m_rw_lock;
				}
			}
		/*virtual ~TopLevelIterator()
		{}*/
		bool Advance(bool forward);
		TopLevelIterator& operator ++ ()
		{
			/*if (m_rw_lock != NULL)
			{
				if (m_rw_lock->TryLockForRead() == false)
				{
					throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
						L"tree is locked for reading",
						EXC_HERE);
				}
			}*/
			TryWriteSynchronizer write_sync(m_rw_lock);
			if (Advance(true) == false)
			{
				m_flags |= INVALID;
			} else {
				m_flags &= (~INVALID);
			}
			return *this;
		}
		TopLevelIterator operator ++ (int)
		{
			/*if (m_rw_lock != NULL)
			{
				if (m_rw_lock->TryLockForRead() == false)
				{
					throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
						L"tree is locked for reading",
						EXC_HERE);
				}
			}*/
			TryReadSynchronizer sync(m_rw_lock);
			TopLevelIterator ret_val = *this;
			if(Advance(true) == false)
			{
				m_flags |= INVALID;
			}
			else {
				m_flags &= (~INVALID);
			}
			return ret_val;
		}
		TopLevelIterator& operator -- ()
		{
			/*if (m_rw_lock != NULL)
			{
				if (m_rw_lock->TryLockForRead() == false)
				{
					throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
						L"tree is locked for reading",
						EXC_HERE);
				}
			}*/
			TryReadSynchronizer sync(m_rw_lock);
			if (Advance(false) == false)
			{
				m_flags |= INVALID;
			}
			else {
				m_flags &= (~INVALID);
			}
			return *this;
		}
		TopLevelIterator operator -- (int)
		{
			/*if (m_rw_lock != NULL)
			{
				if (m_rw_lock->TryLockForRead() == false)
				{
					throw Exception(SYNCHRONIZATION_ERROR_CANNOT_GET_LOCK_FOR_READ,
						L"tree is locked for reading",
						EXC_HERE);
				}
			}*/
			TryReadSynchronizer sync(m_rw_lock);
			TopLevelIterator ret_val = *this;
			if(Advance(false) == false)
			{
				m_flags |= INVALID;
			}
			else {
				m_flags &= (~INVALID);
			}
			return ret_val;
		}
		bool IsValid() const
			{ return ((m_flags & INVALID) == 0); }
		Entry* GetEntry() const
			{ return m_it.GetCurrentChild(); }
	protected:
		inline void SetValid(bool is_valid)
		{
			if(is_valid)
			{
				m_flags &= (~INVALID);
			} else {
				m_flags |= INVALID;
			}
		}
		ChildrenIterator m_it;
		unsigned int m_flags;
		BasicReadWriteLock* m_rw_lock;	//this is those lock that belongs to the tree
	};

	BasicTree(BasicReadWriteLock* lock = NULL) :
		m_root(NULL),
		m_rw_lock(lock)
	{}
	/*if parent == NULL, then root is parent. if child == NULL, then the first child of parent.*/
	ChildrenIterator GetChildrenIterator(unsigned int flags = 0, 
										 Entry* parent = NULL, 
										 Entry* child = NULL,
										 bool is_first = true);
	//if basic_entry == NULL, then basic_entry is m_root
	TopLevelIterator GetTopLevelIterator(bool is_begin, Entry* basic_entry = NULL);
	//this method adds entry as a child to parent. entry may have it's own children.
	//return value is a just added entry;
	Entry* AddEntry(Entry* entry, Entry* parent, Entry* child_before = NULL);
	//this method just cuts off the branch
	//return value is a just removed entry
	Entry* RemoveEntry(Entry* entry);
	Entry* GetRoot()
		{ return m_root; }
	inline bool IsEmpty() const
		{ return (m_root == NULL); }
	bool LockForRead();
	bool LockForWrite();
	void Unlock();
protected:
	Entry* m_root;
	BasicReadWriteLock* m_rw_lock;
};

template <class DataType>
class Tree: public BasicTree
{
public:
	class Entry: public BasicTree::Entry
	{
		friend class Tree;
	public:
		//Entry()
		//{}
		Entry(const DataType& data):
			m_data(data)
		{}
		//virtual ~Entry();
		Entry& operator = (const DataType& data)
			{ m_data = data; }
		operator DataType& ()
			{ return m_data; }
		Entry* GetNextSibling() const
			{ return reinterpret_cast<Entry*>(m_next_sibling); }
	protected:
		DataType m_data;
	};

	/*Tree(BasicTree::Allocator* allocator) :
		BasicTree(allocator)
	{}*/
	//virtual ~Tree();
	Tree(BasicReadWriteLock* lock = NULL) :
		BasicTree(lock)
		{}
	Entry* GetRoot() const
	{
		if (m_root == NULL)
		{
			return NULL;
		}
		return reinterpret_cast<Entry*>(m_root); 
	}
	
protected:
	
};

class MichaelScottQueue
{
public:
	/*no iterators to be provided, this is lock-free quene to pass entries from thread to thread.
	note in order for atomic operations to work data must be aligned on machine word boundary.
	this queue does not store entries, as well as containers above.
	so it does not imply requirements like copy-constructible, default-constructible, e.t.c.
	this is very simplistic queue, not like stl containers or what, with iterators, e.t.c.*/
	class Entry
	{
	public:
		friend class MichaelScottQueue;
		virtual ~Entry()
		{}
	protected:
		Entry* m_next;
	};
	MichaelScottQueue();
	/*if list is not empty it is not treated as error since items
	are probably mentioned somewhere else and something else holds pointers to them.*/
	virtual ~MichaelScottQueue();
	//insert to tail, remove from head.
	//because this is a single-linked list and I cannot get tail's previous element.
	//like in grocery store, going straight to the queue head is misbehaviour.
	//ehtry may have another entries linked to it via m_next.. well, let's allow that.. (add_next)
	bool /*is success*/Push(Entry* entry, time_t timeout = INFINITE, bool add_next = false);
	Entry* /*null if queue is empty*/Pop(time_t timeout = INFINITE);
	unsigned int GetCount() const
		{ return m_count; }
	bool IsEmpty() const
		{ return GetCount() == 0; }
protected:
	/*lock-free, yes, and here i declare locks.
	* lock-free does not mean without synchronization.
	* there are 3 flavours of lock-free
	* 1. lock-free
	*	a guaranteed progress of at least one thread
	* 2. wait-free
	*	a guaranteed progress on all threads
	* 3. lockless
	*	not using locks. Michael-Scott queue would be an example, but sill some base struct is needed
	* for convenience, with head and tail pointers
	* regarding exceptions.
	* when it is about performancem exceptiosns are not the good idea. so here is a compromise variant.
	* exceptions are static and I throw pointers to excpetions.
	* - members as well as the whole structures must be machine-word-aligned.
	* otherwise access to them will not be atomic (single instruction and single CPU action on memory).
	* - exceptions are not to be thrown in push/pop methods. no need for try-catch.
	* structured exception handling there might be desirable due to page allocation exceptions
	* thrown by operting system.
	*/
	Entry* m_head;
	Entry* m_tail;
	//std::atomic<unsigned int> m_count;
	unsigned int m_count;
	//this is a lock for both head and tail. bit 0 - head, bit 1 - tail.
	enum
	{
		UNLOCKED = 0,
		HEAD_LOCK = 1,
		TAIL_LOCK = 2,
		COUNT_LOCK = 4
	};
	std::atomic<unsigned int> m_lock;
	//std::atomic_bool m_head_lock;
	//std::atomic_bool m_tail_lock;
};

//template class will be here. same as with those containers made for chess.

} //end namespace SyncTL

#endif //COLLECTIONS_H_INCLUDED