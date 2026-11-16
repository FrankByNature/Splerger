#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "SplergerHologram.generated.h"

/**
 * Build gun hologram for the Splerger.
 *
 * Same as the vanilla conveyor attachment hologram, plus: while it is snapped
 * onto a belt, rotating (scroll / rotate key) flips which way the side ports
 * face. The Splerger has two in/out pairs on opposite faces (Input1/Output1 on
 * X, Input2/Output2 on Y), so either pair can carry the original belt:
 *
 *   not flipped: belt runs Input1 -> Output1
 *   flipped:     belt runs Input2 -> Output2 (building turned 90 degrees), the
 *                side input and output swap sides of the belt
 *
 * The vanilla hologram always wires the cut belt into the components named
 * AFGConveyorAttachmentHologram::mInputConnection1 / mOutputConnection1
 * ("Input1" / "Output1"), so when flipped those names are pointed at Input2 /
 * Output2 for the duration of the vanilla ConfigureComponents call.
 *
 * The flip state is stored in mScrollRotation (0 or 90 degrees), which the
 * vanilla hologram already replicates and sends to the server on build.
 */
UCLASS()
class SPLERGER_API ASplergerHologram : public AFGConveyorAttachmentHologram
{
	GENERATED_BODY()

public:
	// Begin AFGHologram Interface
	virtual void ScrollRotate(int32 delta, int32 step) override;
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
	virtual bool TrySnapToActor(const FHitResult& hitResult) override;
	// End AFGHologram Interface

protected:
	// Begin AFGBuildableHologram Interface
	virtual void ConfigureComponents(class AFGBuildable* inBuildable) const override;
	// End AFGBuildableHologram Interface

private:
	/** Snapped onto the middle of a belt (not a lift, not another building's port). */
	bool IsSnappedToBelt() const;

	/** Is the belt to run through Input2/Output2 instead of Input1/Output1? */
	bool IsFlipped() const;

	/**
	 * Snapped to a belt: set the rotation to the vanilla one, turned 90 degrees
	 * if flipped (so Input2/Output2 sit on the belt).
	 * @param afterVanillaUpdate  true right after the vanilla hologram placed
	 *                            itself; false when only the flip changed.
	 */
	void ApplyFlip(bool afterVanillaUpdate);

	/** Rotation the vanilla hologram chose on the belt (Input1 upstream). */
	FQuat mVanillaRotation = FQuat::Identity;

	/** Rotation ApplyFlip last set, to tell whether vanilla has moved us since. */
	FQuat mLastAppliedRotation = FQuat::Identity;

	bool bHasAppliedRotation = false;

	/** Yaw (degrees) that moves Input2 onto Input1's position; measured once from the components. */
	float GetFlipYaw() const;

	mutable TOptional<float> mFlipYaw;
};
