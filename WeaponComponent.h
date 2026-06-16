#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "WeaponComponent.generated.h"

// 声明前置，减少头文件依赖
class UStaticMesh;
class UStaticMeshComponent;

// 1. 定义 DataTable 对应的结构体
USTRUCT(BlueprintType)
struct FWeaponData : public FTableRowBase
{
	GENERATED_BODY()

	// 武器的静态模型 (Static Mesh)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> WeaponMesh = nullptr;

	// 射速 (两次射击之间的间隔时间，单位：秒)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float FireRate = 0.2f;

	// 基础伤害
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage = 20.0f;

	// 子弹蓝图类
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AActor> BulletClass;

	// 枪口 Socket 名称（在 Static Mesh 编辑器中配置的 Socket）
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
	// --- 配置项 ---\n
	// 武器数据表
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	TObjectPtr<UDataTable> WeaponDataTable;

	// 默认装备的武器 Row Name (对应 DataTable 里的名字)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	FName DefaultWeaponRowName;

	// 角色手上绑定的 Socket 名字（例如 "GripPoint" 或 "WeaponSocket_R"）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponConfig")
	FName HandSocketName = FName("WeaponSocket_R");

	// --- 核心功能 API ---

	// 装配/切换武器
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipWeapon(FName WeaponRowName);

	// 开火
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	// 停止开火
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

private:
	// 动态创建的武器 Static Mesh 组件
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> EquippedWeaponMesh;

	// 当前武器的数据缓存
	FWeaponData CurrentWeaponData;

	// 开火冷却控制
	bool bCanFire = true;
	FTimerHandle FireCooldownTimerHandle;

	// 冷却结束回调
	void ResetFireCooldown();

	// 实际生成子弹的逻辑
	void SpawnBullet();
};