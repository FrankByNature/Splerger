#include "BuildableSplerger.h"

#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "Misc/ScopeExit.h"
#include "Resources/FGItemDescriptor.h"
#include "SplergerHologram.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSplerger, Log, All);

namespace
{
	// Input1/Output1 are the straight-through pair (the names the vanilla
	// conveyor attachment hologram wires a cut belt into); Input2/Output2 the sides.
	const FName GInputNames[]  = { TEXT("Input1"),  TEXT("Input2")  };
	const FName GOutputNames[] = { TEXT("Output1"), TEXT("Output2") };

	/** A belt end counts as sitting on a port when it is this close (cm). Ports are ~280 cm apart. */
	constexpr float GPortMatchDistance = 50.f;

	/**
	 * Most items an output slot may hold. More than 1 lets a fast belt take
	 * several items in one tick at low frame rates. Each slot only holds what
	 * its belt takes in about two ticks (see SlotCapacity), so slow belts get
	 * 1: items are committed to an output as late as possible.
	 */
	constexpr int32 GSlotCapacity = 3;

	/** Seconds over which an output's drain rate is smoothed. */
	constexpr float GDrainSmoothingSeconds = 1.f;

	/**
	 * An output whose slot holds items but hasn't had one taken for this long
	 * counts as backed up: it no longer holds up items waiting for it.
	 */
	constexpr float GBackedUpAfterSeconds = 0.5f;

	/**
	 * How far (in items) an output may get ahead of its fair share of one item
	 * type before that type waits for another output instead.
	 */
	constexpr float GMixTolerance = 1.f;

	/** Item counts are halved once their total passes this, so the mix follows recent flow. */
	constexpr float GMixHistory = 100.f;

	/** Most items moved per tick: enough to refill every slot from empty. */
	constexpr int32 GMaxMovesPerTick = GSlotCapacity * UE_ARRAY_COUNT(GOutputNames);
}

ABuildableSplerger::ABuildableSplerger()
{
	// One buffer slot per output; each slot is what that output serves from.
	mInventorySize = UE_ARRAY_COUNT(GOutputNames);

	// The conveyor-attachment base doesn't enable the factory tick; the vanilla
	// splitter/merger do it in their own constructors. Without this,
	// Factory_Tick never runs and items never move.
	mFactoryTickFunction.bCanEverTick = true;
	mFactoryTickFunction.bStartWithTickEnabled = true;

	// Own hologram: lets the build gun flip the side ports when placing on a belt.
	// (Build_Splerger's Hologram Class should be Holo_Splerger, a Blueprint child of this.)
	mHologramClass = ASplergerHologram::StaticClass();
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

	UE_LOG(LogSplerger, Verbose, TEXT("%s BeginPlay: %d inputs, %d outputs, buffer %s (%d slots), factory tick %s"),
		*GetName(), mSplergerInputs.Num(), mSplergerOutputs.Num(),
		mBufferInventory ? TEXT("ok") : TEXT("MISSING"),
		mBufferInventory ? mBufferInventory->GetSizeLinear() : 0,
		mFactoryTickFunction.bCanEverTick ? TEXT("on") : TEXT("OFF"));
	if (mSplergerInputs.Num() != UE_ARRAY_COUNT(GInputNames) || mSplergerOutputs.Num() != UE_ARRAY_COUNT(GOutputNames))
	{
		FString found;
		for (UFGFactoryConnectionComponent* c : connections)
		{
			found += FString::Printf(TEXT(" %s"), *c->GetName());
		}
		UE_LOG(LogSplerger, Warning, TEXT("%s: expected ports Input1/Input2/Output1/Output2, found:%s"), *GetName(), *found);
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
	const int32 numOutputs = mSplergerOutputs.Num();
	if (numOutputs > 0) { mNextOutput %= numOutputs; }
	mOutputMix.Reset();
	mOutputMix.SetNum(numOutputs);
	mTimeSinceDrained.Init(0.f, numOutputs);
	mSlotCountAfterTick.Init(0, numOutputs);
	mDrainRate.Init(0.f, numOutputs);

	// A belt cut by the build gun may be wired up after BeginPlay, so check now and again next tick.
	FixUpPortConnections();
	GetWorldTimerManager().SetTimerForNextTick(this, &ABuildableSplerger::FixUpPortConnections);
}

void ABuildableSplerger::FixUpPortConnections()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<UFGFactoryConnectionComponent*> ports;
	ports.Append(mSplergerInputs);
	ports.Append(mSplergerOutputs);

	auto isNear = [](const USceneComponent* a, const FVector& location)
	{
		return FVector::DistSquared(a->GetComponentLocation(), location) <= FMath::Square(GPortMatchDistance);
	};

	// 0. Input wired to an input (or output to an output) can never pass items;
	//    drop it. Leftover from a Splerger placed backwards on a belt.
	for (UFGFactoryConnectionComponent* port : ports)
	{
		UFGFactoryConnectionComponent* other = port ? port->GetConnection() : nullptr;
		if (other && other->GetDirection() == port->GetDirection())
		{
			UE_LOG(LogSplerger, Warning, TEXT("%s: %s is connected to %s.%s of the same direction; disconnecting (rebuild this Splerger)"),
				*GetName(), *port->GetName(), *GetNameSafe(other->GetOwner()), *other->GetName());
			port->ClearConnection();
		}
	}

	// 1. A port connected to a belt end that physically sits on a different, free
	//    port of the same direction: move the connection there. Repairs Splergers
	//    placed on a belt by 1.0.0, where the cut belt was wired to the side ports.
	for (UFGFactoryConnectionComponent* port : ports)
	{
		UFGFactoryConnectionComponent* other = port ? port->GetConnection() : nullptr;
		if (!other || isNear(port, other->GetComponentLocation()))
		{
			continue;
		}
		for (UFGFactoryConnectionComponent* target : ports)
		{
			if (target && target != port && !target->IsConnected()
				&& target->GetDirection() == port->GetDirection()
				&& isNear(target, other->GetComponentLocation()))
			{
				UE_LOG(LogSplerger, Log, TEXT("%s: moving belt connection %s -> %s (belt end sits on %s)"),
					*GetName(), *port->GetName(), *target->GetName(), *target->GetName());
				port->ClearConnection();
				target->SetConnection(other);
				break;
			}
		}
	}

	// 2. A free port with an unconnected belt end sitting right on it: connect them.
	//    Repairs belts whose saved connection pointed at a port name that no
	//    longer exists (1.0.0 named the ports Input0/Input1/Output0/Output1).
	for (UFGFactoryConnectionComponent* port : ports)
	{
		if (!port || port->IsConnected())
		{
			continue;
		}
		UFGFactoryConnectionComponent* found = UFGFactoryConnectionComponent::FindCompatibleOverlappingConnections(
			port, port->GetComponentLocation(), this, GPortMatchDistance);
		if (found && found->GetOwner() != this && !found->IsConnected())
		{
			UE_LOG(LogSplerger, Log, TEXT("%s: reconnecting loose belt end %s to %s"),
				*GetName(), *GetNameSafe(found->GetOwner()), *port->GetName());
			port->SetConnection(found);
		}
	}
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
	const int32 capacity = FMath::Min(SlotCapacity(slot), UFGItemDescriptor::GetStackSize(item.GetItemClass()));
	return stack.NumItems < capacity;
}

