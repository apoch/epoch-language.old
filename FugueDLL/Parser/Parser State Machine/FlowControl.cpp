//
// The Epoch Language Project
// FUGUE Virtual Machine
//
// Code block and flow control management routines for the parser state machine
//

#include "pch.h"

#include "Parser/Parser State Machine/ParserState.h"
#include "Parser/Error Handling/ParserExceptions.h"
#include "Parser/Tracing.h"
#include "Parser/Parse.h"

#include "Virtual Machine/Core Entities/Program.h"
#include "Virtual Machine/Core Entities/Function.h"
#include "Virtual Machine/Core Entities/Block.h"
#include "Virtual Machine/Core Entities/Scopes/ScopeDescription.h"

#include "Virtual Machine/Operations/Flow/FlowControl.h"
#include "Virtual Machine/Operations/Concurrency/Tasks.h"
#include "Virtual Machine/Operations/StackOps.h"
#include "Virtual Machine/Operations/Variables/VariableOps.h"
#include "Virtual Machine/Operations/Variables/StructureOps.h"
#include "Virtual Machine/Operations/Variables/TupleOps.h"

#include "Virtual Machine/SelfAware.inl"

#include "Language Extensions/Handoff.h"
#include "Language Extensions/FunctionPointerTypes.h"

#include "Utility/Strings.h"


using namespace Parser;


//
// Register a flow control element and prepare to parse the attached code block.
//
void ParserState::RegisterControl(const std::wstring& controlname, bool preprocess)
{
	if(controlname == Keywords::Do)
		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_DOLOOP);
	else if(controlname == Keywords::If)
		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_IF);
	else if(controlname == Keywords::ElseIf)
	{
		if(!preprocess)
		{
			VM::Operations::If* ifop = NULL;

			size_t offset = 0;
			while(true)
			{
				VM::Operation* op = Blocks.back().TheBlock->GetOperationFromEnd(offset, *CurrentScope);
				ifop = dynamic_cast<VM::Operations::If*>(op);
				if(ifop)
					break;

				VM::Operations::ElseIfWrapper* elseifop = dynamic_cast<VM::Operations::ElseIfWrapper*>(op);
				if(!elseifop)
					throw ParserFailureException("elseif() without matching if()");

				++offset;
			}

			if(!ifop->GetElseIfBlock())
			{
				std::auto_ptr<VM::Operations::ElseIfWrapper> wrapper(new VM::Operations::ElseIfWrapper);
				std::auto_ptr<VM::ScopeDescription> scope(new VM::ScopeDescription);
				scope->ParentScope = CurrentScope;
				wrapper->GetBlock()->BindToScope(scope.release());
				ifop->SetElseIfBlock(wrapper.release());
			}

			BlockEntry entry;
			entry.TheBlock = ifop->GetElseIfBlock()->GetBlock();
			entry.Type = BlockEntry::BLOCKENTRYTYPE_ELSEIFWRAPPER;

			Blocks.push_back(entry);
		}

		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_ELSEIF);
	}
	else if(controlname == Keywords::Else)
		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_ELSE);
	else if(controlname == Keywords::While)
	{
		if(!preprocess)
		{
			BlockEntry entry;
			entry.TheBlock = NULL;
			entry.Type = BlockEntry::BLOCKENTRYTYPE_WHILELOOP;

			Blocks.push_back(entry);
			Blocks.back().TheBlock = new VM::Block;		// Allocate the block after the entry is tracked, for exception safety

			std::auto_ptr<VM::ScopeDescription> scope(new VM::ScopeDescription);
			scope->ParentScope = CurrentScope;
			CurrentScope = scope.release();
					
			Blocks.back().TheBlock->BindToScope(CurrentScope);
		}
		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_WHILELOOP);
	}
	else if(controlname == Keywords::ParallelFor)
		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_PARALLELFOR);
	else
	{
		// This will throw an exception should the keyword be invalid
		Extensions::GetLibraryProvidingExtension(controlname);

		StackEntry entry;
		entry.Type = StackEntry::STACKENTRYTYPE_IDENTIFIER;
		entry.StringValue = controlname;
		TheStack.push_back(entry);

		ExtensionControlKeywords.push(controlname);

		ExpectedBlockTypes.push(BlockEntry::BLOCKENTRYTYPE_EXTENSIONCONTROL);
	}
}

