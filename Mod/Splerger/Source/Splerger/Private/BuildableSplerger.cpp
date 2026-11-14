#include "BuildableSplerger.h"

#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"

ABuildableSplerger::ABuildableSplerger()
{
	// The conveyor-attachment base already creates mBufferInventory and
	// enables the factory tick; nothing else is needed here for now.
}

void ABuildableSplerger::BeginPlay()
{
	Super::BeginPlay();

	mSplergerInputs.Reset();
	mSplergerOutputs.Reset();

	TInlineComponentArray<UFGFactoryConnectionComponent*> connections;
	GetComponents(connections);

	// Find by name so the order is fixed: Input0, Input1 / Output0, Output1.
	static const FName InputNames[]  = { TEXT("Input0"),  TEXT("Input1")  };
	static const FName OutputNames[] = { TEXT("Output0"), TEXT("Output1") };
	for (const FName& name : InputNames)
	{
		for (UFGFactoryConnectionComponent* c : connections)
		{
			if (c->GetFName() == name) { mSplergerInputs.Add(c); }
		}
	}
	for (const FName& name : OutputNames)
	{
		for (UFGFactoryConnectionComponent* c : connections)
		{
			if (c->GetFName() == name) { mSplergerOutputs.Add(c); }
		}
	}

	mLastRequestTime.Init(-1000.f, mSplergerOutputs.Num());
	if (mSplergerInputs.Num() > 0)  { mNextInput  %= mSplergerInputs.Num(); }
	if (mSplergerOutputs.Num() > 0) { mNextOutput %= mSplergerOutputs.Num(); }
}

void ABuildableSplerger::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);

	if (!mBufferInventory || mSplergerInputs.Num() == 0)
	{
		return;
	}

	// Merger half: pull one item per input per pass, starting with whichever
	// input is next, while the buffer has room.
	const int32 numInputs = mSplergerInputs.Num();
	for (int32 pass = 0; pass < numInputs; ++pass)
	{
		const int32 index = (mNextInput + pass) % numInputs;
		UFGFactoryConnectionComponent* input = mSplergerInputs[index];
		if (!input || !input->IsConnected())
		{
			continue;
		}

		// Peek first so we never pull an item we have no room for.
		TArray<FInventoryItem> peeked;
		if (!input->Factory_PeekOutput(peeked) || peeked.Num() == 0)
		{
			continue;
		}
		FInventoryStack probe;
		probe.Item = peeked[0];
		probe.NumItems = 1;
		if (!mBufferInventory->HasEnoughSpaceForStack(probe))
		{
			break;
		}

		FInventoryItem item;
		float offset = 0.f;
		if (input->Factory_GrabOutput(item, offset))
		{
			FInventoryStack stack;
			stack.Item = item;
			stack.NumItems = 1;
			mBufferInventory->AddStack(stack);
			mNextInput = (index + 1) % numInputs;
		}
	}
}

int32 ABuildableSplerger::FrontSlot() const
{
	if (!mBufferInventory)
	{
		return INDEX_NONE;
	}
	for (int32 i = 0; i < mBufferInventory->GetSizeLinear(); ++i)
	{
		FInventoryStack stack;
		if (mBufferInventory->GetStackFromIndex(i, stack) && stack.HasItems())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int32 ABuildableSplerger::OutputIndexOf(const UFGFactoryConnectionComponent* connection) const
{
	for (int32 i = 0; i < mSplergerOutputs.Num(); ++i)
	{
		if (mSplergerOutputs[i] == connection) { return i; }
	}
	return INDEX_NONE;
}

bool ABuildableSplerger::IsOutputsTurn(int32 index) const
{
	if (index == mNextOutput)
	{
		return true;
	}
	// Not our turn: still allowed if the output that holds the turn is
	// unconnected or hasn't asked recently (i.e. it's backed up).
	const UFGFactoryConnectionComponent* holder = mSplergerOutputs.IsValidIndex(mNextOutput) ? mSplergerOutputs[mNextOutput] : nullptr;
	if (!holder || !holder->IsConnected())
	{
		return true;
	}
	const float now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return now - mLastRequestTime[mNextOutput] > OutputTurnTimeout;
}

bool ABuildableSplerger::Factory_PeekOutput_Implementation(const UFGFactoryConnectionComponent* connection, TArray<FInventoryItem>& out_items, TSubclassOf<UFGItemDescriptor> type) const
{
	const int32 index = OutputIndexOf(connection);
	if (index == INDEX_NONE)
	{
		return false;
	}
	if (GetWorld())
	{
		mLastRequestTime[index] = GetWorld()->GetTimeSeconds();
	}

	const int32 slot = FrontSlot();
	if (slot == INDEX_NONE || !IsOutputsTurn(index))
	{
		return false;
	}
	FInventoryStack stack;
	mBufferInventory->GetStackFromIndex(slot, stack);
	if (type && stack.Item.GetItemClass() != type)
	{
		return false;
	}
	out_items.Add(stack.Item);
	return true;
}

bool ABuildableSplerger::Factory_GrabOutput_Implementation(UFGFactoryConnectionComponent* connection, FInventoryItem& out_item, float& out_OffsetBeyond, TSubclassOf<UFGItemDescriptor> type)
{
	const int32 index = OutputIndexOf(connection);
	if (index == INDEX_NONE)
	{
		return false;
	}
	if (GetWorld())
	{
		mLastRequestTime[index] = GetWorld()->GetTimeSeconds();
	}

	const int32 slot = FrontSlot();
	if (slot == INDEX_NONE || !IsOutputsTurn(index))
	{
		return false;
	}
	FInventoryStack stack;
	mBufferInventory->GetStackFromIndex(slot, stack);
	if (type && stack.Item.GetItemClass() != type)
	{
		return false;
	}

	out_item = stack.Item;
	out_OffsetBeyond = 0.f;
	mBufferInventory->RemoveFromIndex(slot, 1);
	mNextOutput = (index + 1) % mSplergerOutputs.Num();
	return true;
}