int32 ABuildableSplerger::SlotCapacity(int32 slot) const
{
	// Enough for what the belt takes in about two ticks: a slow belt gets 1,
	// so the item it gets is picked when it's ready for it; a fast belt at a
	// low frame rate gets up to GSlotCapacity.
	const float perTick = mDrainRate[slot] * mLastTickSeconds;
	return FMath::Clamp(FMath::CeilToInt(perTick * 2.f), 1, GSlotCapacity);
}

int32 ABuildableSplerger::SlotCount(int32 slot) const
{
	FInventoryStack stack;
	return mBufferInventory->GetStackFromIndex(slot, stack) && stack.HasItems() ? stack.NumItems : 0;
}

bool ABuildableSplerger::IsOutputBackedUp(int32 index) const
{
	const UFGFactoryConnectionComponent* output = mSplergerOutputs[index];
	if (!output || !output->IsConnected())
	{
		return true;
	}

	// Full is not backed up: a fast belt's 1-item slot is full most of the
	// time. Backed up means its belt has stopped taking items.
	return SlotCount(index) > 0 && mTimeSinceDrained[index] > GBackedUpAfterSeconds;
}

float ABuildableSplerger::MixDeficit(int32 output, UClass* itemClass) const
{
	float totalAll = 0.f;
	float itemAll = 0.f;
	for (const FSplergerOutputMix& mix : mOutputMix)
	{
		totalAll += mix.Total;
		if (const float* n = mix.Counts.Find(itemClass)) { itemAll += *n; }
	}
	if (totalAll <= 0.f)
	{
		return 0.f;
	}
	// Fair share: this output's part of all items of this type, in proportion
	// to how much it takes overall (a faster belt takes more of everything).
	const float fair = itemAll * mOutputMix[output].Total / totalAll;
	const float* had = mOutputMix[output].Counts.Find(itemClass);
	return fair - (had ? *had : 0.f);
}

bool ABuildableSplerger::AnyOtherOutputActive(int32 output) const
{
	for (int32 i = 0; i < mSplergerOutputs.Num(); ++i)
	{
		if (i != output && !IsOutputBackedUp(i))
		{
			return true;
		}
	}
	return false;
}

void ABuildableSplerger::RecordSent(int32 output, UClass* itemClass)
{
	mOutputMix[output].Counts.FindOrAdd(itemClass) += 1.f;
	mOutputMix[output].Total += 1.f;

	float totalAll = 0.f;
	for (const FSplergerOutputMix& mix : mOutputMix) { totalAll += mix.Total; }
	if (totalAll > GMixHistory)
	{
		for (FSplergerOutputMix& mix : mOutputMix)
		{
			mix.Total *= 0.5f;
			for (TPair<UClass*, float>& entry : mix.Counts) { entry.Value *= 0.5f; }
		}
	}
}

