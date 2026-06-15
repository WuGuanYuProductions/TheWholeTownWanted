#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoadNetworkComponent.generated.h"

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
	// ==================== Base Road Network Settings ====================

	// [Core] The size of each grid cell in the road network (in centimeters).
	// Larger values increase road spacing, leaving larger blocks for buildings. (Default: 3000.f)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float CellSize = 3000.f;

	// The probability of road blocking (0-100). Higher values result in more dead ends and turns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BlockChance = 20;

	// Seed for generation. Set to -1 for a random seed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	int32 Seed = -1;

	// The physical width of the road (in centimeters). Buildings will spawn outside of this width.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road")
	float RoadWidth = 300.f;

	// ==================== Probability Settings ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrossChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TRoadChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ParkingChance = 0.35f;

	// ==================== Visual Assets ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual")
	UStaticMesh* RoadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual")
	UMaterialInterface* RoadMaterial;

	TArray<FBox2D> GenerateRoadNetworkOnChunk(AActor* ChunkActor, const FBox2D& ChunkBounds);

private:
	uint32 GetGridHash(int32 X, int32 Y) const;
	uint32 GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	bool DoesNodeExistAtGrid(int32 X, int32 Y) const;
	bool IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	ERoadType GetInitialRoadTypeAt(int32 X, int32 Y) const;
};
