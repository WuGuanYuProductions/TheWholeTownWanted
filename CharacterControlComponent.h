#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterControlComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEWHOLETOWNWANTED_API UCharacterControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterControlComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Character Control|Dash")
	void Dash();

public:
	/** Rotation speed of the character. Higher values rotate faster, 0 for instant rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Rotation", meta = (ClampMin = "0.0", ToolTip = "Rotation speed of the character. Higher values rotate faster, 0 for instant rotation."))
	float RotationSpeed;

	/** Collision channel used to detect mouse cursor hit. Defaults to Visibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Rotation", meta = (ToolTip = "Collision channel used to detect mouse cursor hit. Defaults to Visibility."))
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Duration of the dash in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0", ToolTip = "Duration of the dash in seconds."))
	float DashDuration;

	/** Speed of the dash in meters per second (m/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0", ToolTip = "Speed of the dash in meters per second (m/s)."))
	float DashSpeed;

	/** Cooldown duration of the dash in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0", ToolTip = "Cooldown duration of the dash in seconds."))
	float DashCooldown;

public:
	UFUNCTION(BlueprintPure, Category = "Character Control|Dash")
	bool IsDashing() const { return bIsDashing; }

	UFUNCTION(BlueprintPure, Category = "Character Control|Dash")
	bool IsDashOnCooldown() const { return !bCanDash; }

private:
	void EndDash();
	void ResetCooldown();

private:
	bool bIsDashing;
	bool bCanDash;
	FVector DashDirection;

	FTimerHandle DashTimerHandle;
	FTimerHandle CooldownTimerHandle;
};
