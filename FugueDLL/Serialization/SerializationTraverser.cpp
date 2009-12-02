//
// The Epoch Language Project
// FUGUE Virtual Machine
//
// Traverser class for serializing code to Epoch Assembly format
//

#include "pch.h"

#include "Serialization/SerializationTraverser.h"
#include "Serialization/SerializationTokens.h"

#include "Virtual Machine/Core Entities/Scopes/ScopeDescription.h"
#include "Virtual Machine/Core Entities/Concurrency/Future.h"
#include "Virtual Machine/SelfAware.h"

#include "Marshalling/ExternalDLL.h"


using namespace Serialization;


SerializationTraverser::SerializationTraverser(const std::string& filename)
	: OutputStream(filename.c_str(), std::ios_base::out | std::ios_base::trunc),
	  CurrentProgram(NULL),
	  CurrentScope(NULL),
	  TabDepth(0),
	  IgnoreTabPads(false)
{
	if(!OutputStream)
		throw FileException("Failed to write to output file: " + filename);
}


void SerializationTraverser::SetProgram(VM::Program& program)
{
	CurrentProgram = &program;
}

void SerializationTraverser::EnterBlock(const VM::Block& block)
{
	if(TraversedObjects.find(&block) != TraversedObjects.end())
	{
		SkippedObjects.insert(&block);
		return;
	}

	TraversedObjects.insert(&block);

	PadTabs();
	OutputStream << BeginBlock << L"\n";
	++TabDepth;
}

void SerializationTraverser::ExitBlock(const VM::Block& block)
{
	if(SkippedObjects.find(&block) != SkippedObjects.end())
		return;

	if(TabDepth == 0)
		throw VM::InternalFailureException("The compiler's state has been corrupted - indentation level is already 0 when exiting a code block!");

	--TabDepth;
	PadTabs();
	OutputStream << EndBlock << L"\n";
}

void SerializationTraverser::NullBlock()
{
	PadTabs();
	OutputStream << Null << L"\n";
}

void SerializationTraverser::RegisterScope(VM::ScopeDescription& scope)
{
	CurrentScope = &scope;
	TraverseScope(scope);
}

