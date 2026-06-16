#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "WeaponComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	// The static mesh asset used for the weapon model.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> WeaponMesh = nullptr;

	// The time interval between shots (in seconds).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireRate = 0.2f;

	// The base damage dealt by this weapon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage = 20.0f;

	// The actor class (blueprint) of the projectile to spawn when firing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AActor> BulletClass;

	// The socket name defined on the weapon's Static Mesh where the projectile spawns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocketName = FName("Muzzle");
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEWHOLETOWNWANTED_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

protected:
	virtual void BeginPlay() override;

public:
	// The Data Table asset containing all weapon definitions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	TObjectPtr<UDataTable> WeaponDataTable;

	// The row name of the weapon in the Data Table to equip by default when the game starts.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	FName DefaultWeaponRowName;

	// The socket name on the character's skeletal mesh where the weapon will be attached.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	FName HandSocketName = FName("WeaponSocket_R");

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipWeapon(FName WeaponRowName);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EquippedWeaponMesh;

	FWeaponData CurrentWeaponData;

	bool bCanFire = true;
	FTimerHandle FireCooldownTimerHandle;

	void ResetFireCooldown();
	void SpawnBullet();
};
