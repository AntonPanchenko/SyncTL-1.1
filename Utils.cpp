#include "pch.h"
//#include "Utils.h"
#ifdef BUILDING_CHESS_MAIN_MODULE
#include "../Engine/Chess.h"
#include "../Chess/ChessBoardUI.h"
#endif //BUILDING_CHESS_MAIN_MODULE
#include <Windows.h>
#ifdef QT_VERSION
#include <QMessageBox>
#include <QString>
#endif //QT_VERSION

#include <tchar.h>

//#define _ITERATOR_DEBUG_LEVEL	0

void debug_break()
{
	TCHAR my_string[] = _T("warlock");
	DebugBreak();
}



/*unsigned int copied size ByteCopy(const char* src, char* dst, unsigned int size)
{
	unsigned int bytes_left = size;
	bool to_upper_address = (dst > src);
	//note that int size depends on architecture;
	if (sizeof(int) == 8)
	{
	}
	else if (sizeof(int) == 4)
	{
	}
	else {
		Throw<Exception>(UTILS_ERROR_UNSUPPORTED_INT_SIZE,
			L"sizeof(int) != 8 and sizeof(int) != 4, ByteCopy cannot work here",
			EXC_HERE);
	}
	if (to_upper_address)
	{
		while (bytes_left != 0)
		{
			switch (bytes_left)
			{
			case 3:
			case 2:
			case 1:
			default:
				(int*)(*dst) = (int*)(*src);
			}
		}
	}
	else {
		while (bytes_left != 0)
		{
			switch (bytes_left)
			{
			case 3:
			case 2:
			case 1:
			default:
			}
		}
	}
}*/