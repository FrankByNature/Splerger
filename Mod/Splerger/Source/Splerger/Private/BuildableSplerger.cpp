#include "BuildableSplerger.h"

#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"

namespace
{
	const FName GInputNames[]  = { TEXT("Input0"),  TEXT("Input1")  };
	const FName GOutputNames[] = { TEXT("Output0"), TEXT("Output1") };

	/** Most items moved per tick, so a long frame can't stall a fast belt. */
	constexpr int32 GMaxMovesPerTick = 4;
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

int32 ABuildableSplerger::PickOutputSlot() const
{
	const int32 numOutputs = mSplergerOutputs.Num();
	for (int32 step = 0; step < numOutputs; ++step)
	{
		const int32 index = (mNextOutput + step) % numOutputs;
		const UFGFactoryConnectionComponent* output = mSplergerOutputs[index];
		if (output && output->IsConnected() && mBufferInventory->IsIndexEmpty(index))
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
		const int32 slot = PickOutputSlot();
		if (slot == INDEX_NONE)
		{
			return; // every connected output is still holding an item
		}

		// Take one item from the next input that has one.
		bool moved = false;
		for (int32 step = 0; step < numInputs && !moved; ++step)
		{
			const int32 inputIndex = (mNextInput + step) % numInputs;
			UFGFactoryConnectionComponent* input = mSplergerInputs[inputIndex];
			if (!input || !input->IsConnected())
			{
				continue;
			}

			FInventoryItem item;
			float offset = 0.f;
			if (input->Factory_GrabOutput(item, offset))
			{
				mBufferInventory->AddStackToIndex(slot, FInventoryStack(item));
				mNextInput = (inputIndex + 1) % numInputs;
				mNextOutput = (slot + 1) % mSplergerOutputs.Num();
				moved = true;
			}
		}

		if (!moved)
		{
			return; // nothing waiting on any input
		}
	}
}