//
// Verify condition for a do-while loop, attach the parsed code
// block to the loop operation, and clean up from parsing.
//
void ParserState::PopDoWhileLoop()
{
	const StackEntry& entry = TheStack.back();

	if(entry.Type != StackEntry::STACKENTRYTYPE_OPERATION)
	{
		ReportFatalError("Syntax error - expected condition for do/while loop");

		std::auto_ptr<VM::Block> poppedblock(Blocks.back().TheBlock);
		Blocks.pop_back();
		TheStack.pop_back();
		return;
	}

	if(Blocks.back().TheBlock->GetTailOperation()->GetType(*CurrentScope) != VM::EpochVariableType_Boolean)
		ReportFatalError("Condition in do-while() statement must be a boolean expression");

	std::auto_ptr<VM::Block> poppedblock(Blocks.back().TheBlock);
	Blocks.pop_back();
	AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::DoWhileLoop(poppedblock.release())));

	TheStack.pop_back();
	PopParameterCount();
}

//
// Clean up (after ignoring) a do-while loop.
//
void ParserState::PopDoWhileLoopPP()
{
	Blocks.pop_back();
}

//
// Inject the while loop conditional check operation
//
void ParserState::RegisterEndOfWhileLoopConditional()
{
	PopParameterCount();
	MergeDeferredOperations();
	AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::WhileLoopConditional));
}

