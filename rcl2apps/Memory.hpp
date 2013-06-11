#pragma once

#include "Headers.hpp"

LPVOID WINAPIV memset(LPVOID dst,INT val,size_t count);
LPVOID WINAPIV memcpy(LPVOID dst,LPCVOID src,size_t count);

INT WINAPIV _purecall();