void SerializationTraverser::TraverseScope(VM::ScopeDescription& scope)
{
	if(TraversedObjects.find(&scope) != TraversedObjects.end())
		return;

	TraversedObjects.insert(&scope);

	PadTabs();
	OutputStream << CurrentScope << L" " << Scope << L"\n";
	++TabDepth;

	PadTabs();
	OutputStream << ParentScope << L" " << scope.ParentScope << L"\n";

	PadTabs();
	OutputStream << Variables << L" " << scope.Variables.size() << L"\n";
	for(std::vector<std::wstring>::const_iterator iter = scope.MemberOrder.begin(); iter != scope.MemberOrder.end(); ++iter)
	{
		PadTabs();
		OutputStream << *iter << L" " << scope.GetVariableType(*iter) << L"\n";
	}

	PadTabs();
	OutputStream << Ghosts << L" " << scope.Ghosts.size() << L"\n";
	for(std::deque<VM::ScopeDescription::GhostVariableMap>::const_iterator iter = scope.Ghosts.begin(); iter != scope.Ghosts.end(); ++iter)
	{
		PadTabs();
		OutputStream << GhostRecord << L" " << iter->size() << L"\n";
		for(VM::ScopeDescription::GhostVariableMap::const_iterator inneriter = iter->begin(); inneriter != iter->end(); ++inneriter)
		{
			PadTabs();
			OutputStream << inneriter->first << L" " << inneriter->second << L"\n";
		}
	}

	PadTabs();
	OutputStream << Functions << L" " << scope.Functions.size() << L"\n";
	for(VM::ScopeDescription::FunctionMap::const_iterator iter = scope.Functions.begin(); iter != scope.Functions.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
		VM::SelfAwareBase* ptr = dynamic_cast<VM::SelfAwareBase*>(iter->second);
		if(ptr)
			ptr->Traverse(*this);
	}

	PadTabs();
	OutputStream << FunctionSignatureList << L" " << scope.FunctionSignatures.size() << L"\n";
	for(VM::ScopeDescription::FunctionSignatureMap::const_iterator iter = scope.FunctionSignatures.begin(); iter != scope.FunctionSignatures.end(); ++iter)
	{
		PadTabs();
		WriteFunctionSignature(iter->second);
	}

	PadTabs();
	OutputStream << TupleTypes << L" " << scope.TupleTypes.size() << L"\n";
	for(VM::ScopeDescription::TupleTypeIDMap::const_iterator iter = scope.TupleTypes.begin(); iter != scope.TupleTypes.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	PadTabs();
	OutputStream << TupleTypeHints << L" " << scope.TupleTypeHints.size() << L"\n";
	for(VM::ScopeDescription::TupleTypeIDMap::const_iterator iter = scope.TupleTypeHints.begin(); iter != scope.TupleTypeHints.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	PadTabs();
	OutputStream << TupleTypeMap << L" " << scope.TupleTracker.TupleTypeMap.size() << L"\n";
	for(std::map<IDType, VM::TupleType*>::const_iterator iter = scope.TupleTracker.TupleTypeMap.begin(); iter != scope.TupleTracker.TupleTypeMap.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L"\n";
		WriteTupleType(*(iter->second));
	}


	PadTabs();
	OutputStream << StructureTypes << L" " << scope.StructureTypes.size() << L"\n";
	for(VM::ScopeDescription::TupleTypeIDMap::const_iterator iter = scope.StructureTypes.begin(); iter != scope.StructureTypes.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	PadTabs();
	OutputStream << StructureTypeHints << L" " << scope.StructureTypeHints.size() << L"\n";
	for(VM::ScopeDescription::TupleTypeIDMap::const_iterator iter = scope.StructureTypeHints.begin(); iter != scope.StructureTypeHints.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	PadTabs();
	OutputStream << StructureTypeMap << L" " << scope.StructureTracker.StructureTypeMap.size() << L"\n";
	for(std::map<IDType, VM::StructureType*>::const_iterator iter = scope.StructureTracker.StructureTypeMap.begin(); iter != scope.StructureTracker.StructureTypeMap.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L"\n";
		WriteStructureType(*(iter->second));
	}

	PadTabs();
	OutputStream << Constants << L" " << scope.Constants.size() << L"\n";
	for(std::vector<std::wstring>::const_iterator iter = scope.Constants.begin(); iter != scope.Constants.end(); ++iter)
	{
		PadTabs();
		OutputStream << *iter << L"\n";
	}

	PadTabs();
	OutputStream << ResponseMaps << L" " << scope.ResponseMaps.size() << L"\n";
	for(VM::ScopeDescription::ResponseMapList::const_iterator iter = scope.ResponseMaps.begin(); iter != scope.ResponseMaps.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L"\n";
		WriteResponseMap(*(iter->second));
	}

	PadTabs();
	OutputStream << Futures << L" " << scope.Futures.size() << L"\n";
	for(VM::ScopeDescription::FutureMap::const_iterator iter = scope.Futures.begin(); iter != scope.Futures.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" ";
		dynamic_cast<VM::SelfAwareBase*>(iter->second->GetNestedOperation())->Traverse(*this);
	}

	PadTabs();
	OutputStream << ListTypes << L" " << scope.ListTypes.size() << L"\n";
	for(std::map<std::wstring, VM::EpochVariableTypeID>::const_iterator iter = scope.ListTypes.begin(); iter != scope.ListTypes.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	PadTabs();
	OutputStream << ListSizes << L" " << scope.ListSizes.size() << L"\n";
	for(std::map<std::wstring, size_t>::const_iterator iter = scope.ListSizes.begin(); iter != scope.ListSizes.end(); ++iter)
	{
		PadTabs();
		OutputStream << iter->first << L" " << iter->second << L"\n";
	}

	--TabDepth;
	PadTabs();
	OutputStream << EndScope << L"\n";
}

void SerializationTraverser::EnterTask()
{
	// Nothing to do for serialization.
}

void SerializationTraverser::ExitTask()
{
	// Nothing to do for serialization.
}


void SerializationTraverser::TraverseGlobalInitBlock(VM::Block* block)
{
	OutputStream << Serialization::GlobalBlock << L"\n";
	if(!block)
		OutputStream << Serialization::Null << L"\n";
}


void SerializationTraverser::PadTabs()
{
	if(IgnoreTabPads)
	{
		IgnoreTabPads = false;
		return;
	}

	for(unsigned i = 0; i < TabDepth; ++i)
		OutputStream << L"\t";
}


void SerializationTraverser::WriteFunctionSignature(const VM::FunctionSignature& signature)
{
	OutputStream << Serialization::FunctionSignatureBegin << L" ";

	OutputStream << signature.Params.size() << L"\n";
	PadTabs();
	for(std::vector<VM::EpochVariableTypeID>::const_iterator iter = signature.Params.begin(); iter != signature.Params.end(); ++iter)
		OutputStream << *iter << L" ";
	OutputStream << L"\n";
	PadTabs();
	OutputStream << signature.Returns.size() << L"\n";
	PadTabs();
	for(std::vector<VM::EpochVariableTypeID>::const_iterator iter = signature.Returns.begin(); iter != signature.Returns.end(); ++iter)
		OutputStream << *iter << L" ";
	OutputStream << L"\n";

	PadTabs();
	OutputStream << signature.ParamTypeHints.size() << L"\n";
	PadTabs();
	for(std::vector<IDType>::const_iterator iter = signature.ParamTypeHints.begin(); iter != signature.ParamTypeHints.end(); ++iter)
		OutputStream << *iter << L" ";
	OutputStream << L"\n";

	PadTabs();
	OutputStream << signature.ParamFlags.size() << L"\n";
	PadTabs();
	for(std::vector<unsigned>::const_iterator iter = signature.ParamFlags.begin(); iter != signature.ParamFlags.end(); ++iter)
		OutputStream << *iter << L" ";
	OutputStream << L"\n";

	PadTabs();
	OutputStream << signature.FunctionSignatures.size() << L"\n";
	for(std::vector<VM::FunctionSignature*>::const_iterator iter = signature.FunctionSignatures.begin(); iter != signature.FunctionSignatures.end(); ++iter)
	{
		if((*iter) != NULL)
			WriteFunctionSignature(**iter);
		else
		{
			PadTabs();
			OutputStream << Serialization::FunctionSignatureEnd << L"\n";
		}
	}
	OutputStream << L"\n";

	PadTabs();
	OutputStream << signature.ReturnTypeHints.size() << L"\n";
	PadTabs();
	for(std::vector<IDType>::const_iterator iter = signature.ReturnTypeHints.begin(); iter != signature.ReturnTypeHints.end(); ++iter)
		OutputStream << *iter << L" ";

	OutputStream << L"\n";
	PadTabs();
	OutputStream << Serialization::FunctionSignatureEnd << L"\n";
}


void SerializationTraverser::WriteStructureType(const VM::StructureType& type)
{
	PadTabs();
	OutputStream << Serialization::Members << L" " << type.MemberOrder.size() << L"\n";
	for(std::vector<std::wstring>::const_iterator iter = type.MemberOrder.begin(); iter != type.MemberOrder.end(); ++iter)
	{
		PadTabs();
		const VM::CompositeType::MemberInfo& member = type.MemberInfoMap.find(*iter)->second;
		OutputStream << *iter << L" " << member.Type << L" " << member.Offset << L"\n";

		if(member.Type == VM::EpochVariableType_Structure || member.Type == VM::EpochVariableType_Tuple)
		{
			PadTabs();
			OutputStream << type.GetMemberTypeHint(*iter) << L"\n";
		}
	}
}

void SerializationTraverser::WriteTupleType(const VM::TupleType& type)
{
	PadTabs();
	OutputStream << Serialization::Members << L" " << type.MemberOrder.size() << L"\n";
	for(std::vector<std::wstring>::const_iterator iter = type.MemberOrder.begin(); iter != type.MemberOrder.end(); ++iter)
	{
		PadTabs();
		const VM::CompositeType::MemberInfo& member = type.MemberInfoMap.find(*iter)->second;
		OutputStream << *iter << L" " << member.Type << L" " << member.Offset << L"\n";
	}
}

void SerializationTraverser::WriteResponseMap(const VM::ResponseMap& themap)
{
	PadTabs();
	OutputStream << themap.GetEntries().size() << L"\n";
	for(std::vector<VM::ResponseMapEntry*>::const_iterator iter = themap.GetEntries().begin(); iter != themap.GetEntries().end(); ++iter)
		WriteResponseMapEntry(**iter);
}

void SerializationTraverser::WriteResponseMapEntry(const VM::ResponseMapEntry& entry)
{
	PadTabs();
	OutputStream << entry.GetMessageName() << L"\n";
	
	PadTabs();
	OutputStream << entry.GetPayloadTypes().size() << L"\n";
	
	for(std::list<VM::EpochVariableTypeID>::const_iterator iter = entry.GetPayloadTypes().begin(); iter != entry.GetPayloadTypes().end(); ++iter)
	{
		PadTabs();
		OutputStream << *iter << L"\n";
	}
}

void SerializationTraverser::WriteOp(const void* opptr, const std::wstring& token, bool newline)
{
	PadTabs();
	OutputStream << opptr << L" " << token;
	if(newline)
		OutputStream << L"\n";
	else
	{
		OutputStream << L" ";
		IgnoreTabPads = true;
	}
}

void SerializationTraverser::WriteOp(const std::wstring& token)
{
	PadTabs();
	OutputStream << token << L"\n";
}

void SerializationTraverser::WriteOp(const void* opptr, const std::wstring& token, const std::wstring& param)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << param << L"\n";
}

void SerializationTraverser::WriteOp(const void* opptr, const std::wstring& token, const std::wstring& param1, const std::wstring& param2)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << param1 << L" " << param2 << L"\n";
}

void SerializationTraverser::WriteOp(const void* opptr, const std::wstring& token, const std::wstring& param1, const std::wstring& param2, VM::EpochVariableTypeID param3)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" ";
	OutputStream << param1 << L" " << param2 << L" " << param3 << L"\n";
}

void SerializationTraverser::WriteChainedOp(const void* opptr, const std::wstring& token, bool ischained, const std::wstring& param1, const std::wstring& param2)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << (ischained ? Serialization::True : Serialization::False);
	if(ischained)
		OutputStream << L" " << param2 << L"\n";
	else
		OutputStream << L" " << param1 << L" " << param2 << L"\n";
}

void SerializationTraverser::WriteOpWithPayload(const VM::Operation* opptr, const std::wstring& token)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" ";
	WritePayload(opptr->GetNodeTraversalPayload());
	OutputStream << L"\n";
}