//
// Register that we are entering a new code block level.
// Creates the appropriate lexical scope and performs any other
// bindings and validation needed.
//
void ParserState::EnterBlock()
{
	if(!ExpectedBlockTypes.empty())
	{
		// We open the block for While loops early so we can inject the conditional
		if(ExpectedBlockTypes.top() == BlockEntry::BLOCKENTRYTYPE_WHILELOOP)
		{
			ExpectedBlockTypes.pop();
			return;
		}
		else if(ExpectedBlockTypes.top() == BlockEntry::BLOCKENTRYTYPE_MSGDISPATCH)
		{
			MessageDispatchScope->ParentScope = CurrentScope;
			CurrentScope = MessageDispatchScope;
			
			StackEntry entry;
			entry.Type = StackEntry::STACKENTRYTYPE_SCOPE;
			entry.ScopePointer = MessageDispatchScope;
			TheStack.push_back(entry);

			MessageDispatchScope = NULL;
		}
		else if(ExpectedBlockTypes.top() == BlockEntry::BLOCKENTRYTYPE_RESPONSEMAP)
		{
			ExpectedBlockTypes.pop();
			return;
		}
	}

	std::auto_ptr<VM::Block> blockptr(new VM::Block);

	BlockEntry entry;
	entry.TheBlock = blockptr.get();
	entry.Type = ExpectedBlockTypes.empty() ? BlockEntry::BLOCKENTRYTYPE_FREE : ExpectedBlockTypes.top();

	std::auto_ptr<VM::ScopeDescription> scope(new VM::ScopeDescription);
	if(entry.Type == BlockEntry::BLOCKENTRYTYPE_TASK || entry.Type == BlockEntry::BLOCKENTRYTYPE_THREAD)
	{
		scope->ParentScope = &GetParsedProgram()->GetGlobalScope();
		DisplacedScopes.push_back(CurrentScope);
		TraceScopeCreation(scope.get(), CurrentScope);
	}
	else if(entry.Type == BlockEntry::BLOCKENTRYTYPE_PARALLELFOR || entry.Type == BlockEntry::BLOCKENTRYTYPE_EXTENSIONCONTROL)
	{
		scope->AddVariable(ControlVarName, ControlVarType);

		TraceScopeCreation(scope.get(), NULL);
		scope->ParentScope = CurrentScope;
	}
	else
	{
		TraceScopeCreation(scope.get(), NULL);
		scope->ParentScope = CurrentScope;
	}

	CurrentScope = scope.release();

	if(entry.Type == BlockEntry::BLOCKENTRYTYPE_FUNCTION_NOCREATE)
	{
		if(TheStack.back().Type != StackEntry::STACKENTRYTYPE_IDENTIFIER)
			throw ParserFailureException("Entering function block but the function identifier is not on the parse stack!");

		VM::Function* func = dynamic_cast<VM::Function*>(CurrentScope->ParentScope->GetFunction(TheStack.back().StringValue));
		if(!func)
			throw ParserFailureException("Function not found or not a user-defined function; probably the internal parse stacks are corrupted");

		CurrentScope->PushNewGhostSet();
		func->GetParams().GhostIntoScope(*CurrentScope);

		func->GetReturns().ParentScope = NULL;
		func->GetReturns().GhostIntoScope(*CurrentScope);

		std::map<std::wstring, FunctionRetMap>::iterator funciter = FunctionReturnValueTracker.find(TheStack.back().StringValue);
		if(funciter != FunctionReturnValueTracker.end())
		{
			MergeFunctionReturns(funciter->second, *entry.TheBlock);
			FunctionReturnValueTracker.erase(funciter);
		}

		std::map<std::wstring, VM::Block*>::iterator funcretblockiter = FunctionReturnInitializationBlocks.find(TheStack.back().StringValue);
		if(funcretblockiter != FunctionReturnInitializationBlocks.end())
		{
			VM::Block* initblock = funcretblockiter->second;

			if(initblock)
			{
				std::vector<VM::Operation*>& ops = initblock->GetAllOperations();
				for(std::vector<VM::Operation*>::const_iterator iter = ops.begin(); iter != ops.end(); ++iter)
				{
					VM::Operation* op = *iter;
					VM::Operations::AssignValue* assignop = dynamic_cast<VM::Operations::AssignValue*>(op);

					if(assignop)
					{
						entry.TheBlock->AddOperation(VM::OperationPtr(new VM::Operations::InitializeValue(assignop->GetAssociatedIdentifier())));
						delete op;
					}
					else
						entry.TheBlock->AddOperation(VM::OperationPtr(op));
				}
				ops.clear();
				delete initblock;
			}

			FunctionReturnInitializationBlocks.erase(funcretblockiter);
		}
	}
	else if(entry.Type == BlockEntry::BLOCKENTRYTYPE_IF)
	{
		if(Blocks.back().TheBlock->GetTailOperation()->GetType(*CurrentScope) != VM::EpochVariableType_Boolean)
			ReportFatalError("Condition in if() statement must be a boolean expression");
	}
	else if(entry.Type == BlockEntry::BLOCKENTRYTYPE_ELSEIF)
	{
		if(Blocks.back().TheBlock->GetTailOperation()->GetType(*CurrentScope) != VM::EpochVariableType_Boolean)
			ReportFatalError("Condition in elseif() statement must be a boolean expression");
	}

	Blocks.push_back(entry);
	blockptr.release();
	Blocks.back().TheBlock->BindToScope(CurrentScope);

	if(!ExpectedBlockTypes.empty())
		ExpectedBlockTypes.pop();
}

//
// Enter a new code block in preparse phase; this effectively
// ignores the block, since we don't need to do anything with
// it during preparse.
//
void ParserState::EnterBlockPP()
{
	BlockEntry entry;
	entry.TheBlock = NULL;
	if(ExpectedBlockTypes.empty())
		entry.Type = BlockEntry::BLOCKENTRYTYPE_FREE;
	else
	{
		entry.Type = ExpectedBlockTypes.top();
		ExpectedBlockTypes.pop();
	}
	Blocks.push_back(entry);
}

