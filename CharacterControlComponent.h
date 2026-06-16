#include "CharacterControlComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCharacterControlComponent::UCharacterControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	RotationSpeed = 10.0f;
	TraceChannel = ECC_Visibility;

	DashDuration = 0.25f;
	DashSpeed = 25.0f;
	DashCooldown = 1.5f;

	bIsDashing = false;
	bCanDash = true;
}

void UCharacterControlComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCharacterControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	if (bIsDashing)
	{
		if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
		{
			MoveComp->Velocity = DashDirection * (DashSpeed * 100.0f);
		}
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC || !PC->IsLocalController()) return;

	FHitResult HitResult;
	if (PC->GetHitResultUnderCursor(TraceChannel, true, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();

		if (HitActor == OwnerCharacter)
		{
			return;
		}

		FVector TargetLocation = HitResult.ImpactPoint;
		FVector CharacterLocation = OwnerCharacter->GetActorLocation();

		TargetLocation.Z = CharacterLocation.Z;
		FVector TargetDirection = TargetLocation - CharacterLocation;

		if (!TargetDirection.IsNearlyZero())
		{
			FRotator TargetRotation = TargetDirection.Rotation();
			FRotator CurrentRotation = OwnerCharacter->GetActorRotation();

			TargetRotation.Pitch = 0.0f;
			TargetRotation.Roll = 0.0f;
			CurrentRotation.Pitch = 0.0f;
			CurrentRotation.Roll = 0.0f;

			FRotator NewRotation;
			if (RotationSpeed <= 0.0f)
			{
				NewRotation = TargetRotation;
			}
			else
			{
				NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
			}

			OwnerCharacter->SetActorRotation(NewRotation);
		}
	}
}

void UCharacterControlComponent::Dash()
{
	if (bIsDashing || !bCanDash) return;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	FVector LastInput = OwnerCharacter->GetLastMovementInputVector();

	if (LastInput.IsNearlyZero())
	{
		return;
	}

	LastInput.Z = 0.0f;
	if (!LastInput.Normalize())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (PC && PC->IsLocalController())
	{
		FRotator ControlRot = PC->GetControlRotation();
		ControlRot.Pitch = 0.0f;
		ControlRot.Roll = 0.0f;

		FVector LocalInput = ControlRot.UnrotateVector(LastInput);

		FRotator CharacterRot = OwnerCharacter->GetActorRotation();
		CharacterRot.Pitch = 0.0f;
		CharacterRot.Roll = 0.0f;

		DashDirection = CharacterRot.RotateVector(LocalInput);
	}
	else
	{
		DashDirection = LastInput;
	}

	if (DashDirection.IsNearlyZero())
	{
		DashDirection = OwnerCharacter->GetActorForwardVector();
		DashDirection.Z = 0.0f;
		DashDirection.Normalize();
	}

	bIsDashing = true;
	bCanDash = false;

	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, this, &UCharacterControlComponent::EndDash, DashDuration, false);
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UCharacterControlComponent::ResetCooldown, DashCooldown, false);
}

void UCharacterControlComponent::EndDash()
{
	bIsDashing = false;

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
	}
}

void UCharacterControlComponent::ResetCooldown()
{
	bCanDash = true;
}
