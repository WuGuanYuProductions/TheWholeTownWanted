#include "WeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!DefaultWeaponRowName.IsNone())
	{
		EquipWeapon(DefaultWeaponRowName);
	}
}

bool UWeaponComponent::EquipWeapon(FName WeaponRowName)
{
	if (!WeaponDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponComponent: WeaponDataTable is null!"));
		return false;
	}

	FWeaponData* Row = WeaponDataTable->FindRow<FWeaponData>(WeaponRowName, TEXT("EquipWeaponContext"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponComponent: Cannot find weapon row %s"), *WeaponRowName.ToString());
		return false;
	}

	CurrentWeaponData = *Row;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponComponent: Owner is not a Character or has no Mesh!"));
		return false;
	}

	if (EquippedWeaponMesh)
	{
		EquippedWeaponMesh->DestroyComponent();
		EquippedWeaponMesh = nullptr;
	}

	EquippedWeaponMesh = NewObject<UStaticMeshComponent>(OwnerCharacter);
	if (EquippedWeaponMesh)
	{
		EquippedWeaponMesh->SetStaticMesh(CurrentWeaponData.WeaponMesh);
		EquippedWeaponMesh->RegisterComponent();

		FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
		EquippedWeaponMesh->AttachToComponent(OwnerCharacter->GetMesh(), AttachmentRules, HandSocketName);

		UE_LOG(LogTemp, Log, TEXT("WeaponComponent: Successfully equipped StaticMesh weapon: %s"), *WeaponRowName.ToString());
		return true;
	}

	return false;
}

void UWeaponComponent::Fire()
{
	if (!bCanFire || !EquippedWeaponMesh || !CurrentWeaponData.BulletClass)
	{
		return;
	}

	SpawnBullet();

	bCanFire = false;

	GetWorld()->GetTimerManager().SetTimer(
		FireCooldownTimerHandle,
		this,
		&UWeaponComponent::ResetFireCooldown,
		CurrentWeaponData.FireRate,
		false
	);
}

void UWeaponComponent::StopFire()
{
}

void UWeaponComponent::ResetFireCooldown()
{
	bCanFire = true;
}

void UWeaponComponent::SpawnBullet()
{
	if (!GetWorld() || !EquippedWeaponMesh || !CurrentWeaponData.BulletClass) return;

	FTransform MuzzleTransform = EquippedWeaponMesh->GetSocketTransform(CurrentWeaponData.MuzzleSocketName);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Bullet = GetWorld()->SpawnActor<AActor>(
		CurrentWeaponData.BulletClass,
		MuzzleTransform.GetLocation(),
		MuzzleTransform.GetRotation().Rotator(),
		SpawnParams
	);

	if (Bullet)
	{
		UE_LOG(LogTemp, Log, TEXT("WeaponComponent: Fired bullet with Damage: %f"), CurrentWeaponData.Damage);
	}
}
