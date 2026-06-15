#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadNetworkComponent.generated.h"

// 道路类型枚举
UENUM(BlueprintType)
enum class ERoadType : uint8
{
	Straight UMETA(DisplayName = "Straight"),
	StraightToNode UMETA(DisplayName = "StraightToNode"),
	Turn UMETA(DisplayName = "Turn"),
	Cross UMETA(DisplayName = "Cross"),
	TRoad UMETA(DisplayName = "T-Road"),
	End UMETA(DisplayName = "End"),
	Parking UMETA(DisplayName = "Parking")
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class THEWHOLETOWNWANTED_API URoadNetworkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoadNetworkComponent();

protected:
	virtual void BeginPlay() override;

public:
	// ==================== 基础路网设置 ====================

	// 【核心控制：路网单元网格大小】（单位：厘米）
	// 值越大，道路间距越宽，给建筑留出的街区腹地越大。默认改为 3000.f（30米）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float CellSize = 3000.f;

	// 道路阻断概率 (0-100)，值越大死路和转弯越多
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlockChance = 20;

	// 生成种子，-1 为随机
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	int32 Seed = -1;

	// 道路宽度（单位：厘米，建筑会根据此宽度进行避让）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float RoadWidth = 300.f;

	// ==================== 概率设置 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrossChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TRoadChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ParkingChance = 0.35f;

	// ==================== 道路视觉资源 ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual")
	UStaticMesh* RoadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual")
	UMaterialInterface* RoadMaterial;

	/**
	 * 核心接口：在指定的区块（ChunkActor）上生成路网
	 * @param ChunkActor 目标区块Actor
	 * @param ChunkBounds 该区块的世界坐标2D范围
	 * @return 返回生成的路网在世界坐标下的所有 2D 占用盒子，用于建筑避让
	 */
	TArray<FBox2D> GenerateRoadNetworkOnChunk(AActor* ChunkActor, const FBox2D& ChunkBounds);

private:
	// 确定性哈希算法
	uint32 GetGridHash(int32 X, int32 Y) const;
	uint32 GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// 基础规则判定
	bool DoesNodeExistAtGrid(int32 X, int32 Y) const;
	bool IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// 获取节点基础类型
	ERoadType GetInitialRoadTypeAt(int32 X, int32 Y) const;
};