//
// Register that we are exiting a code block.
// Binds the completed code block onto the appropriate
// function/operation, and cleans up from parsing.
//
void ParserState::ExitBlock()
{
	BlockEntry::BlockType type = Blocks.back().Type;
	switch(type)
	{
	case BlockEntry::BLOCKENTRYTYPE_FUNCTION:
		throw ParserFailureException("The grammar tried to do something unspeakable.");

	case BlockEntry::BLOCKENTRYTYPE_FUNCTION_NOCREATE:
		{
			const StackEntry& entry = TheStack.back();
			if(entry.Type != StackEntry::STACKENTRYTYPE_IDENTIFIER)
				throw ParserFailureException("Expected a valid function identifier but parse stack contains something else");

			VM::Function* func = dynamic_cast<VM::Function*>(CurrentScope->GetFunction(entry.StringValue));
			if(!func)
				throw ParserFailureException("Function not found or not a user-defined function; probably the internal parse stacks are corrupted");

			if(!Blocks.back().TheBlock)
				throw ParserFailureException("Expected to find a function code block, but found a null pointer instead!");

			func->SetCodeBlock(Blocks.back().TheBlock);
			TheStack.pop_back();
			Blocks.pop_back();
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_DOLOOP:
		break;		// We will clean up the stacks when the loop is popped

	case BlockEntry::BLOCKENTRYTYPE_IF:
		{
			const StackEntry& entry = TheStack.back();
			std::auto_ptr<VM::Block> ifblock(Blocks.back().TheBlock);
			Blocks.pop_back();

			if(entry.Type == StackEntry::STACKENTRYTYPE_IDENTIFIER)
			{
				if(CurrentScope->GetVariableType(entry.StringValue) != VM::EpochVariableType_Boolean)
				{
					ReportFatalError("Conditional variables must be of the boolean type");
					TheStack.pop_back();
					CurrentScope = CurrentScope->ParentScope;
					return;
				}
			}
			else if(entry.Type == StackEntry::STACKENTRYTYPE_OPERATION)
			{
				if(entry.OperationPointer->GetType(*CurrentScope) != VM::EpochVariableType_Boolean)
				{
					ReportFatalError("Conditional expression must be of the boolean type");
					TheStack.pop_back();
					CurrentScope = CurrentScope->ParentScope;
					return;
				}
			}
			else
			{
				ReportFatalError("Expected a conditional expression here");
				TheStack.pop_back();
				CurrentScope = CurrentScope->ParentScope;
				return;
			}

			AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::If(ifblock.release(), NULL)));
			TheStack.pop_back();
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_ELSEIF:
		{
			const StackEntry& entry = TheStack.back();
			std::auto_ptr<VM::Block> savedblock(Blocks.back().TheBlock);
			Blocks.pop_back();

			savedblock->AddOperation(VM::OperationPtr(new VM::Operations::ExitIfChain));

			if(entry.Type == StackEntry::STACKENTRYTYPE_IDENTIFIER)
			{
				if(CurrentScope->GetVariableType(entry.StringValue) != VM::EpochVariableType_Boolean)
				{
					ReportFatalError("Conditional variables must be of the boolean type");
					Blocks.pop_back();
					TheStack.pop_back();
					CurrentScope = CurrentScope->ParentScope;
					return;
				}
			}
			else if(entry.Type == StackEntry::STACKENTRYTYPE_OPERATION)
			{
				if(entry.OperationPointer->GetType(*CurrentScope) != VM::EpochVariableType_Boolean)
				{
					ReportFatalError("Expected a conditional expression here");
					Blocks.pop_back();
					TheStack.pop_back();
					CurrentScope = CurrentScope->ParentScope;
				}
			}
			else
			{
				ReportFatalError("Conditional variables must be of the boolean type");
				Blocks.pop_back();
				TheStack.pop_back();
				CurrentScope = CurrentScope->ParentScope;
				return;
			}

			AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::ElseIf(savedblock.release())));
			Blocks.pop_back();
			TheStack.pop_back();
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_ELSE:
		{
			std::auto_ptr<VM::Block> elseblock(Blocks.back().TheBlock);
			Blocks.pop_back();

			size_t position = 0;

			while(true)
			{
				VM::Operation* op = Blocks.back().TheBlock->GetOperationFromEnd(position, *CurrentScope);
				VM::Operations::If* ifop = dynamic_cast<VM::Operations::If*>(op);

				if(ifop)
				{
					ifop->SetFalseBlock(elseblock.release());
					break;
				}
				else
				{
					VM::Operations::ElseIfWrapper* elseifop = dynamic_cast<VM::Operations::ElseIfWrapper*>(op);
					if(!elseifop)
					{
						ReportFatalError("Unexpected else block with no matching if block");
						break;
					}
				}

				++position;
			}
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_WHILELOOP:
		{
			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();
			TheStack.pop_back();

			AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::WhileLoop(body.release())));
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_FREE:
		{
			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();

			AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::ExecuteBlock(body.release())));
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_TASK:
		{
			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();

			CurrentScope = DisplacedScopes.back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_String)
				throw ParserFailureException("Task identifiers must be string values");

			std::auto_ptr<VM::Operations::ForkTask> opptr(new VM::Operations::ForkTask(body.release()));
			DebugInfo.TrackTaskName(opptr.get(), SavedTaskNames.top());
			AddOperationToCurrentBlock(VM::OperationPtr(opptr.release()));
			TheStack.pop_back();
			DisplacedScopes.pop_back();
			SavedTaskNames.pop();
			return;
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_THREAD:
		{
			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();

			CurrentScope = DisplacedScopes.back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_String)
				throw ParserFailureException("Thread pool identifiers must be string values");

			TheStack.pop_back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_String)
				throw ParserFailureException("Thread identifiers must be string values");

			std::auto_ptr<VM::Operations::ForkThread> opptr(new VM::Operations::ForkThread(body.release()));
			DebugInfo.TrackTaskName(opptr.get(), SavedTaskNames.top());
			AddOperationToCurrentBlock(VM::OperationPtr(opptr.release()));

			TheStack.pop_back();
			DisplacedScopes.pop_back();
			SavedTaskNames.pop();
			return;
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_MSGDISPATCH:
		return;

	case BlockEntry::BLOCKENTRYTYPE_RESPONSEMAP:
		return;

	case BlockEntry::BLOCKENTRYTYPE_PARALLELFOR:
		{
			bool success = false;

			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_Integer)
				throw ParserFailureException("Last parameter to parallelfor() should be a thread count");
			TheStack.pop_back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_Integer)
				throw ParserFailureException("Third parameter to parallelfor() should be an upper boundary value");
			TheStack.pop_back();

			if(TheStack.empty() || TheStack.back().DetermineEffectiveType(*CurrentScope) != VM::EpochVariableType_Integer)
				throw ParserFailureException("Second parameter to parallelfor() should be a lower boundary value");
			TheStack.pop_back();

			if(TheStack.empty() || TheStack.back().Type != StackEntry::STACKENTRYTYPE_IDENTIFIER)
				ReportFatalError("First parameter to parallelfor() should be a loop counter variable name");
			else
			{
				success = true;
				AddOperationToCurrentBlock(VM::OperationPtr(new VM::Operations::ParallelFor(body.release(), ParsedProgram->PoolStaticString(TheStack.back().StringValue), true, 0)));
			}

			TheStack.pop_back();

			// If we didn't manage to attach the code block to a parallelfor instruction,
			// we need to ensure that we reset the current scope before exiting the switch
			// case, so that the auto_ptr doesn't release the block prior to us getting
			// ahold of the block's parent scope pointer.
			if(!success)
			{
				CurrentScope = CurrentScope->ParentScope;
				return;
			}
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_EXTENSIONCONTROL:
		{
			std::auto_ptr<VM::Block> body(Blocks.back().TheBlock);
			Blocks.pop_back();

			std::wstring controlname = ExtensionControlKeywords.top();
			ExtensionControlKeywords.pop();

			std::wstring countervarname;

			const std::vector<Extensions::ExtensionControlParamInfo>& paraminfo = Extensions::GetParamsForControl(controlname);
			for(std::vector<Extensions::ExtensionControlParamInfo>::const_reverse_iterator paraminfoiter = paraminfo.rbegin(); paraminfoiter != paraminfo.rend(); ++paraminfoiter)
			{
				if(paraminfoiter->CreatesLocalVariable)
				{
					if(TheStack.back().Type != StackEntry::STACKENTRYTYPE_IDENTIFIER)
						ReportFatalError("Expected a variable identifier");
					else
						countervarname = TheStack.back().StringValue;
				}
				else
				{
					if(TheStack.back().DetermineEffectiveType(*CurrentScope) != paraminfoiter->LocalVariableType)
						ReportFatalError("Parameter type is incorrect");
				}

				TheStack.pop_back();
			}

			std::wstring keyword = TheStack.back().StringValue;
			TheStack.pop_back();
			if(keyword != controlname)
				throw ParserFailureException("Mismatched control flow keywords, something has gone horribly wrong in the parser");

			AddOperationToCurrentBlock(VM::OperationPtr(new Extensions::HandoffControlOperation(ParsedProgram->PoolStaticString(keyword), body.release(), ParsedProgram->PoolStaticString(countervarname), *CurrentScope)));
		}
		break;

	default:
		throw ParserFailureException("Invalid block type; this probably reflects corruption in the parser");
	}

	CurrentScope = CurrentScope->ParentScope;
}

