#include "SplergerHologram.h"

#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSplergerHologram, Log, All);

namespace
{
	const FName GInputName         = TEXT("Input1");
	const FName GFlippedInputName  = TEXT("Input2");
	const FName GFlippedOutputName = TEXT("Output2");

	/** mScrollRotation value (degrees) for the flipped orientation. */
	constexpr int32 GFlippedRotation = 90;

	/**
	 * Points the vanilla hologram's pass-through connection names at Input2 /
	 * Output2 while alive, and restores them afterwards. Only used on the game
	 * thread around a single vanilla call.
	 */
	struct FScopedFlippedConnectionNames
	{
		explicit FScopedFlippedConnectionNames(bool active)
			: bActive(active)
		{
			if (bActive)
			{
				SavedInput = AFGConveyorAttachmentHologram::mInputConnection1;
				SavedOutput = AFGConveyorAttachmentHologram::mOutputConnection1;
				AFGConveyorAttachmentHologram::mInputConnection1 = GFlippedInputName;
				AFGConveyorAttachmentHologram::mOutputConnection1 = GFlippedOutputName;
			}
		}

		~FScopedFlippedConnectionNames()
		{
			if (bActive)
			{
				AFGConveyorAttachmentHologram::mInputConnection1 = SavedInput;
				AFGConveyorAttachmentHologram::mOutputConnection1 = SavedOutput;
			}
		}

		bool bActive;
		FName SavedInput;
		FName SavedOutput;
	};
}

bool ASplergerHologram::IsSnappedToBelt() const
{
	return mSnappedConveyor && mSnappedConveyor->IsA<AFGBuildableConveyorBelt>();
}

bool ASplergerHologram::IsFlipped() const
{
	// mScrollRotation is in degrees. Treat it as quarter turns so a rotation
	// left over from placing on the floor also maps onto one of the two states.
	const int32 quarterTurns = FMath::RoundToInt(FMath::Abs(static_cast<float>(mScrollRotation)) / 90.f);
	return (quarterTurns % 2) == 1;
}

float ASplergerHologram::GetFlipYaw() const
{
	if (mFlipYaw.IsSet())
	{
		return mFlipYaw.GetValue();
	}

	// The turn that moves Input2 to where Input1 is, worked out from where the
	// hologram's connection components actually are (so it doesn't matter which
	// side face Input2 is on in Build_Splerger).
	TInlineComponentArray<UFGFactoryConnectionComponent*> connections;
	GetComponents(connections);
	const UFGFactoryConnectionComponent* input1 = nullptr;
	const UFGFactoryConnectionComponent* input2 = nullptr;
	FString found;
	for (const UFGFactoryConnectionComponent* c : connections)
	{
		found += FString::Printf(TEXT(" %s"), *c->GetName());
		if (c->GetFName() == GInputName)         { input1 = c; }
		if (c->GetFName() == GFlippedInputName)  { input2 = c; }
	}

	float yaw = -90.f; // Input2 on the -Y face, as in Build_Splerger
	if (input1 && input2)
	{
		const FTransform& actor = GetActorTransform();
		const FVector local1 = actor.InverseTransformPosition(input1->GetComponentLocation());
		const FVector local2 = actor.InverseTransformPosition(input2->GetComponentLocation());
		const float angle1 = FMath::RadiansToDegrees(FMath::Atan2(local1.Y, local1.X));
		const float angle2 = FMath::RadiansToDegrees(FMath::Atan2(local2.Y, local2.X));
		yaw = FRotator::NormalizeAxis(angle1 - angle2);
		UE_LOG(LogSplergerHologram, Log, TEXT("Flip turn %.1f degrees (Input1 at %s, Input2 at %s)"),
			yaw, *local1.ToCompactString(), *local2.ToCompactString());
	}
	else
	{
		UE_LOG(LogSplergerHologram, Warning, TEXT("Hologram has no Input1/Input2 (found:%s); flip turn defaults to %.1f"), *found, yaw);
	}
	mFlipYaw = yaw;
	return yaw;
}

void ASplergerHologram::ScrollRotate(int32 delta, int32 step)
{
	if (delta != 0 && IsSnappedToBelt())
	{
		// On a belt the only meaningful rotations are the two mirror layouts.
		mScrollRotation = IsFlipped() ? 0 : GFlippedRotation;
		ApplyFlip(false);
		return;
	}

	Super::ScrollRotate(delta, step);
}

void ASplergerHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	Super::SetHologramLocationAndRotation(hitResult);
	ApplyFlip(true);
}

bool ASplergerHologram::TrySnapToActor(const FHitResult& hitResult)
{
	const bool snapped = Super::TrySnapToActor(hitResult);
	if (snapped)
	{
		ApplyFlip(true);
	}
	return snapped;
}

void ASplergerHologram::ApplyFlip(bool afterVanillaUpdate)
{
	if (!IsSnappedToBelt())
	{
		bHasAppliedRotation = false;
		return;
	}

	// The vanilla hologram orients the building on the belt itself (Input1
	// upstream, Output1 downstream) and that is the base we turn from.
	const FQuat current = GetActorQuat();
	const bool vanillaWroteRotation = afterVanillaUpdate
		&& (!bHasAppliedRotation || !current.Equals(mLastAppliedRotation, 1.e-4f));
	if (vanillaWroteRotation || !bHasAppliedRotation)
	{
		mVanillaRotation = current;
	}

	FQuat target = mVanillaRotation;
	if (IsFlipped())
	{
		// Turn about the building's up axis so Input2 lands where Input1 was
		// (upstream) and Output2 where Output1 was (downstream).
		target = mVanillaRotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(GetFlipYaw()));
	}

	if (!current.Equals(target, 1.e-4f))
	{
		SetActorRotation(target);
	}
	mLastAppliedRotation = target;
	bHasAppliedRotation = true;
}

void ASplergerHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
	const bool flipped = IsSnappedToBelt() && IsFlipped();
	if (IsSnappedToBelt())
	{
		FVector beltLocation;
		FVector beltDirection;
		mSnappedConveyor->GetLocationAndDirectionAtOffset(mSnappedConveyorOffset, beltLocation, beltDirection);
		UE_LOG(LogSplergerHologram, Log, TEXT("%s placed on %s: flipped %s, scroll %d, vanilla yaw %.1f, final yaw %.1f, belt yaw %.1f"),
			*GetNameSafe(inBuildable), *GetNameSafe(mSnappedConveyor), flipped ? TEXT("yes") : TEXT("no"), mScrollRotation,
			mVanillaRotation.Rotator().Yaw, GetActorRotation().Yaw, beltDirection.Rotation().Yaw);
	}

	FScopedFlippedConnectionNames names(flipped);
	Super::ConfigureComponents(inBuildable);
}
