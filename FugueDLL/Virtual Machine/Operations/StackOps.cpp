//
// The Epoch Language Project
// FUGUE Virtual Machine
//
// Operations for working with the execution stack
//

#include "pch.h"

#include "Virtual Machine/Operations/StackOps.h"
#include "Virtual Machine/Types Management/Typecasts.h"
#include "Virtual Machine/Core Entities/Scopes/ActivatedScope.h"
#include "Virtual Machine/Core Entities/Variables/TupleVariable.h"
#include "Virtual Machine/Core Entities/Variables/StructureVariable.h"
#include "Virtual Machine/Core Entities/Function.h"
#include "Virtual Machine/Core Entities/Program.h"
#include "Virtual Machine/Routines.inl"
#include "Virtual Machine/VMExceptions.h"
#include "Virtual Machine/Operations/Containers/ContainerOps.h"
#include "Virtual Machine/Operations/Flow/Invoke.h"
#include "Virtual Machine/SelfAware.inl"

#include "Parser/Debug Info Tables/DebugTable.h"


using namespace VM;
using namespace VM::Operations;


//
// Push an integer value onto the stack
//
void PushIntegerLiteral::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::IntegerT>(context.Stack, LiteralValue);
}

RValuePtr PushIntegerLiteral::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new IntegerRValue(LiteralValue));
}


Traverser::Payload PushIntegerLiteral::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(LiteralValue);
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}

//
// Push an integer value onto the stack
//
void PushInteger16Literal::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::Integer16T>(context.Stack, LiteralValue);
}

RValuePtr PushInteger16Literal::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new Integer16RValue(LiteralValue));
}

Traverser::Payload PushInteger16Literal::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(LiteralValue);
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}


//
// Push a string value onto the stack
//
void PushStringLiteral::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::StringT>(context.Stack, StringVariable::PoolStringLiteral(LiteralValue));
}

RValuePtr PushStringLiteral::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new StringRValue(LiteralValue));
}

Traverser::Payload PushStringLiteral::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(LiteralValue.c_str());
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}

//
// Push a real value onto the stack
//
void PushRealLiteral::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::RealT>(context.Stack, LiteralValue);
}

RValuePtr PushRealLiteral::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new RealRValue(LiteralValue));
}

Traverser::Payload PushRealLiteral::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(LiteralValue);
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}

//
// Push a boolean value onto the stack
//
void PushBooleanLiteral::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::BooleanT>(context.Stack, LiteralValue);
}

RValuePtr PushBooleanLiteral::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new BooleanRValue(LiteralValue));
}

Traverser::Payload PushBooleanLiteral::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(LiteralValue);
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}

//
// Construct and initialize an operation that pushes the
// result of another operation onto the stack
//
PushOperation::PushOperation(VM::Operation* op, const ScopeDescription& scope)
	: TheOp(op),
	  IsConsArray(false),
	  IsConsFromFunction(false)
{
	if(dynamic_cast<VM::Operations::ConsArray*>(TheOp) != NULL)
		IsConsArray = true;

	VM::Operations::Invoke* invokeop = dynamic_cast<VM::Operations::Invoke*>(TheOp);
	if(invokeop && invokeop->GetType(scope) == EpochVariableType_Array)
		IsConsFromFunction = true;
}

//
// Destruct and clean up an operation that pushes the result
// of another operation onto the stack
//
PushOperation::~PushOperation()
{
	delete TheOp;
}

//
// Evaluate an operation and place its value on the stack
//
RValuePtr PushOperation::ExecuteAndStoreRValue(ExecutionContext& context)
{
	RValuePtr opresult(TheOp->ExecuteAndStoreRValue(context));
	DoPush(TheOp->GetType(context.Scope.GetOriginalDescription()), opresult.get(), context.Scope.GetOriginalDescription(), context.Stack, IsConsArray, IsConsFromFunction);

	return opresult;
}

void PushOperation::ExecuteFast(ExecutionContext& context)
{
	ExecuteAndStoreRValue(context);
}