int32 ABuildableSplerger::PickOutputSlot(const FInventoryItem& item, bool onlyEmpty) const
{
	UClass* itemClass = item.GetItemClass();
	const int32 numOutputs = mSplergerOutputs.Num();

	int32 best = INDEX_NONE;
	bool bestEmpty = false;
	float bestDeficit = 0.f;
	for (int32 step = 0; step < numOutputs; ++step)
	{
		const int32 index = (mNextOutput + step) % numOutputs;
		const UFGFactoryConnectionComponent* output = mSplergerOutputs[index];
		if (!output || !output->IsConnected())
		{
			continue;
		}
		const bool empty = SlotCount(index) == 0;
		if ((onlyEmpty && !empty) || !SlotAccepts(index, item))
		{
			continue;
		}
		// Already had more than its share of this item: leave it for another
		// output, unless no other output is taking items (overflow).
		const float deficit = MixDeficit(index, itemClass);
		if (deficit < -GMixTolerance && AnyOtherOutputActive(index))
		{
			continue;
		}
		// Prefer an empty slot (that output is ready for more, so faster belts
		// get more), then the output that most needs this item; ties go to
		// whichever output's turn it is.
		const bool better = best == INDEX_NONE
			|| (empty && !bestEmpty)
			|| (empty == bestEmpty && deficit > bestDeficit + KINDA_SMALL_NUMBER);
		if (better)
		{
			best = index;
			bestEmpty = empty;
			bestDeficit = deficit;
		}
	}
	return best;
}

void ABuildableSplerger::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);

	if (!HasAuthority() || !mBufferInventory || mSplergerInputs.Num() == 0 || mSplergerOutputs.Num() == 0)
	{
		return;
	}

	if (mTimeSinceDrained.Num() != mSplergerOutputs.Num() || mOutputMix.Num() != mSplergerOutputs.Num()
		|| mDrainRate.Num() != mSplergerOutputs.Num())
	{
		return; // not set up (BeginPlay didn't finish)
	}

	// Track when each output last had an item taken by its belt, to tell a
	// slow output from a backed-up one.
	mLastTickSeconds = dt;
	const float smoothing = dt > 0.f ? FMath::Min(1.f, dt / GDrainSmoothingSeconds) : 0.f;
	for (int32 i = 0; i < mSplergerOutputs.Num(); ++i)
	{
		const int32 count = SlotCount(i);
		if (dt > 0.f)
		{
			const int32 drained = FMath::Max(0, mSlotCountAfterTick[i] - count);
			mDrainRate[i] += (drained / dt - mDrainRate[i]) * smoothing;
		}
		if (count == 0 || count < mSlotCountAfterTick[i])
		{
			mTimeSinceDrained[i] = 0.f;
		}
		else
		{
			mTimeSinceDrained[i] += dt;
		}
	}
	ON_SCOPE_EXIT
	{
		for (int32 i = 0; i < mSplergerOutputs.Num(); ++i)
		{
			mSlotCountAfterTick[i] = SlotCount(i);
		}
	};

	const int32 numInputs = mSplergerInputs.Num();
	for (int32 move = 0; move < GMaxMovesPerTick; ++move)
	{
		// Front item of every input, starting with the input whose turn it is.
		struct FCandidate { int32 Input; FInventoryItem Item; };
		TArray<FCandidate, TInlineAllocator<2>> candidates;
		for (int32 step = 0; step < numInputs; ++step)
		{
			const int32 inputIndex = (mNextInput + step) % numInputs;
			UFGFactoryConnectionComponent* input = mSplergerInputs[inputIndex];
			TArray<FInventoryItem> waiting;
			if (input && input->IsConnected() && input->Factory_PeekOutput(waiting) && waiting.Num() > 0)
			{
				candidates.Add({ inputIndex, waiting[0] });
			}
		}

		bool moved = false;
		for (int32 c = 0; c < candidates.Num() && !moved; ++c)
		{
			// The input whose turn it is may top up a slot; the others only fill
			// empty ones, so they can't keep a slot busy with their item while
			// the turn input waits for it to drain (that starved one input).
			const FCandidate& candidate = candidates[c];
			const int32 slot = PickOutputSlot(candidate.Item, /*onlyEmpty*/ c > 0);
			if (slot == INDEX_NONE)
			{
				continue;
			}

			FInventoryItem item;
			float offset = 0.f;
			UFGFactoryConnectionComponent* input = mSplergerInputs[candidate.Input];
			if (input->Factory_GrabOutput(item, offset, candidate.Item.GetItemClass()) && SlotAccepts(slot, item))
			{
				mBufferInventory->AddStackToIndex(slot, FInventoryStack(item));
				RecordSent(slot, item.GetItemClass());
				mNextInput = (candidate.Input + 1) % numInputs;
				mNextOutput = (slot + 1) % mSplergerOutputs.Num();
				moved = true;
			}
		}

		if (!moved)
		{
			return; // inputs empty, or nothing can go anywhere yet
		}
	}
}