//
// Register that we are exiting a code block in preparse phase.
// Since we basically ignore blocks (except for function definitions)
// during preparse, this mainly cleans up the parser state a bit.
//
void ParserState::ExitBlockPP()
{
	BlockEntry::BlockType type = Blocks.back().Type;
	switch(type)
	{
	case BlockEntry::BLOCKENTRYTYPE_FUNCTION:
		{
			unsigned originalparamcount = ParamCount;

			std::auto_ptr<VM::ScopeDescription> params(new VM::ScopeDescription);

			params->ParentScope = CurrentScope;

			for( ; ParamCount > 0; --ParamCount)
			{
				switch(VariableTypeStack.top())
				{
				case VM::EpochVariableType_Tuple:
					if(ParamsByRef.top())
					{
						params->AddReference(VM::EpochVariableType_Tuple, VariableNameStack.top());
						params->SetVariableTupleTypeID(VariableNameStack.top(), VariableHintStack.top());
					}
					else
						params->AddTupleVariable(CurrentScope->GetTupleTypeID(VariableHintStack.top()), VariableNameStack.top());
					VariableHintStack.pop();
					break;

				case VM::EpochVariableType_Structure:
					if(ParamsByRef.top())
					{
						params->AddReference(VM::EpochVariableType_Structure, VariableNameStack.top());
						params->SetVariableStructureTypeID(VariableNameStack.top(), VariableHintStack.top());
					}
					else
						params->AddStructureVariable(CurrentScope->GetStructureTypeID(VariableHintStack.top()), VariableNameStack.top());
					VariableHintStack.pop();
					break;

				case VM::EpochVariableType_Function:
					if(ParamsByRef.top())
					{
						ReportFatalError("Cannot pass functions by reference");
						HigherOrderFunctionHintStack.pop();
						break;
					}
					params->AddFunctionSignature(VariableNameStack.top(), HigherOrderFunctionHintStack.top(), true);
					HigherOrderFunctionHintStack.pop();
					break;

				case VM::EpochVariableType_Array:
					if(ParamsByRef.top())
						params->AddReference(VM::EpochVariableType_Array, VariableNameStack.top());
					else
						params->AddVariable(VariableNameStack.top(), VM::EpochVariableType_Array);
					params->SetArrayType(VariableNameStack.top(), TempArrayType);
					break;

				default:
					if(ParamsByRef.top())
						params->AddReference(VariableTypeStack.top(), VariableNameStack.top());
					else
						params->AddVariable(VariableNameStack.top(), VariableTypeStack.top());
					break;
				}
				VariableTypeStack.pop();
				VariableNameStack.pop();
				ParamsByRef.pop();
			}

			params->ParentScope = NULL;

			const StackEntry& entry = TheStack.back();
			if(entry.Type != StackEntry::STACKENTRYTYPE_IDENTIFIER)
				throw ParserFailureException("Expected to find function identifier on the parse stack");

			FunctionReturns->ParentScope = NULL;
			FunctionReturns->RegisterSelfAsTupleType(entry.StringValue);
			std::auto_ptr<VM::FunctionBase> func(new VM::Function(*ParsedProgram, Blocks.back().TheBlock, params.release(), FunctionReturns));
			CurrentScope->AddFunction(ParsedProgram->PoolStaticString(entry.StringValue), func);
			FunctionReturns = NULL;

			if(UserInfixOperators.find(narrow(entry.StringValue)) != UserInfixOperators.end())
			{
				if(originalparamcount != 2)
					ReportFatalError("Infix functions must take exactly 2 parameters");
			}

			TheStack.pop_back();
			Blocks.pop_back();
		}
		break;

	case BlockEntry::BLOCKENTRYTYPE_FUNCTION_NOCREATE:
		TheStack.pop_back();
		Blocks.pop_back();
		break;

	case BlockEntry::BLOCKENTRYTYPE_DOLOOP:
		break;		// We will clean up the stacks when the loop is popped

	case BlockEntry::BLOCKENTRYTYPE_IF:
	case BlockEntry::BLOCKENTRYTYPE_ELSEIFWRAPPER:
	case BlockEntry::BLOCKENTRYTYPE_ELSEIF:
	case BlockEntry::BLOCKENTRYTYPE_ELSE:
	case BlockEntry::BLOCKENTRYTYPE_WHILELOOP:
	case BlockEntry::BLOCKENTRYTYPE_FREE:
	case BlockEntry::BLOCKENTRYTYPE_TASK:
	case BlockEntry::BLOCKENTRYTYPE_THREAD:
	case BlockEntry::BLOCKENTRYTYPE_MSGDISPATCH:
	case BlockEntry::BLOCKENTRYTYPE_PARALLELFOR:
	case BlockEntry::BLOCKENTRYTYPE_EXTENSIONCONTROL:
		Blocks.pop_back();
		break;

	default:
		throw ParserFailureException("Invalid block type - this probably reflects corruption in the parser");
	}
}


//
// Enter a special global data block
//
void ParserState::EnterGlobalBlock()
{
	BlockEntry entry;
	entry.Type = BlockEntry::BLOCKENTRYTYPE_GLOBAL;
	entry.TheBlock = &ParsedProgram->CreateGlobalInitBlock();
	Blocks.push_back(entry);
}

//
// Exit a special global data block
//
void ParserState::ExitGlobalBlock()
{
	Blocks.pop_back();
}



void ParserState::RegisterEndOfParallelFor()
{
	if(PassedParameterCount.top() != 4)
		ReportFatalError("parallelfor() expects 4 parameters");

	ControlVarName = (TheStack.rbegin() + 3)->StringValue;
	ControlVarType = VM::EpochVariableType_Integer;

	PopParameterCount();
}

