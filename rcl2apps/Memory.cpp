#include "Headers.hpp"


LPVOID WINAPIV memset(LPVOID dst,INT val, size_t count)
{
	LPVOID start=dst;
	while(count--)
	{
		*static_cast<PBYTE>(dst)=static_cast<BYTE>(val);
		dst=static_cast<PBYTE>(dst)+1;
	}
	return start;
}

LPVOID WINAPIV memcpy(LPVOID dst,LPCVOID src, size_t count)
{
	LPVOID ret=dst;
	while(count--)
	{
		*static_cast<BYTE*>(dst)=*static_cast<const BYTE*>(src);
		dst=static_cast<BYTE*>(dst)+1;
		src=static_cast<const BYTE*>(src)+1;
	}
	return ret;
}

int WINAPIV _purecall()
{
	return 0;
}
