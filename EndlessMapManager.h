#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationInvokerComponent.h"
#include "EndlessMapManager.generated.h"

UCLASS()
class THEWHOLETOWNWANTED_API AEndlessMapManager : public AActor
{
	GENERATED_BODY()

public:
	AEndlessMapManager();

protected:
	virtual void BeginPlay() override;

public:
	// ==========================================
	// Ground Settings
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	UStaticMesh* GroundMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	UMaterialInterface* GroundMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	FVector2D GroundScale_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Ground")
	int32 MaxBlocks;

	// ==========================================
	// Global Limits - Safe Zone
	// ==========================================

	// Radius of the safe zone around the world origin (0,0) in meters. No architectures will spawn within this area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Global Limits")
	float InitialSafeZoneRadius_Meters;

	// ==========================================
	// Architecture Generation Settings
	// ==========================================

	// Directory path where architecture meshes are located (e.g., /Game/Environment/Buildings)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (RelativeToGameContentDir))
	FDirectoryPath ArchitecturePath;

	// Spacing between buildings (X = min, Y = max) in meters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D BuildingSpacing_Meters;

	// Margin from the edge of the chunk (X = min, Y = max) in meters
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D EdgeMargin_Meters;

	// Maximum number of buildings to spawn per chunk
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	int32 MaxBuildingsPerChunk;

	// ==========================================
	// Grid Check Settings
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Settings")
	float CheckInterval;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Navigation")
	UNavigationInvokerComponent* NavInvokerComp;

private:
	UPROPERTY()
	TMap<FIntPoint, AActor*> ActiveChunks;

	// Array to store loaded architecture static meshes
	UPROPERTY()
	TArray<UStaticMesh*> LoadedArchitectures;

	FIntPoint CurrentPlayerGrid;
	FIntPoint LastMoveDir;
	FTimerHandle MapUpdateTimer;

	UFUNCTION()
	void UpdateMap();

	void LoadArchitectures();
	void SpawnTileAtGrid(const FIntPoint& GridLocation);

	// Spawns architectures onto the specified chunk actor
	void SpawnArchitecturesOnChunk(AActor* ChunkActor);

	// Checks if a 2D bounding box falls within the initial safe zone
	bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;

	bool DestroyFarthestTile(const TArray<FIntPoint>& ProtectedGrids);

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorld(const FIntPoint& GridLocation) const;
};
