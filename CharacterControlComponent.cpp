#include "CharacterControlComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h" // 引入移动组件
#include "Engine/World.h"
#include "TimerManager.h" // 引入定时器

UCharacterControlComponent::UCharacterControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// 默认旋转参数
	RotationSpeed = 10.0f;
	TraceChannel = ECC_Visibility;

	// 默认冲刺参数初始化 (需求 2: 策划可配置)
	DashDuration = 0.25f; // 冲刺持续 0.25 秒
	DashSpeed = 25.0f; // 冲刺速度 25 米/秒 (相当于 2500 cm/s)
	DashCooldown = 1.5f; // 冷却 1.5 秒

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

	// 获取拥有该组件的 Character
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	// 核心逻辑 A: 如果正在冲刺，强行覆盖速度并跳过旋转逻辑
	if (bIsDashing)
	{
		if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
		{
			// 将米/秒(m/s) 转换为 虚幻单位(cm/s) 即 乘以 100
			MoveComp->Velocity = DashDirection * (DashSpeed * 100.0f);
		}
		return; // 冲刺期间不进行鼠标旋转检测，锁定朝向
	}

	// 获取玩家控制器
	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PC || !PC->IsLocalController()) return;

	FHitResult HitResult;
	// 1. 获取鼠标在屏幕中的 3D 空间位置
	if (PC->GetHitResultUnderCursor(TraceChannel, true, HitResult))
	{
		AActor* HitActor = HitResult.GetActor();

		// 3. 鼠标停留在角色身上时不旋转
		if (HitActor == OwnerCharacter)
		{
			return;
		}

		FVector TargetLocation = HitResult.ImpactPoint;
		FVector CharacterLocation = OwnerCharacter->GetActorLocation();

		// 忽略高度差（Z轴）
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
				// 2. 使用 RInterpTo 确保最短路径平滑旋转
				NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
			}

			OwnerCharacter->SetActorRotation(NewRotation);
		}
	}
}

void UCharacterControlComponent::Dash()
{
	// 3. 如果正在冲刺或处于CD期，则无法再次触发
	if (bIsDashing || !bCanDash) return;

	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;

	// ------------------ 迭代需求 1 ------------------
	// 获取玩家或NPC当前的移动输入向量 (世界空间)
	FVector LastInput = OwnerCharacter->GetLastMovementInputVector();

	// 如果没有位移输入（向量接近 0），直接拦截，不触发冲刺
	if (LastInput.IsNearlyZero())
	{
		return;
	}

	// 忽略 Z 轴高度，只考虑水平位移
	LastInput.Z = 0.0f;
	if (!LastInput.Normalize())
	{
		return;
	}

	// ------------------ 迭代需求 2 ------------------
	APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
	if (PC && PC->IsLocalController())
	{
		// 1. 获取控制器旋转（通常对应相机方向）
		FRotator ControlRot = PC->GetControlRotation();
		ControlRot.Pitch = 0.0f;
		ControlRot.Roll = 0.0f;

		// 2. 将世界输入向量“逆旋转”到控制器本地空间（从而知道玩家按下的是 W/A/S/D 中的哪个相对方向）
		FVector LocalInput = ControlRot.UnrotateVector(LastInput);

		// 3. 获取角色当前的真实世界朝向
		FRotator CharacterRot = OwnerCharacter->GetActorRotation();
		CharacterRot.Pitch = 0.0f;
		CharacterRot.Roll = 0.0f;

		// 4. 将提取出的相对输入方向，通过角色当前的真实朝向进行旋转
		DashDirection = CharacterRot.RotateVector(LocalInput);
	}
	else
	{
		// 如果是 NPC 或者是 AI 操控，由于没有本地 PlayerController 
		// 我们直接采用其当前的移动期望方向（LastInput已经是指向移动目标的世界向量了）
		DashDirection = LastInput;
	}

	// 防御性安全处理：如果计算出来的冲刺朝向接近 0，则采用角色当前的正前方
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = OwnerCharacter->GetActorForwardVector();
		DashDirection.Z = 0.0f;
		DashDirection.Normalize();
	}

	// 标记状态
	bIsDashing = true;
	bCanDash = false;

	// 设置定时器：冲刺持续时间结束后结束冲刺
	GetWorld()->GetTimerManager().SetTimer(DashTimerHandle, this, &UCharacterControlComponent::EndDash, DashDuration, false);

	// 设置定时器：冷却结束后重置CD状态
	GetWorld()->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UCharacterControlComponent::ResetCooldown, DashCooldown, false);
}

void UCharacterControlComponent::EndDash()
{
	bIsDashing = false;

	// 冲刺结束后将速度清零，防止由于惯性继续滑行
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