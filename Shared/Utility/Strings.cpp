//
// The Epoch Language Project
// Shared Library Code
//
// Common library functions for string manipulation
//

#include "pch.h"
#include "Utility/Strings.h"

#include <iostream>
#include <vector>
#include <cstdlib>


//
// Convert a narrow string to a wide string
//
std::wstring widen(const std::string& str)
{
	const char* cstr = str.c_str();
	size_t len = str.length() + 1;
	size_t reqsize = 0;
	if(mbstowcs_s(&reqsize, NULL, 0, cstr, len) != 0)
		throw std::exception("Cannot widen string - invalid character detected");

	if(!reqsize)
		throw std::exception("Failed to widen string");

	std::vector<wchar_t> buffer(reqsize, 0);
	if(mbstowcs_s(NULL, &buffer[0], len, cstr, len) != 0)
		throw std::exception("Cannot widen string - invalid character detected");

	return std::wstring(buffer.begin(), buffer.end() - 1);
}

wchar_t widen(char c)
{
	wchar_t ret;
	mbtowc(&ret, &c, 1);
	return ret;
}


//
// Convert a wide string to a narrow string
//
std::string narrow(const std::wstring& str)
{
	const wchar_t* cstr = str.c_str();
	size_t len = str.length() + 1;
	size_t reqsize = 0;
	if(wcstombs_s(&reqsize, NULL, 0, cstr, len) != 0)
		throw std::exception("Cannot narrow string - invalid character detected");

	if(!reqsize)
		throw std::exception("Failed to narrow string");

	std::vector<Byte> buffer(reqsize, 0);
	if(wcstombs_s(NULL, &buffer[0], len, cstr, len) != 0)
		throw std::exception("Cannot narrow string - invalid character detected");

	return std::string(buffer.begin(), buffer.end() - 1);
}

char narrow(wchar_t c)
{
	char ret;
	wctomb_s(NULL, &ret, c, 1);
	return ret;
}


//
// Strip whitespace characters from the beginning and end of a string
//
std::wstring StripWhitespace(const std::wstring& str)
{
	const wchar_t* whitespacechars = L" \t\r\n";

	if(str.empty())
		return str;

	size_t pos = str.find_first_not_of(whitespacechars);
	size_t endpos = str.find_last_not_of(whitespacechars);

	return str.substr(pos, endpos - pos + 1);
}

