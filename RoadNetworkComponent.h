#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadNetworkComponent.generated.h"

// Road type enum
UENUM(BlueprintType)
enum class ERoadType : uint8
{
	Straight UMETA(DisplayName = "Straight"),
	StraightToNode UMETA(DisplayName = "StraightToNode"), // 新增分类
	Turn UMETA(DisplayName = "Turn"),
	Cross UMETA(DisplayName = "Cross"),
	TRoad UMETA(DisplayName = "T-Road"),
	End UMETA(DisplayName = "End"),
	Parking UMETA(DisplayName = "Parking")
};

// Runtime-generated temporary road network node
struct FProceduralRoadNode
{
	FIntPoint GridCoords;
	FVector WorldPosition;
	ERoadType RoadType;
	TArray<FIntPoint> ConnectedCoords;
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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Size of each grid cell (in centimeters, 800.f = 8 meters)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float CellSize = 800.f;

	// Generation half-radius (number of grid cells. 4 cells represents a 4 * 8m = 32m radius, covering 64m * 64m in total)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	int32 GridRadius = 4;

	// Road block probability (0-100). Higher values lead to more dead ends and turns, resulting in a sparser road network.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlockChance = 20;

	// Debug size of the node sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Debug")
	float NodeSphereRadius = 30.f;

private:
	// Deterministic pseudo-random hash functions
	uint32 GetGridHash(int32 X, int32 Y) const;
	uint32 GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// Checks if a road node exists at the specified grid coordinates
	bool DoesNodeExistAtGrid(int32 X, int32 Y) const;

	// Checks if two adjacent nodes are connected (not blocked)
	bool IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// Helper: Safely calculate the base road type (without StraightToNode conversion) at any coordinates
	ERoadType GetInitialRoadTypeAt(int32 X, int32 Y) const;

	// Core logic: Gets the player position, generates the local road network, and draws debug info
	void UpdateAndDrawRoadNetwork(const FVector& PlayerLocation);

	FColor GetColorForRoadType(ERoadType Type) const;
	FString GetTextForRoadType(ERoadType Type) const;
};