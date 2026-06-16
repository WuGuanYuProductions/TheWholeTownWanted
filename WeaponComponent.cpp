#include "WeaponComponent.h"
#include "GameFramework/Character.h"
#include "Components/StaticMeshComponent.h" // 引入静态网格体组件头文件
#include "Engine/World.h"
#include "TimerManager.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // 武器组件不需要 Tick 提升性能
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 如果配置了默认武器，在游戏开始时自动装备
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

	// 1. 从 DataTable 中查找数据
	FWeaponData* Row = WeaponDataTable->FindRow<FWeaponData>(WeaponRowName, TEXT("EquipWeaponContext"));
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponComponent: Cannot find weapon row %s"), *WeaponRowName.ToString());
		return false;
	}

	CurrentWeaponData = *Row;

	// 2. 获取拥有此组件的 Character 以及其 Mesh
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter || !OwnerCharacter->GetMesh())
	{
		UE_LOG(LogTemp, Warning, TEXT("WeaponComponent: Owner is not a Character or has no Mesh!"));
		return false;
	}

	// 3. 如果之前已经有武器 Mesh，先销毁它
	if (EquippedWeaponMesh)
	{
		EquippedWeaponMesh->DestroyComponent();
		EquippedWeaponMesh = nullptr;
	}

	// 4. 动态创建并附加新的武器 Static Mesh 组件
	EquippedWeaponMesh = NewObject<UStaticMeshComponent>(OwnerCharacter);
	if (EquippedWeaponMesh)
	{
		// 设置 Static Mesh
		EquippedWeaponMesh->SetStaticMesh(CurrentWeaponData.WeaponMesh);

		// 注册组件使其在世界中生效（渲染、物理等）
		EquippedWeaponMesh->RegisterComponent();

		// 附加到角色的骨骼插槽 (HandSocketName)
		FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
		EquippedWeaponMesh->AttachToComponent(OwnerCharacter->GetMesh(), AttachmentRules, HandSocketName);

		UE_LOG(LogTemp, Log, TEXT("WeaponComponent: Successfully equipped StaticMesh weapon: %s"), *WeaponRowName.ToString());
		return true;
	}

	return false;
}

void UWeaponComponent::Fire()
{
	// 检查是否可以开火（CD 限制）并且当前有装备武器
	if (!bCanFire || !EquippedWeaponMesh || !CurrentWeaponData.BulletClass)
	{
		return;
	}

	// 1. 执行射击逻辑（生成子弹）
	SpawnBullet();

	// 2. 进入 CD 状态
	bCanFire = false;

	// 3. 根据 DataTable 配置的 FireRate 设置定时器
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
	// 单发武器暂无需处理
}

void UWeaponComponent::ResetFireCooldown()
{
	bCanFire = true;
}

void UWeaponComponent::SpawnBullet()
{
	if (!GetWorld() || !EquippedWeaponMesh || !CurrentWeaponData.BulletClass) return;

	// 获取武器 Static Mesh 上的枪口（Muzzle）位置和旋转
	// 注意：静态网格体同样可以在编辑器中添加 Sockets
	FTransform MuzzleTransform = EquippedWeaponMesh->GetSocketTransform(CurrentWeaponData.MuzzleSocketName);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 生成子弹
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