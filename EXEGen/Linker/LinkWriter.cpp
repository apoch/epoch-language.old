//
// The Epoch Language Project
// Win32 EXE Generator
//
// Wrapper object for emitting the final linked binary file
//

#include "pch.h"
#include "LinkWriter.h"


//-------------------------------------------------------------------------------
// LinkWriter implementation
//-------------------------------------------------------------------------------

//
// Construct and initialize the file writer wrapper
//
LinkWriter::LinkWriter(std::ofstream& outputstream)
	: OutputStream(outputstream)
{
}

//
// Write a single byte to the output file
//
void LinkWriter::EmitByte(unsigned char out)
{
	OutputStream << out;
}

//
// Write a WORD (16 bits) to the output file
//
void LinkWriter::EmitWORD(WORD out)
{
	OutputStream << static_cast<unsigned char>(out & 0xff)
					<< static_cast<unsigned char>((out >> 8) & 0xff);
}

//
// Write a DWORD (32 bits) to the output file
//
void LinkWriter::EmitDWORD(DWORD out)
{
	OutputStream << static_cast<unsigned char>(out & 0xff)
					<< static_cast<unsigned char>((out >> 8) & 0xff)
					<< static_cast<unsigned char>((out >> 16) & 0xff)
					<< static_cast<unsigned char>((out >> 24) & 0xff);
}

//
// Write a narrow (ASCII) string to the output file
//
void LinkWriter::EmitNarrowString(const std::string& out)
{
	OutputStream << out << '\0';
}

//
// Write a wide (UTF-16) string to the output file
//
void LinkWriter::EmitWideString(const std::wstring& out)
{
	for(std::wstring::const_iterator iter = out.begin(); iter != out.end(); ++iter)
		EmitWORD(*iter);
	EmitWORD(0);
}

//
// Write a structure or other buffer to the output file
//
void LinkWriter::EmitBlob(void* data, size_t size)
{
	OutputStream.write(reinterpret_cast<Byte*>(data), static_cast<std::streamsize>(size));
}

//
// Output bytes until the file reaches a given size
//
void LinkWriter::Pad(std::streamsize size, unsigned char byte)
{
	if(OutputStream.tellp() > size)
	{
#ifdef _DEBUG
		std::streamsize curpos = OutputStream.tellp();			// Just so we can read the value during debugging
#endif
		throw Exception("Already wrote past the end of padding area; aborting!");
	}

	while(OutputStream.tellp() < size)
		OutputStream << byte;
}

//
// Return the current write offset in the file
//
DWORD LinkWriter::GetOffset() const
{
	return OutputStream.tellp();
}
