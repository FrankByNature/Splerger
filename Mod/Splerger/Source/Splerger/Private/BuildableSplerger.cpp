#include "BuildableSplerger.h"

#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"

namespace
{
	const FName GInputNames[]  = { TEXT("Input0"),  TEXT("Input1")  };
	const FName GOutputNames[] = { TEXT("Output0"), TEXT("Output1") };

	/**
	 * Items each output slot may hold. More than 1 lets a belt take several
	 * items in one tick at low frame rates; the cost is that up to this many
	 * items are committed to an output ahead of time.
	 */
	constexpr int32 GSlotCapacity = 3;

	/** Most items moved per tick: enough to refill every slot from empty. */
	constexpr int32 GMaxMovesPerTick = GSlotCapacity * UE_ARRAY_COUNT(GOutputNames);
}

ABuildableSplerger::ABuildableSplerger()
{
	// One buffer slot per output; each slot is what that output serves from.
	mInventorySize = UE_ARRAY_COUNT(GOutputNames);
}

void ABuildableSplerger::BeginPlay()
{
	Super::BeginPlay();

	mSplergerInputs.Reset();
	mSplergerOutputs.Reset();

	TInlineComponentArray<UFGFactoryConnectionComponent*> connections;
	GetComponents(connections);

	auto findByName = [&connections](FName name) -> UFGFactoryConnectionComponent*
	{
		for (UFGFactoryConnectionComponent* c : connections)
		{
			if (c && c->GetFName() == name) { return c; }
		}
		return nullptr;
	};

	for (const FName& name : GInputNames)
	{
		if (UFGFactoryConnectionComponent* c = findByName(name)) { mSplergerInputs.Add(c); }
	}
	for (const FName& name : GOutputNames)
	{
		if (UFGFactoryConnectionComponent* c = findByName(name)) { mSplergerOutputs.Add(c); }
	}

	if (!HasAuthority() || !mBufferInventory)
	{
		return;
	}

	// The base class sets this up too, but be explicit: output i serves from slot i.
	if (mBufferInventory->GetSizeLinear() < mSplergerOutputs.Num())
	{
		mBufferInventory->Resize(mSplergerOutputs.Num());
	}
	for (int32 i = 0; i < mSplergerOutputs.Num(); ++i)
	{
		mSplergerOutputs[i]->SetInventory(mBufferInventory);
		mSplergerOutputs[i]->SetInventoryAccessIndex(i);
	}

	if (mSplergerInputs.Num() > 0)  { mNextInput  %= mSplergerInputs.Num(); }
	if (mSplergerOutputs.Num() > 0) { mNextOutput %= mSplergerOutputs.Num(); }
}

bool ABuildableSplerger::SlotAccepts(int32 slot, const FInventoryItem& item) const
{
	FInventoryStack stack;
	if (!mBufferInventory->GetStackFromIndex(slot, stack) || !stack.HasItems())
	{
		return true; // empty slot takes anything
	}
	// A slot is one inventory stack: same item type only, and items with
	// their own state (e.g. equipment) never stack.
	if (stack.Item.GetItemClass() != item.GetItemClass() || stack.Item.HasState() || item.HasState())
	{
		return false;
	}
	const int32 capacity = FMath::Min(GSlotCapacity, UFGItemDescriptor::GetStackSize(item.GetItemClass()));
	return stack.NumItems < capacity;
}

int32 ABuildableSplerger::PickOutputSlot(const FInventoryItem& item) const
{
	const int32 numOutputs = mSplergerOutputs.Num();
	for (int32 step = 0; step < numOutputs; ++step)
	{
		const int32 index = (mNextOutput + step) % numOutputs;
		const UFGFactoryConnectionComponent* output = mSplergerOutputs[index];
		if (output && output->IsConnected() && SlotAccepts(index, item))
		{
			return index;
		}
	}
	return INDEX_NONE;
}

void ABuildableSplerger::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);

	if (!HasAuthority() || !mBufferInventory || mSplergerInputs.Num() == 0 || mSplergerOutputs.Num() == 0)
	{
		return;
	}

	const int32 numInputs = mSplergerInputs.Num();
	for (int32 move = 0; move < GMaxMovesPerTick; ++move)
	{
		// Take one item from the next input whose front item has somewhere to go.
		bool moved = false;
		for (int32 step = 0; step < numInputs && !moved; ++step)
		{
			const int32 inputIndex = (mNextInput + step) % numInputs;
			UFGFactoryConnectionComponent* input = mSplergerInputs[inputIndex];
			if (!input || !input->IsConnected())
			{
				continue;
			}

			// Peek first so an item is only grabbed once it's sure to fit.
			TArray<FInventoryItem> waiting;
			if (!input->Factory_PeekOutput(waiting) || waiting.Num() == 0)
			{
				continue;
			}
			const int32 slot = PickOutputSlot(waiting[0]);
			if (slot == INDEX_NONE)
			{
				continue; // no output can take this item right now
			}

			FInventoryItem item;
			float offset = 0.f;
			if (input->Factory_GrabOutput(item, offset, waiting[0].GetItemClass()) && SlotAccepts(slot, item))
			{
				mBufferInventory->AddStackToIndex(slot, FInventoryStack(item));
				mNextInput = (inputIndex + 1) % numInputs;
				mNextOutput = (slot + 1) % mSplergerOutputs.Num();
				moved = true;
			}
		}

		if (!moved)
		{
			return; // inputs empty, or every output is full
		}
	}
}