void SerializationTraverser::WritePayload(const Traverser::Payload& payload)
{
	switch(payload.Type)
	{
	case VM::EpochVariableType_Integer:			OutputStream << payload.Int32Value;						break;
	case VM::EpochVariableType_Integer16:		OutputStream << payload.Int16Value;						break;
	case VM::EpochVariableType_Real:			OutputStream << payload.FloatValue;						break;
	case VM::EpochVariableType_Boolean:			OutputStream << (payload.BoolValue ? True : False);		break;
	case VM::EpochVariableType_Address:			OutputStream << payload.PointerValue;					break;
	case VM::EpochVariableType_String:
		if(payload.IsIdentifier)
			OutputStream << payload.StringValue;
		else
			OutputStream << std::wstring(payload.StringValue).length() << L" " << payload.StringValue;
		break;
	default:
		throw VM::NotImplementedException("Cannot emit contents of Payload structure - type is not supported or not recognized");
	}
}

void SerializationTraverser::WriteCastOp(const void* opptr, const std::wstring& token, VM::EpochVariableTypeID originaltype, VM::EpochVariableTypeID destinationtype)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << originaltype << L" " << destinationtype << L"\n";
}

void SerializationTraverser::WriteArithmeticOp(const void* opptr, const std::wstring& token, bool isfirstlist, bool issecondlist, size_t numparams)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" ";
	OutputStream << (isfirstlist ? Serialization::True : Serialization::False) << L" ";
	OutputStream << (issecondlist ? Serialization::True : Serialization::False) << L" ";
	OutputStream << numparams << L"\n";
}

