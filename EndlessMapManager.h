#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationInvokerComponent.h"
#include "RoadNetworkComponent.h"
#include "EndlessMapManager.generated.h"

class UPropGenerator;

UCLASS()
class THEWHOLETOWNWANTED_API AEndlessMapManager : public AActor
{
	GENERATED_BODY()

public:
	AEndlessMapManager();

protected:
	virtual void BeginPlay() override;

public:
	/** The static mesh asset used as the ground floor for each map tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	UStaticMesh* GroundMesh;

	/** The material applied to the ground static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	UMaterialInterface* GroundMaterial;

	/** The size of each map tile in meters (X and Y dimensions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	FVector2D GroundScale_Meters;

	/** The maximum number of map tiles active at the same time. Older tiles will be destroyed when exceeding this limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	int32 MaxBlocks;

	/** Radius of the safe zone around the world origin (0,0) in meters where no architectures will spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Global Limits")
	float InitialSafeZoneRadius_Meters;

	/** The directory path in the content folder containing the building static meshes to load automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (RelativeToGameContentDir))
	FDirectoryPath ArchitecturePath;

	/** Min and Max spacing distance between spawned buildings (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D BuildingSpacing_Meters;

	/** Min and Max margin from the edge of the tile where no buildings should spawn (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D EdgeMargin_Meters;

	/** Minimum clearance distance to keep between buildings and roads (in meters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (ClampMin = "0.0"))
	float BuildingToRoadDistance_Meters;

	/** Maximum number of buildings to attempt to spawn per map tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	int32 MaxBuildingsPerChunk;

	/** How often (in seconds) the manager checks if the player has crossed tile boundaries to update the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Settings")
	float CheckInterval;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Navigation")
	UNavigationInvokerComponent* NavInvokerComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Road")
	URoadNetworkComponent* RoadNetworkComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Prop")
	UPropGenerator* PropGeneratorComp;

private:
	UPROPERTY()
	TMap<FIntPoint, AActor*> ActiveChunks;

	UPROPERTY()
	TArray<UStaticMesh*> LoadedArchitectures;

	FIntPoint CurrentPlayerGrid;
	FIntPoint LastMoveDir;
	FTimerHandle MapUpdateTimer;

	UFUNCTION()
	void UpdateMap();

	void LoadArchitectures();
	void SpawnTileAtGrid(const FIntPoint& GridLocation);

	TArray<FBox2D> SpawnArchitecturesOnChunk(AActor* ChunkActor, const TArray<FBox2D>& RoadOccupiedBoxes);

	bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;
	bool DestroyFarthestTile(const TArray<FIntPoint>& ProtectedGrids);

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorld(const FIntPoint& GridLocation) const;
};
