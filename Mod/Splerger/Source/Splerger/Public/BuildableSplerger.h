#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "BuildableSplerger.generated.h"

class UFGFactoryConnectionComponent;

/**
 * Hybrid splitter / merger: two belt inputs, two belt outputs.
 *
 * Works like the vanilla attachments in game 1.x: each output connection
 * serves items straight out of its own slot of mBufferInventory
 * (SetInventory + SetInventoryAccessIndex), and Factory_Tick moves items
 * from the inputs into those slots.
 *
 * - Inputs are taken round-robin (merger behaviour).
 * - Each item goes to the output whose turn it is; if that output's slot is
 *   still full (backed up / unconnected), it goes to the other one
 *   (splitter behaviour).
 *
 * The Blueprint child (Build_Splerger) must name its four
 * FGFactoryConnectionComponents "Input0", "Input1", "Output0", "Output1"
 * and set their Direction.
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
	/** Output slot index (== output index) that should get the next item, or INDEX_NONE if all are full. */
	int32 PickOutputSlot() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFGFactoryConnectionComponent>> mSplergerInputs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFGFactoryConnectionComponent>> mSplergerOutputs;

	/** Next input to pull from (round-robin). */
	UPROPERTY(SaveGame)
	int32 mNextInput = 0;

	/** Output whose turn it is. */
	UPROPERTY(SaveGame)
	int32 mNextOutput = 0;
};
