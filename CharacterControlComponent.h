#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterControlComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEWHOLETOWNWANTED_API UCharacterControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// 设置默认值
	UCharacterControlComponent();

protected:
	// 游戏开始时调用
	virtual void BeginPlay() override;

public:
	// 每帧调用
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 主动触发冲刺的函数 (支持在蓝图和 C++ 中调用) */
	UFUNCTION(BlueprintCallable, Category = "Character Control|Dash")
	void Dash();

public:
	// ==================== 旋转配置 ====================

	/** 角色旋转速度 (值越大旋转越快，0表示瞬间旋转) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Rotation", meta = (ClampMin = "0.0"))
	float RotationSpeed;

	/** 用于检测鼠标悬停的碰撞通道，默认使用 Visibility */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Rotation")
	TEnumAsByte<ECollisionChannel> TraceChannel;

	// ==================== 冲刺配置 (策划配置项) ====================

	/** 冲刺持续时间 (单位：秒) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0"))
	float DashDuration;

	/** 冲刺速度 (单位：米/秒 m/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0"))
	float DashSpeed;

	/** 冲刺冷却时间 (单位：秒) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Control|Dash", meta = (ClampMin = "0.0"))
	float DashCooldown;

	// ==================== 状态查询 (供蓝图动画或UI使用) ====================

	UFUNCTION(BlueprintPure, Category = "Character Control|Dash")
	bool IsDashing() const { return bIsDashing; }

	UFUNCTION(BlueprintPure, Category = "Character Control|Dash")
	bool IsDashOnCooldown() const { return !bCanDash; }

private:
	// 冲刺结束回调
	void EndDash();
	// 冷却结束回调
	void ResetCooldown();

private:
	// 状态变量
	bool bIsDashing;
	bool bCanDash;
	FVector DashDirection;

	// 定时器句柄
	FTimerHandle DashTimerHandle;
	FTimerHandle CooldownTimerHandle;
};