void SerializationTraverser::WriteForkFuture(const void* opptr, const std::wstring& token, const std::wstring& varname, VM::EpochVariableTypeID type)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" ";
	OutputStream << varname << L" " << type << L"\n";
}

void SerializationTraverser::WriteSendMessage(const void* opptr, const std::wstring& token, bool usestaskid, const std::wstring& messagename, const std::list<VM::EpochVariableTypeID>& payloadtypes)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" ";
	OutputStream << (usestaskid ? Serialization::True : Serialization::False) << L" ";
	OutputStream << messagename << L" " << payloadtypes.size() << L"\n";

	++TabDepth;
	for(std::list<VM::EpochVariableTypeID>::const_iterator iter = payloadtypes.begin(); iter != payloadtypes.end(); ++iter)
	{
		PadTabs();
		OutputStream << *iter << L"\n";
	}
	--TabDepth;
}

void SerializationTraverser::WriteAcceptMessage(const void* opptr, const std::wstring& token, const std::wstring& messagename, const std::list<VM::EpochVariableTypeID>& payloadtypes)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L"\n";

	PadTabs();
	OutputStream << messagename << L"\n";

	PadTabs();
	OutputStream << payloadtypes.size() << L"\n";

	++TabDepth;
	for(std::list<VM::EpochVariableTypeID>::const_iterator iter = payloadtypes.begin(); iter != payloadtypes.end(); ++iter)
	{
		PadTabs();
		OutputStream << *iter << L"\n";
	}
	--TabDepth;
}


void SerializationTraverser::WriteConsList(const void* opptr, const std::wstring& token, VM::EpochVariableTypeID elementtype, size_t numelements)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << elementtype << L" " << numelements << L"\n";
}

void SerializationTraverser::WriteCompoundOp(const void* opptr, const std::wstring& token, size_t numops)
{
	PadTabs();
	OutputStream << opptr << L" " << token << L" " << numops << L"\n";
}
