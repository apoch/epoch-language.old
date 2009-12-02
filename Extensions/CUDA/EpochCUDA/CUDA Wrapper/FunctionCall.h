//
// The Epoch Language Project
// CUDA Interoperability Library
//
// Wrapper class for handling the invocation of CUDA functions
//

#pragma once


// Dependencies
#include <cuda.h>


// Forward declarations
class Module;


class FunctionCall
{
// Construction
public:
	FunctionCall(CUfunction handle);

// Parameter management
public:
	void AddParameter(CUdeviceptr devicepointer);

// Execution interface
public:
	void Execute();

// Internal tracking
private:
	CUfunction FunctionHandle;
	unsigned ParamOffset;
};
