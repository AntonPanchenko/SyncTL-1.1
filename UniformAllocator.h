#ifndef UNIFORM_ALLOCATOR_H
#define UNIFORM_ALLOCATOR_H

#include "Collections.h"
#include "Utils.h"
#include "Synchronization.h"

#pragma warning (disable:4291)	//because operator new takes a pointer to UniformAllocator
								//and the object allocated this way keeps that pointer.
								//so there is no need (by design) for symmetric operator delete.
								//UniformAllocator is called from overloaded operator delete,
								//see UNIFORM_OPERATOR_NEW and UNIFORM_OPERATOR_DELETE

namespace SyncTL
{

	class UniformAllocator
	{
	public:
		enum
		{
			DEFAULT_ARRAY_SIZE = 0xFF,
			INVALID_FREE_INDEX = -1
		};

		enum
		{
			ERR_CANNOT_ALLOC = UNIFORM_ALLOCATOR_ERROR_BASE + 1,
			ERR_CANNOT_FREE,
			ERR_CANNOT_CREATE_ALLOCATOR_ARRAY
		};

		UniformAllocator(unsigned int unit_size, unsigned int array_size = DEFAULT_ARRAY_SIZE);
		virtual ~UniformAllocator();
		virtual void* Alloc();	//can return NULL if it cannot allocate
		virtual void Free(void* addr);	//only when addr points at the start of allocated block
		virtual bool IsMyAllocation(void* addr) const;
		inline unsigned int GetUnitSize() const
		{
			return m_unit_size;
		}
	protected:
		class Allocation
		{
			static const unsigned int IS_BUSY = 0x1;
#ifdef _DEBUG
			const unsigned int mmark = 'MNWR';
#endif //_DEBUG
			//
		public:
			Allocation(char flags = 0) :
				m_flags(flags)
			{}
			inline bool GetIsFree() const
			{
				return ((m_flags & IS_BUSY) == 0);
			}
			inline void SetIsFree(bool is_free)
			{
				if (is_free)
				{
					m_flags &= (~IS_BUSY);
				}
				else {
					m_flags |= IS_BUSY;
				}
			}
			inline char* GetDataPtr() const
			{
				return (char*)(this + 1);
			}
			char m_flags;
		};

		class Array : public virtual BasicList::Entry
		{
		public:
			Array(unsigned int unit_size, unsigned int count);
			virtual ~Array();
			void* Alloc();
			void Free(void* addr);
			inline unsigned int GetUnitSize() const
			{
				return m_unit_size;
			}
			inline unsigned int GetCount() const
			{
				return m_count;
			}
			inline unsigned int GetFreeCount() const
			{
				return m_free_count;
			}
			inline bool IsMyAllocation(void* addr) const
			{
				ASSERT(addr != NULL);
				char* array_start = GetArrayStart();
				char* array_end = GetArrayEnd();
				if ((addr > array_start) && (addr < array_end))
				{
					return true;
				}
				return false;
			}
		protected:
			inline char* GetArrayStart() const
			{
				return(char*)this + sizeof(Array);
			}
			inline char* GetArrayEnd() const
			{
				return GetArrayStart() + (GetAbsoluteUnitSize() * m_count);
			}
			inline unsigned int GetAbsoluteUnitSize() const
			{
				return sizeof(Allocation) + m_unit_size;
			}
			inline Allocation* GetAllocation(unsigned int index) const
			{
				return (Allocation*)(GetArrayStart() + (index * GetAbsoluteUnitSize()));
			}
			inline Allocation* GetAllocation(char* addr) const
			{
				return (Allocation*)(addr - sizeof(Allocation));
			}
			inline void* GetArrayEntry(unsigned int index) const
			{
				return GetAllocation(index) + sizeof(Allocation);
			}
			unsigned int m_unit_size;
			unsigned int m_count;
			unsigned int m_free_count;
			unsigned int m_nearest_free_index;
		}; // and memory for allocation right after the end of array instance.

		Array* CreateArray();
		void DeleteArray(Array* array);

		BasicList m_array_list;
		unsigned int m_unit_size;
		unsigned int m_array_size;
		//CriticalSection m_cs;
		std::atomic_flag m_atomic_flag;
	};

	class AllocatorInfo
	{
	public:
		//m_ua is set from operator new in derived classes, AllocatorInfo consturctor is called later
		//there s no matter to touch m_ua here.
		UniformAllocator* GetUniformAllocator() const
		{
			return m_ua;
		}
		void SetUniformAllocator(UniformAllocator* ua)
		{
			m_ua = ua;
		}
		/*template <typename Class>
		void* operator new (size_t size, UniformAllocator* ua)
		{
			ASSERT(ua != NULL);
			Class* obj = ua->Alloc();
			if (obj == NULL)
			{
				Throw<Exception>(ERR_CANNOT_ALLOC, L"Uniform Allocator cannot allocate memory", EXC_HERE);
			}
		}
		template <typename Class>
		void operator delete(void* ptr)
		{
			ASSERT(ptr != NULL);
			Class* obj = (Class*)ptr;
			AllocatorInfo* ai = dynamic_cast<AllocatorInfo*>(obj);
			ASSERT(ai != NULL);
			ai->Free(obj);
		}*/
	protected:
		UniformAllocator* m_ua;
	};

#define UNIFORM_OPERATOR_NEW(ClassName)		\
	void* operator new (size_t size, UniformAllocator* ua)	\
	{										\
		ASSERT(ua != NULL);					\
		void* ret_val = ua->Alloc();		\
		if(ret_val == NULL) Throw<Exception>(UniformAllocator::ERR_CANNOT_ALLOC, L"UniformAllocator cannot allocate memory", EXC_HERE);	\
		ClassName* obj = (ClassName*)ret_val;	\
		AllocatorInfo* ai = dynamic_cast<AllocatorInfo*>(obj);	\
		ASSERT(ai != NULL);					\
		ai->SetUniformAllocator(ua);		\
		return ret_val;						\
	}

#define UNIFORM_OPERATOR_DELETE(ClassName)	\
	void operator delete (void* ptr)		\
	{										\
		ClassName* obj =  (ClassName*)ptr;	\
		AllocatorInfo* ai = dynamic_cast<AllocatorInfo*>(obj);	\
		ASSERT(ai != NULL);					\
		UniformAllocator* ua = ai->GetUniformAllocator();	\
		ASSERT(ua != NULL);					\
		ua->Free(ptr);						\
	}

#define UNIFORM_NEW_AND_DELETE(ClassName)	\
	UNIFORM_OPERATOR_NEW(ClassName)			\
	UNIFORM_OPERATOR_DELETE(ClassName)

	template <class DataType>	//note that DataType here must be inherited from AllocatorInfo. and AllocatorInfo must be the first inheritance, DataType* == AllocatorInfo*.
	class TemplateUniformAllocator : public UniformAllocator
	{
	public:
		TemplateUniformAllocator(unsigned int array_size = 0xFF) :
			UniformAllocator(sizeof(DataType), array_size)
		{}
		virtual ~TemplateUniformAllocator()
		{}
		void* Alloc()
		{
			DataType* ret_val = (DataType*)UniformAllocator::Alloc();
			AllocatorInfo* ai = (AllocatorInfo*)(ret_val);
			ai->SetUniformAllocator(this);
			return ret_val;
		}
		//void Free(void* addr);
	};

} //end namespace SyncTL

#endif //UNIFORM_ALLOCATOR_H