//
// Actually perform the push onto the stack
// This is factored out for easier usage with tuples/structures
//
void PushOperation::DoPush(EpochVariableTypeID type, RValue* value, const ScopeDescription& scope, StackSpace& stack, bool isconsarray, bool isconsfromfunction)
{
	switch(type)
	{
	case EpochVariableType_Null:
		throw ExecutionException("Cannot pass a null value on the stack");

	case EpochVariableType_Integer:
		PushValueOntoStack<TypeInfo::IntegerT>(stack, value);
		break;

	case EpochVariableType_Integer16:
		PushValueOntoStack<TypeInfo::Integer16T>(stack, value);
		break;

	case EpochVariableType_Real:
		PushValueOntoStack<TypeInfo::RealT>(stack, value);
		break;

	case EpochVariableType_Boolean:
		PushValueOntoStack<TypeInfo::BooleanT>(stack, value);
		break;

	case EpochVariableType_String:
		PushValueOntoStack<TypeInfo::StringT>(stack, VM::StringVariable::PoolStringLiteral(value->CastTo<StringRValue>().GetValue()));
		break;

	case EpochVariableType_Tuple:
		{
			const TupleRValue& tuple = value->CastTo<TupleRValue>();
			IDType tupletypeid = tuple.GetTupleTypeID();
			const TupleType& tupletype = scope.GetTupleType(tupletypeid);

			std::vector<std::wstring> members = tupletype.GetMemberOrder();
			for(std::vector<std::wstring>::const_reverse_iterator iter = members.rbegin(); iter != members.rend(); ++iter)
				DoPush(tupletype.GetMemberType(*iter), tuple.GetValue(*iter).get(), scope, stack, false, false);

			PushValueOntoStack<TypeInfo::TupleT>(stack, tupletypeid);
		}
		break;

	case EpochVariableType_Structure:
		{
			const StructureRValue& structure = value->CastTo<StructureRValue>();
			IDType structuretypeid = structure.GetStructureTypeID();
			const StructureType& structuretype = scope.GetStructureType(structuretypeid);

			std::vector<std::wstring> members = structuretype.GetMemberOrder();
			for(std::vector<std::wstring>::const_reverse_iterator iter = members.rbegin(); iter != members.rend(); ++iter)
				DoPush(structuretype.GetMemberType(*iter), structure.GetValue(*iter).get(), scope, stack, false, false);

			PushValueOntoStack<TypeInfo::StructureT>(stack, structuretypeid);
		}
		break;

	case EpochVariableType_Function:
		PushValueOntoStack<TypeInfo::FunctionT>(stack, value);
		break;

	case EpochVariableType_Address:
		PushValueOntoStack<TypeInfo::AddressT>(stack, value);
		break;

	case EpochVariableType_Array:
		stack.Push(sizeof(HandleType));
		*reinterpret_cast<HandleType*>(stack.GetCurrentTopOfStack()) = value->CastTo<ArrayRValue>().GetHandle();
		break;

	case EpochVariableType_TaskHandle:
		PushValueOntoStack<TypeInfo::TaskHandleT>(stack, value);
		break;

	case EpochVariableType_Buffer:
		stack.Push(sizeof(HandleType));
		*reinterpret_cast<HandleType*>(stack.GetCurrentTopOfStack()) = value->CastTo<BufferRValue>().GetOriginHandle();
		break;

	default:
		throw NotImplementedException("Cannot pass value of this type on the stack");
	}
}

template <typename TraverserT>
void PushOperation::TraverseHelper(TraverserT& traverser)
{
	traverser.TraverseNode(*this);
	if(TheOp)
		dynamic_cast<SelfAwareBase*>(TheOp)->Traverse(traverser);
}

void PushOperation::Traverse(Validator::ValidationTraverser& traverser)
{
	TraverseHelper(traverser);
}

void PushOperation::Traverse(Serialization::SerializationTraverser& traverser)
{
	TraverseHelper(traverser);
}

//
// Push a reference binding onto the stack
//
void BindReference::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::ReferenceBindingT>(context.Stack, &context.Scope.GetVariableRef(VarName));
}

RValuePtr BindReference::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new NullRValue);
}

Traverser::Payload BindReference::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(VarName.c_str());
	ret.IsIdentifier = true;
	return ret;
}

//
// Retrieve the binding's type
//
EpochVariableTypeID BindReference::GetType(const ScopeDescription& scope) const
{
	return scope.GetVariableType(VarName);
}

//
// Push a function binding onto the stack
//
void BindFunctionReference::ExecuteFast(ExecutionContext& context)
{
	PushValueOntoStack<TypeInfo::FunctionBindingT>(context.Stack, context.Scope.GetOriginalDescription().GetFunction(FunctionName));
}

RValuePtr BindFunctionReference::ExecuteAndStoreRValue(ExecutionContext& context)
{
	ExecuteFast(context);
	return RValuePtr(new NullRValue);
}

Traverser::Payload BindFunctionReference::GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
{
	Traverser::Payload ret;
	ret.SetValue(FunctionName.c_str());
	ret.IsIdentifier = true;
	ret.ParameterCount = GetNumParameters(*scope);
	return ret;
}

//
// Retrieve the bound function's type
//
EpochVariableTypeID BindFunctionReference::GetType(const ScopeDescription& scope) const
{
	return scope.GetFunction(FunctionName)->GetType(scope);
}


