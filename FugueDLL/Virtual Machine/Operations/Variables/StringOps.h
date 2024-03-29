//
// The Epoch Language Project
// FUGUE Virtual Machine
//
// Operations for working with strings
//

#pragma once

// Dependencies
#include "Virtual Machine/Core Entities/Operation.h"


namespace VM
{

	namespace Operations
	{

		//
		// Operation for concatenating two strings
		//
		class Concatenate : public Operation, public SelfAware<Concatenate>
		{
		// Construction
		public:
			Concatenate()
				: FirstIsArray(true),
				  SecondIsArray(false),
				  NumParams(1)
			{ }

			Concatenate(bool firstisarray, bool secondisarray)
				: FirstIsArray(firstisarray),
				  SecondIsArray(secondisarray),
				  NumParams(2)
			{ }

		// Operation interface
		public:
			virtual void ExecuteFast(ExecutionContext& context);
			virtual RValuePtr ExecuteAndStoreRValue(ExecutionContext& context);
			
			virtual EpochVariableTypeID GetType(const ScopeDescription& scope) const
			{ return EpochVariableType_String; }

			virtual size_t GetNumParameters(const VM::ScopeDescription& scope) const
			{ return NumParams; }

		// Array support
		public:
			void AddOperation(VM::Operation* op);
			void AddOperationToFront(VM::Operation* op);

		// Additional queries
		public:
			bool IsFirstArray() const			{ return FirstIsArray; }
			bool IsSecondArray() const			{ return SecondIsArray; }
			size_t GetNumParameters() const		{ return NumParams; }

		// Internal helpers
		private:
			std::wstring OperateOnArray(StackSpace& stack) const;

		// Internal tracking
		private:
			bool FirstIsArray;
			bool SecondIsArray;
			unsigned NumParams;
		};


		//
		// Operation for retrieving the length of a string
		//
		class Length : public Operation, public SelfAware<Length>
		{
		// Construction
		public:
			Length(const std::wstring& varname)
				: VarName(varname)
			{ }

		// Operation interface
		public:
			virtual void ExecuteFast(ExecutionContext& context);
			virtual RValuePtr ExecuteAndStoreRValue(ExecutionContext& context);
			
			virtual EpochVariableTypeID GetType(const ScopeDescription& scope) const
			{ return EpochVariableType_Integer; }

			virtual size_t GetNumParameters(const VM::ScopeDescription& scope) const
			{ return 0; }

			const std::wstring& GetAssociatedIdentifier() const
			{ return VarName; }

		// Traversal interface
		public:
			virtual Traverser::Payload GetNodeTraversalPayload(const VM::ScopeDescription* scope) const
			{
				Traverser::Payload payload;
				payload.SetValue(VarName.c_str());
				payload.IsIdentifier = true;
				payload.ParameterCount = GetNumParameters(*scope);
				return payload;
			}

		// Internal tracking
		private:
			const std::wstring& VarName;
		};


	}

}

