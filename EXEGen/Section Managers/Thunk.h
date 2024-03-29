//
// The Epoch Language Project
// Win32 EXE Generator
//
// Wrapper objects for building thunk tables that
// store dynamically linked function addresses
//

#pragma once


// Dependencies
#include "Linker/Linker.h"


//
// Manages thunk tables for the executable
//
class ThunkManager : public LinkerSectionManager
{
// Section manager interface
public:
	virtual void Generate(Linker& linker);
	virtual void Emit(Linker& linker, LinkWriter& writer);

	virtual bool RepresentsPESection() const;

// Thunk management interface
public:
	void AddThunkFunction(const std::string& functionname, const std::string& libraryname);
	DWORD GetThunkAddress(const std::string& functionname) const;

	DWORD GetThunkTableOffset() const;
	DWORD GetThunkTableSize() const;

// Internal helpers
private:
	std::string MakeMagic(const std::string& library, const std::string& func) const;

// Internal tracking
private:
	std::map<std::string, DWORD> Libraries;
	std::map<std::string, std::pair<DWORD, DWORD> > LibraryThunkSpots;
	std::map<std::string, std::pair<DWORD, DWORD> > LibraryRewriteThunkSpots;
	std::map<std::string, std::pair<DWORD, std::string> > Functions;
	std::map<std::string, DWORD> FunctionThunkLocations;
	DWORD DataSize;
	DWORD ThunkTableOffset;
};

