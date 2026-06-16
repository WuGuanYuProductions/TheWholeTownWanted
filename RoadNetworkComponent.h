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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (Tooltip = "The size of each grid cell in the road network (in centimeters). Larger values increase road spacing, leaving larger blocks for buildings."))
	float CellSize = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (ClampMin = "0", ClampMax = "100", Tooltip = "The probability of road blocking (0-100). Higher values result in more dead ends and turns."))
	int32 BlockChance = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (Tooltip = "Seed for generation. Set to -1 for a random seed."))
	int32 Seed = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road", meta = (Tooltip = "The physical width of the road (in centimeters). Buildings will spawn outside of this width."))
	float RoadWidth = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "The probability of a 4-way intersection (Cross) spawning."))
	float CrossChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "The probability of a 3-way intersection (T-Road) spawning."))
	float TRoadChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Probability", meta = (ClampMin = "0.0", ClampMax = "1.0", Tooltip = "The probability of a parking spot spawning at dead ends."))
	float ParkingChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual", meta = (Tooltip = "Static mesh asset representing the road segments."))
	UStaticMesh* RoadMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Road|Visual", meta = (Tooltip = "Material interface to apply to the generated road meshes."))
	UMaterialInterface* RoadMaterial;

	TArray<FBox2D> GenerateRoadNetworkOnChunk(AActor* ChunkActor, const FBox2D& ChunkBounds);

private:
	uint32 GetGridHash(int32 X, int32 Y) const;
	uint32 GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	bool DoesNodeExistAtGrid(int32 X, int32 Y) const;
	bool IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const;

	ERoadType GetInitialRoadTypeAt(int32 X, int32 Y) const;
};
