#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

enum
{
	UTILS_ERROR_BASE = 1,
	CHESS_ERROR_BASE = 1000,
	GUI_ERROR_BASE = 2000,
	XML_ERROR_BASE = 3000,
	CHESS_TREE_ERROR_BASE = 4000,
	CHESS_PLAYER_ERROR_BASE = 5000,
	SYNCHRONIZATION_ERROR_BASE = 6000,
	THREADING_ERROR_BASE = 7000,
	TIMER_ERROR_BASE = 8000,
	UNIFORM_ALLOCATOR_ERROR_BASE = 9000,
	BASE_FOR_ANOTHER_ERROR_CLASSES = 10000
};

enum
{
	ERR_OK = 0,
	UNDEFINED_ERROR = -1
};

enum
{
	GUI_ERR_OK = 0,
	GUI_ERR_CANNOT_LOAD_IMAGE = GUI_ERROR_BASE + 1,
	GUI_ERR_NO_SKINS,
	GUI_ERR_INVALID_PARAMETERS,
	GUI_ERR_CANNOT_CREATE_MENU,
	GUI_ERR_CANCELLED_BY_USER,
	GUI_ERR_TREE_DEPTH_SPECIFIED_IS_TOO_LOW,
	GUI_ERR_TREE_DEPTH_SPECIFIED_IS_TOO_HIGH,
	GUI_ERR_TREE_DEPTH_INVALID,
	GUI_ERR_INVALID_URL
};

/*enum
{
	UI_ERR_OK = 0,
	UI_ERR_CANNOT_LOAD_IMAGE = GUI_ERROR_BASE,
	UI_ERR_CANNOT_CREATE_MENU
};*/

enum
{
	UTILS_ERROR_UNSUPPORTED_INT_SIZE = UTILS_ERROR_BASE + 1,
	UTILS_ERROR_UNDEFINED_ERROR = -1,
	UTILS_ERROR_STD_EXCEPTION
};

enum
{
	ERROR_MSG_BUFFER_LENGTH = 0x7F
};

#define COMA	,

#ifdef _DEBUG
#define CH(c)	c
#else
#define CH(c)
#endif

void debug_break();
#ifdef _DEBUG
#define ASSERT(condition)	\
if (condition != true)	\
	{						\
	debug_break();		\
	}
#else
#define ASSERT(condition)
#endif //DEBUG

#ifndef NULL
#define NULL 0//((void*)0)
#endif//

//built-in "int" size depends on architecture.
#define BIT_SIZEOF_INT	sizeof(int) << 3

template <typename CharType>
unsigned int TStrLen(CharType* str)
{
	unsigned int ret_val(0);
	while (*str != NULL)
	{
		++ret_val;
	}
	return ret_val;
}

//unsigned int /*copied size*/ ByteCopy(const char* src, char* dst, unsigned int size);

inline void _Yield()
{
	//SwitchToThread();
#ifdef WINDOWS
	SwitchToThread();
#elif (POSIX)
	sched_yield();
#endif
}

#endif //UTILS_H_INCLUDED