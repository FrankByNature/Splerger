#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "BuildableSplerger.generated.h"

class UFGFactoryConnectionComponent;

/**
 * Hybrid splitter / merger: two belt inputs, two belt outputs.
 *
 * Inputs are pulled round-robin into the attachment's buffer (like a merger);
 * outputs are served alternately from that buffer (like a splitter). When one
 * output is backed up or unconnected the other gets everything.
 *
 * Connection components are found by name in BeginPlay, so the Blueprint
 * child (Build_Splerger) must name its four FGFactoryConnectionComponents
 * "Input0", "Input1", "Output0", "Output1" and set their Direction.
 *
 * NOTE: the overridden signatures follow FGBuildableConveyorAttachment.h /
 * FGBuildableFactory.h in the SML starter project. If the first compile
 * fails, compare them with Source/FactoryGame/Public/Buildables/ in your
 * SatisfactoryModLoader checkout and adjust.
 */
UCLASS()
class SPLERGER_API ABuildableSplerger : public AFGBuildableConveyorAttachment
{
	GENERATED_BODY()

public:
	ABuildableSplerger();

protected:
	virtual void BeginPlay() override;

	// AFGBuildableFactory
	virtual void Factory_Tick(float dt) override;
	virtual bool Factory_PeekOutput_Implementation(const UFGFactoryConnectionComponent* connection, TArray<FInventoryItem>& out_items, TSubclassOf<UFGItemDescriptor> type) const override;
	virtual bool Factory_GrabOutput_Implementation(UFGFactoryConnectionComponent* connection, FInventoryItem& out_item, float& out_OffsetBeyond, TSubclassOf<UFGItemDescriptor> type) override;

	/** Seconds an output may go without asking before it loses its turn. */
	UPROPERTY(EditDefaultsOnly, Category = "Splerger")
	float OutputTurnTimeout = 0.25f;

private:
	/** Index of the first non-empty buffer slot, or INDEX_NONE. */
	int32 FrontSlot() const;

	/** May output `index` take the front item right now? */
	bool IsOutputsTurn(int32 index) const;

	int32 OutputIndexOf(const UFGFactoryConnectionComponent* connection) const;

	UPROPERTY(Transient)
	TArray<UFGFactoryConnectionComponent*> mSplergerInputs;

	UPROPERTY(Transient)
	TArray<UFGFactoryConnectionComponent*> mSplergerOutputs;

	/** Next input to pull from (round-robin). */
	UPROPERTY(SaveGame)
	int32 mNextInput = 0;

	/** Output whose turn it is. */
	UPROPERTY(SaveGame)
	int32 mNextOutput = 0;

	/** World time each output last asked for an item. */
	mutable TArray<float> mLastRequestTime;
};
