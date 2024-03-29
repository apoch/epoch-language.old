//
// The Epoch Language Project
// Win32 EXE Generator
//
// Wrapper objects for embedding Epoch bytecode in the executable
//

#pragma once


// Dependencies
#include "Linker/Linker.h"


//
// Wrapper class for writing Epoch bytecode
//
class EpochCode : public LinkerSectionManager
{
// Construction and destruction
public:
	explicit EpochCode(const std::wstring& filename);

// Section manager interface
public:
	virtual void Generate(Linker& linker);
	virtual void Emit(Linker& linker, LinkWriter& writer);

	virtual bool RepresentsPESection() const;

// Internal tracking
private:
	std::wstring Filename;
};

