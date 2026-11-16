#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "BuildableSplerger.generated.h"

class UFGFactoryConnectionComponent;

/** Recent items sent to one Splerger output, per item type (halved now and then). */
struct FSplergerOutputMix
{
	TMap<UClass*, float> Counts;
	float Total = 0.f;
};

/**
 * Hybrid splitter / merger: two belt inputs, two belt outputs.
 *
 * Works like the vanilla attachments in game 1.x: each output connection
 * serves items straight out of its own slot of mBufferInventory
 * (SetInventory + SetInventoryAccessIndex), and Factory_Tick moves items
 * from the inputs into those slots.
 *
 * - Inputs are taken in turn (merger behaviour). Only the input whose turn
 *   it is may top up a non-empty slot, so one input can't starve the other.
 * - An item goes to an output with an empty slot first (that output is ready,
 *   so faster belts get more), then to the output that most needs that item
 *   type to match the overall mix, so both outputs carry the same mix
 *   whatever order the items arrive in.
 * - An output that has had more than its share of an item type is skipped
 *   for it, unless no other output is taking items (splitter overflow:
 *   a blocked output sends everything to the other one).
 * - Each output slot holds what its belt takes in about two ticks (1 for
 *   slow belts, up to 3 for fast belts at low frame rates).
 *
 * The Blueprint child (Build_Splerger) must name its four
 * FGFactoryConnectionComponents "Input1", "Output1" (straight through, on X)
 * and "Input2", "Output2" (the sides, on Y) and set their Direction.
 * Input1/Output1 must be the straight-through pair: the vanilla conveyor
 * attachment hologram wires a cut belt into the components with those names.
 *
 * Checked against the headers of SatisfactoryModLoader dev (CL517554).
 */
UCLASS()
class SPLERGER_API ABuildableSplerger : public AFGBuildableConveyorAttachment
{
	GENERATED_BODY()

public:
	ABuildableSplerger();

	virtual void BeginPlay() override;

protected:
	virtual void Factory_Tick(float dt) override;

private:
	/** Can buffer slot `slot` take one more of `item`? (empty, or same stackable item below capacity) */
	bool SlotAccepts(int32 slot, const FInventoryItem& item) const;

	/** Items slot `slot` may hold now: what its belt takes in about two ticks, 1 to GSlotCapacity. */
	int32 SlotCapacity(int32 slot) const;

	/**
	 * Makes belt connections match where the belt ends physically are: moves a
	 * connection to the port the belt end sits on, and connects loose belt ends
	 * sitting on a free port. Server only.
	 */
	void FixUpPortConnections();

	/** Items in buffer slot `slot`. */
	int32 SlotCount(int32 slot) const;

	/** Output not connected, or holding items its belt hasn't taken for a while. */
	bool IsOutputBackedUp(int32 index) const;

	/**
	 * How many more of `itemClass` output `output` should have had to match
	 * the overall mix (negative: it has had more than its share).
	 */
	float MixDeficit(int32 output, UClass* itemClass) const;

	/** Is any output other than `output` still taking items? */
	bool AnyOtherOutputActive(int32 output) const;

	/** Count an item sent to `output` for the mix. */
	void RecordSent(int32 output, UClass* itemClass);

	/**
	 * Output slot index (== output index) that should get `item` now, or
	 * INDEX_NONE if it should wait. `onlyEmpty`: consider empty slots only.
	 */
	int32 PickOutputSlot(const FInventoryItem& item, bool onlyEmpty) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFGFactoryConnectionComponent>> mSplergerInputs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFGFactoryConnectionComponent>> mSplergerOutputs;

	/** Next input to pull from (round-robin). */
	UPROPERTY(SaveGame)
	int32 mNextInput = 0;

	/** Output whose turn it is when nothing else decides. */
	UPROPERTY(SaveGame)
	int32 mNextOutput = 0;

	/** Per output. Not saved: it rebuilds within a few items. */
	TArray<FSplergerOutputMix> mOutputMix;

	/** Per output: seconds since its belt last took an item (0 while empty). */
	TArray<float> mTimeSinceDrained;

	/** Per output: slot item count at the end of the last factory tick. */
	TArray<int32> mSlotCountAfterTick;

	/** Per output: items per second its belt has been taking (smoothed). */
	TArray<float> mDrainRate;

	/** Length of the last factory tick (s), for SlotCapacity. */
	float mLastTickSeconds = 1.f / 30.f;
};
