#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "NavigationInvokerComponent.h"
#include "RoadNetworkComponent.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Global Limits")
	float InitialSafeZoneRadius_Meters;

	// ==========================================
	// Architecture Generation Settings
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (RelativeToGameContentDir))
	FDirectoryPath ArchitecturePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D BuildingSpacing_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	FVector2D EdgeMargin_Meters;

	// 建筑物离道路的最小间距（单位：米，可设置为 0.1 等极小值实现精准贴合）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (ClampMin = "0.0"))
	float BuildingToRoadDistance_Meters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
	int32 MaxBuildingsPerChunk;

	// ==========================================
	// Grid Check Settings
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Settings")
	float CheckInterval;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Navigation")
	UNavigationInvokerComponent* NavInvokerComp;

	// ==========================================
	// Road Network Component
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Road")
	URoadNetworkComponent* RoadNetworkComp;

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

	// 支持接收路网占用盒列表，并在避让时结合策划配置进行物理精准的外扩检测
	void SpawnArchitecturesOnChunk(AActor* ChunkActor, const TArray<FBox2D>& RoadOccupiedBoxes);

	bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;
	bool DestroyFarthestTile(const TArray<FIntPoint>& ProtectedGrids);

	FIntPoint WorldToGrid(const FVector& WorldLocation) const;
	FVector GridToWorld(const FIntPoint& GridLocation) const;
};
