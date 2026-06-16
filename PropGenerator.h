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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (RelativeToGameContentDir, ToolTip = "Path to the directory containing prop static meshes."))
	FDirectoryPath PropPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ToolTip = "Additional spacing range between props (in meters)."))
	FVector2D PropSpacing_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ToolTip = "Reserved edge margin from the chunk boundary (in meters)."))
	FVector2D EdgeMargin_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ClampMin = "0.0", ToolTip = "Physical avoidance distance from props to roads (in meters)."))
	float PropToRoadDistance_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ClampMin = "0.0", ToolTip = "Physical avoidance distance from props to buildings (in meters)."))
	float PropToBuildingDistance_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation", meta = (ToolTip = "Maximum number of props to generate per chunk."))
	int32 MaxPropsPerChunk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop Generation|Global Limits", meta = (ToolTip = "Radius of the independent safe zone around the starting area where no props will spawn (in meters)."))
	float InitialSafeZoneRadius_Meters;

	void GeneratePropsOnChunk(
		AActor* ChunkActor,
		const FBox2D& ChunkBounds,
		const TArray<FBox2D>& RoadOccupiedBoxes,
		const TArray<FBox2D>& BuildingOccupiedBoxes
	);

private:
	UPROPERTY()
	TArray<UStaticMesh*> LoadedProps;

	void LoadProps();

	bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;
};
