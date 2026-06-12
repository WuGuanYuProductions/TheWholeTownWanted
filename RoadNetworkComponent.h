#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadNetworkComponent.generated.h"

// Road type enum
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

// Temporary struct for road network nodes generated at runtime
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

	// Size of each grid cell (in centimeters, e.g., 800.f = 8 meters)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float CellSize = 800.f;

	// Generation half-radius (in grid units. 4 means 4 * 8m = 32m radius, covering a total area of 64m * 64m)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	int32 GridRadius = 4;

	// Road block probability (0-100). Higher values lead to more dead ends and turns, resulting in a sparser road network.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlockChance = 20;

	// Seed for procedural generation. If set to -1, a unique random seed is automatically generated on BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	int32 Seed = -1;

	// ==================== Probability Settings ====================

	// Generation probability of a four-way intersection (Cross) (0.0 to 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrossChance = 1.0f; // Default is 1.0 (100% probability)

	// Generation probability of a T-junction (T-Road) (0.0 to 1.0)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TRoadChance = 1.0f; // Default is 1.0 (100% probability)

	// Probability of converting a dead end (End) into a parking lot (Parking) (0.0 to 1.0), replacing the original hardcoded 35%
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ParkingChance = 0.35f; // Default is 0.35 (35% probability)

	// ==================== Debug Settings ====================

	// Radius of the node sphere drawn during debugging
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Debug")
	float NodeSphereRadius = 30.f;

private:
	// Deterministic pseudo-random hash algorithm incorporating the seed
	uint32 GetGridHash(int32 X, int32 Y) const;
	uint32 GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// Check if a road network node exists at the specified grid coordinates
	bool DoesNodeExistAtGrid(int32 X, int32 Y) const;

	// Check if two adjacent nodes are physically connected (not blocked by BlockChance)
	bool IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	// Core helper method: Safely calculate the base road type at the specified coordinates (excluding StraightToNode conversion)
	ERoadType GetInitialRoadTypeAt(int32 X, int32 Y) const;

	// Get player location, generate local road network, and perform real-time debug drawing
	void UpdateAndDrawRoadNetwork(const FVector& PlayerLocation);

	FColor GetColorForRoadType(ERoadType Type) const;
	FString GetTextForRoadType(ERoadType Type) const;
};
