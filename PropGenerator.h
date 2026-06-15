#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PropGenerator.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEWHOLETOWNWANTED_API UPropGenerator : public UActorComponent
{
	GENERATED_BODY()

public:
	UPropGenerator();

protected:
	virtual void BeginPlay() override;

public:
	// ==========================================
	// 道具生成配置
	// ==========================================

	// 道具网格体读取文件路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (RelativeToGameContentDir))
	FDirectoryPath PropPath;

	// 道具与道具之间的额外间距范围（单位：米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation")
	FVector2D PropSpacing_Meters;

	// 距区块边缘的预留边距（单位：米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation")
	FVector2D EdgeMargin_Meters;

	// 道具离马路的物理避让距离（单位：米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ClampMin = "0.0"))
	float PropToRoadDistance_Meters;

	// 道具离建筑的物理避让距离（单位：米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ClampMin = "0.0"))
	float PropToBuildingDistance_Meters;

	// 每个区块最多生成的道具数量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation")
	int32 MaxPropsPerChunk;

	// 【需求3】独立的道具安全区半径（单位：米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation|Global Limits")
	float InitialSafeZoneRadius_Meters;

	/**
	 * 【核心接口】在指定的区块上生成道具
	 * @param ChunkActor 目标区块 Actor
	 * @param ChunkBounds 该区块的世界坐标2D范围
	 * @param RoadOccupiedBoxes 道路占用 2D 盒子
	 * @param BuildingOccupiedBoxes 建筑占用 2D 盒子
	 */
	void GeneratePropsOnChunk(
		AActor* ChunkActor,
		const FBox2D& ChunkBounds,
		const TArray<FBox2D>& RoadOccupiedBoxes,
		const TArray<FBox2D>& BuildingOccupiedBoxes
	);

private:
	UPROPERTY()
	TArray<UStaticMesh*> LoadedProps;

	// 【需求4】与建筑一样读取文件路径加载 Mesh
	void LoadProps();

	// 判定是否落入道具专属安全区
	bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;
};