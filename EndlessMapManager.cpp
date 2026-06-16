#include "EndlessMapManager.h"
#include "PropGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

AEndlessMapManager::AEndlessMapManager()
{
	PrimaryActorTick.bCanEverTick = false;

	GroundScale_Meters = FVector2D(50.0f, 50.0f);
	MaxBlocks = 10;
	CheckInterval = 0.5f;

	InitialSafeZoneRadius_Meters = 20.0f;

	BuildingSpacing_Meters = FVector2D(2.0f, 5.0f);
	EdgeMargin_Meters = FVector2D(1.0f, 2.0f);
	BuildingToRoadDistance_Meters = 0.5f;
	MaxBuildingsPerChunk = 25;

	CurrentPlayerGrid = FIntPoint(-99999, -99999);
	LastMoveDir = FIntPoint(0, 0);

	NavInvokerComp = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvokerComp"));
	NavInvokerComp->SetGenerationRadii(3000.f, 5000.f);

	RoadNetworkComp = CreateDefaultSubobject<URoadNetworkComponent>(TEXT("RoadNetworkComp"));

	PropGeneratorComp = CreateDefaultSubobject<UPropGenerator>(TEXT("PropGeneratorComp"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeVisualAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeVisualAsset.Succeeded())
	{
		GroundMesh = CubeVisualAsset.Object;
	}
}

void AEndlessMapManager::BeginPlay()
{
	Super::BeginPlay();

	LoadArchitectures();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (PlayerPawn)
	{
		CurrentPlayerGrid = WorldToGrid(PlayerPawn->GetActorLocation());
		SetActorLocation(PlayerPawn->GetActorLocation());
	}
	else
	{
		CurrentPlayerGrid = FIntPoint(0, 0);
	}

	LastMoveDir = FIntPoint(0, 0);

	UpdateMap();

	GetWorld()->GetTimerManager().SetTimer(MapUpdateTimer, this, &AEndlessMapManager::UpdateMap, CheckInterval, true);
}

void AEndlessMapManager::LoadArchitectures()
{
	if (ArchitecturePath.Path.IsEmpty()) return;

	FString FinalPath = ArchitecturePath.Path;
	FinalPath.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (!FinalPath.StartsWith(TEXT("/")))
	{
		FinalPath = TEXT("/Game/") + FinalPath;
	}
	else if (FinalPath.StartsWith(TEXT("/Content/")))
	{
		FinalPath = FinalPath.Replace(TEXT("/Content/"), TEXT("/Game/"));
	}

	if (FinalPath.EndsWith(TEXT("/")))
	{
		FinalPath.LeftChopInline(1);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> AssetDataArray;

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*FinalPath));
	Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = false;

	AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

	for (const FAssetData& AssetData : AssetDataArray)
	{
		UStaticMesh* LoadedMesh = Cast<UStaticMesh>(AssetData.GetAsset());
		if (LoadedMesh)
		{
			LoadedArchitectures.Add(LoadedMesh);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Loaded %d architectures from auto-fixed path: %s"), LoadedArchitectures.Num(), *FinalPath);
}

void AEndlessMapManager::UpdateMap()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player) return;

	SetActorLocation(Player->GetActorLocation());

	FIntPoint NewPlayerGrid = WorldToGrid(Player->GetActorLocation());

	if (NewPlayerGrid != CurrentPlayerGrid)
	{
		LastMoveDir.X = FMath::Clamp(NewPlayerGrid.X - CurrentPlayerGrid.X, -1, 1);
		LastMoveDir.Y = FMath::Clamp(NewPlayerGrid.Y - CurrentPlayerGrid.Y, -1, 1);
		CurrentPlayerGrid = NewPlayerGrid;
	}

	int32 MinX = (LastMoveDir.X < 0) ? -2 : -1;
	int32 MaxX = (LastMoveDir.X > 0) ? 2 : 1;
	int32 MinY = (LastMoveDir.Y < 0) ? -2 : -1;
	int32 MaxY = (LastMoveDir.Y > 0) ? 2 : 1;

	TArray<FIntPoint> GridsToGenerate;
	for (int32 x = MinX; x <= MaxX; ++x)
	{
		for (int32 y = MinY; y <= MaxY; ++y)
		{
			GridsToGenerate.Add(CurrentPlayerGrid + FIntPoint(x, y));
		}
	}

	for (const FIntPoint& Grid : GridsToGenerate)
	{
		if (!ActiveChunks.Contains(Grid))
		{
			SpawnTileAtGrid(Grid);
		}
	}

	int32 SafeMaxBlocks = FMath::Max(MaxBlocks, GridsToGenerate.Num());

	while (ActiveChunks.Num() > SafeMaxBlocks)
	{
		if (!DestroyFarthestTile(GridsToGenerate))
		{
			break;
		}
	}
}

bool AEndlessMapManager::DestroyFarthestTile(const TArray<FIntPoint>& ProtectedGrids)
{
	FIntPoint FarthestGrid;
	float MaxScore = -999999.0f;
	AActor* ActorToDestroy = nullptr;
	bool bFound = false;

	for (const auto& Pair : ActiveChunks)
	{
		if (ProtectedGrids.Contains(Pair.Key)) continue;

		FIntPoint Grid = Pair.Key;
		FVector2D Offset(Grid.X - CurrentPlayerGrid.X, Grid.Y - CurrentPlayerGrid.Y);
		FVector2D Dir(LastMoveDir.X, LastMoveDir.Y);

		float Score = Offset.SizeSquared() - (FVector2D::DotProduct(Offset, Dir) * 2.0f);

		if (Score > MaxScore)
		{
			MaxScore = Score;
			FarthestGrid = Grid;
			ActorToDestroy = Pair.Value;
			bFound = true;
		}
	}

	if (bFound && ActorToDestroy)
	{
		ActorToDestroy->Destroy();
		ActiveChunks.Remove(FarthestGrid);
		return true;
	}

	return false;
}

FIntPoint AEndlessMapManager::WorldToGrid(const FVector& WorldLocation) const
{
	float SafeScaleX = FMath::Max(0.01f, GroundScale_Meters.X);
	float SafeScaleY = FMath::Max(0.01f, GroundScale_Meters.Y);

	float GridCM_X = SafeScaleX * 100.0f;
	float GridCM_Y = SafeScaleY * 100.0f;
	return FIntPoint(FMath::RoundToInt(WorldLocation.X / GridCM_X), FMath::RoundToInt(WorldLocation.Y / GridCM_Y));
}

FVector AEndlessMapManager::GridToWorld(const FIntPoint& GridLocation) const
{
	float SafeScaleX = FMath::Max(0.01f, GroundScale_Meters.X);
	float SafeScaleY = FMath::Max(0.01f, GroundScale_Meters.Y);

	float GridCM_X = SafeScaleX * 100.0f;
	float GridCM_Y = SafeScaleY * 100.0f;
	return FVector(GridLocation.X * GridCM_X, GridLocation.Y * GridCM_Y, 0.0f);
}

void AEndlessMapManager::SpawnTileAtGrid(const FIntPoint& GridLocation)
{
	FVector ChunkCenter = GridToWorld(GridLocation);

	AActor* ChunkActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ChunkCenter, FRotator::ZeroRotator);
	USceneComponent* RootComp = NewObject<USceneComponent>(ChunkActor);
	ChunkActor->SetRootComponent(RootComp);
	RootComp->RegisterComponent();
	ChunkActor->SetActorLocation(ChunkCenter);

	UStaticMeshComponent* TileComp = NewObject<UStaticMeshComponent>(ChunkActor);
	TileComp->SetupAttachment(RootComp);

	if (GroundMesh)
	{
		TileComp->SetStaticMesh(GroundMesh);
	}
	if (GroundMaterial)
	{
		TileComp->SetMaterial(0, GroundMaterial);
	}

	TileComp->SetCanEverAffectNavigation(true);

	float SafeScaleX = FMath::Max(0.01f, GroundScale_Meters.X);
	float SafeScaleY = FMath::Max(0.01f, GroundScale_Meters.Y);

	float TargetCM_X = SafeScaleX * 100.0f;
	float TargetCM_Y = SafeScaleY * 100.0f;

	float NativeSizeX = 100.0f;
	float NativeSizeY = 100.0f;
	float NativeSizeZ = 100.0f;

	if (GroundMesh)
	{
		FBox Bounds = GroundMesh->GetBoundingBox();
		FVector MeshSize = Bounds.GetSize();
		if (MeshSize.X > 0.1f) NativeSizeX = MeshSize.X;
		if (MeshSize.Y > 0.1f) NativeSizeY = MeshSize.Y;
		if (MeshSize.Z > 0.1f) NativeSizeZ = MeshSize.Z;
	}

	float RealScaleX = TargetCM_X / NativeSizeX;
	float RealScaleY = TargetCM_Y / NativeSizeY;
	float RealScaleZ = 1.0f;

	TileComp->SetRelativeScale3D(FVector(RealScaleX, RealScaleY, RealScaleZ));

	float OffsetZ = 0.0f;
	if (NativeSizeZ > 1.0f)
	{
		OffsetZ = -(NativeSizeZ * RealScaleZ) / 2.0f;
	}
	TileComp->SetRelativeLocation(FVector(0.f, 0.f, OffsetZ));
	TileComp->RegisterComponent();

	ActiveChunks.Add(GridLocation, ChunkActor);

	FBox2D ChunkBounds(
		FVector2D(ChunkCenter.X - TargetCM_X / 2.f, ChunkCenter.Y - TargetCM_Y / 2.f),
		FVector2D(ChunkCenter.X + TargetCM_X / 2.f, ChunkCenter.Y + TargetCM_Y / 2.f)
	);

	TArray<FBox2D> RoadBoxes;
	if (RoadNetworkComp)
	{
		RoadBoxes = RoadNetworkComp->GenerateRoadNetworkOnChunk(ChunkActor, ChunkBounds);
	}

	TArray<FBox2D> BuildingMeshBoxes = SpawnArchitecturesOnChunk(ChunkActor, RoadBoxes);

	if (PropGeneratorComp)
	{
		PropGeneratorComp->GeneratePropsOnChunk(ChunkActor, ChunkBounds, RoadBoxes, BuildingMeshBoxes);
	}
}

bool AEndlessMapManager::IsInInitialSafeZone(const FBox2D& Bounds2D) const
{
	if (InitialSafeZoneRadius_Meters <= 0.0f) return false;

	float SafeRadiusCM = InitialSafeZoneRadius_Meters * 100.0f;
	FVector2D Origin(0.0f, 0.0f);

	FVector2D ClosestPoint(\
		FMath::Clamp(Origin.X, Bounds2D.Min.X, Bounds2D.Max.X), \
		FMath::Clamp(Origin.Y, Bounds2D.Min.Y, Bounds2D.Max.Y)\
	);

	return ClosestPoint.SizeSquared() < (SafeRadiusCM * SafeRadiusCM);
}

TArray<FBox2D> AEndlessMapManager::SpawnArchitecturesOnChunk(AActor* ChunkActor, const TArray<FBox2D>& RoadOccupiedBoxes)
{
	TArray<FBox2D> SpawnedBuildingMeshBoxes;

	if (LoadedArchitectures.Num() == 0 || !ChunkActor) return SpawnedBuildingMeshBoxes;

	float SafeScaleX = FMath::Max(0.01f, GroundScale_Meters.X);
	float SafeScaleY = FMath::Max(0.01f, GroundScale_Meters.Y);
	float ChunkSizeX_CM = SafeScaleX * 100.0f;
	float ChunkSizeY_CM = SafeScaleY * 100.0f;

	FVector2D ChunkCenter2D(ChunkActor->GetActorLocation().X, ChunkActor->GetActorLocation().Y);

	float ActualEdgeMargin = FMath::RandRange(EdgeMargin_Meters.X, EdgeMargin_Meters.Y) * 100.0f;

	float SafeHalfX = (ChunkSizeX_CM / 2.0f) - ActualEdgeMargin;
	float SafeHalfY = (ChunkSizeY_CM / 2.0f) - ActualEdgeMargin;

	if (SafeHalfX <= 0 || SafeHalfY <= 0) return SpawnedBuildingMeshBoxes;

	FBox2D ChunkSafeBox(\
		ChunkCenter2D - FVector2D(SafeHalfX, SafeHalfY), \
		ChunkCenter2D + FVector2D(SafeHalfX, SafeHalfY)\
	);

	float RoadBufferCM = BuildingToRoadDistance_Meters * 100.0f;

	TArray<FBox2D> ExpandedRoadBoxes;
	for (const FBox2D& RoadBox : RoadOccupiedBoxes)
	{
		FBox2D ExpandedBox(\
			RoadBox.Min - FVector2D(RoadBufferCM, RoadBufferCM), \
			RoadBox.Max + FVector2D(RoadBufferCM, RoadBufferCM)\
		);
		ExpandedRoadBoxes.Add(ExpandedBox);
	}

	TArray<FBox2D> SpawnedBuildingSpacingBoxes;

	int32 BuildingsSpawned = 0;
	int32 MaxAttempts = MaxBuildingsPerChunk * 5;

	for (int32 i = 0; i < MaxAttempts && BuildingsSpawned < MaxBuildingsPerChunk; ++i)
	{
		UStaticMesh* SelectedMesh = LoadedArchitectures[FMath::RandRange(0, LoadedArchitectures.Num() - 1)];
		if (!SelectedMesh) continue;

		FBox Bounds = SelectedMesh->GetBoundingBox();
		FVector MeshExtents = Bounds.GetExtent();
		float MeshZOffset = -Bounds.Min.Z;

		int32 RotIndex = FMath::RandRange(0, 3);
		float Yaw = RotIndex * 90.0f;
		FRotator SpawnRotation(0.f, Yaw, 0.f);

		float RotatedExtentX = (RotIndex % 2 == 0) ? MeshExtents.X : MeshExtents.Y;
		float RotatedExtentY = (RotIndex % 2 == 0) ? MeshExtents.Y : MeshExtents.X;

		float ActualSpacing = FMath::RandRange(BuildingSpacing_Meters.X, BuildingSpacing_Meters.Y) * 100.0f;

		float HalfMeshX = RotatedExtentX;
		float HalfMeshY = RotatedExtentY;

		float HalfSpacingX = RotatedExtentX + (ActualSpacing / 2.0f);
		float HalfSpacingY = RotatedExtentY + (ActualSpacing / 2.0f);

		float MinLocX = ChunkSafeBox.Min.X + HalfMeshX;
		float MaxLocX = ChunkSafeBox.Max.X - HalfMeshX;
		float MinLocY = ChunkSafeBox.Min.Y + HalfMeshY;
		float MaxLocY = ChunkSafeBox.Max.Y - HalfMeshY;

		if (MinLocX > MaxLocX || MinLocY > MaxLocY) continue;

		FVector2D RandomLoc2D(FMath::RandRange(MinLocX, MaxLocX), FMath::RandRange(MinLocY, MaxLocY));

		FBox2D CandidateMeshBox(\
			RandomLoc2D - FVector2D(HalfMeshX, HalfMeshY), \
			RandomLoc2D + FVector2D(HalfMeshX, HalfMeshY)\
		);

		FBox2D CandidateSpacingBox(\
			RandomLoc2D - FVector2D(HalfSpacingX, HalfSpacingY), \
			RandomLoc2D + FVector2D(HalfSpacingX, HalfSpacingY)\
		);

		if (IsInInitialSafeZone(CandidateMeshBox))
		{
			continue;
		}

		bool bOverlapsRoad = false;
		for (const FBox2D& RoadBox : ExpandedRoadBoxes)
		{
			if (CandidateMeshBox.Intersect(RoadBox))
			{
				bOverlapsRoad = true;
				break;
			}
		}
		if (bOverlapsRoad) continue;

		bool bOverlapsBuilding = false;
		for (const FBox2D& ExistingBox : SpawnedBuildingSpacingBoxes)
		{
			if (CandidateSpacingBox.Intersect(ExistingBox))
			{
				bOverlapsBuilding = true;
				break;
			}
		}
		if (bOverlapsBuilding) continue;

		SpawnedBuildingSpacingBoxes.Add(CandidateSpacingBox);
		SpawnedBuildingMeshBoxes.Add(CandidateMeshBox);

		UStaticMeshComponent* BuildingComp = NewObject<UStaticMeshComponent>(ChunkActor);
		BuildingComp->SetStaticMesh(SelectedMesh);
		BuildingComp->SetupAttachment(ChunkActor->GetRootComponent());

		FVector WorldLoc(RandomLoc2D.X, RandomLoc2D.Y, ChunkActor->GetActorLocation().Z + MeshZOffset);

		BuildingComp->SetWorldLocationAndRotation(WorldLoc, SpawnRotation);
		BuildingComp->SetCanEverAffectNavigation(true);
		BuildingComp->RegisterComponent();

		BuildingsSpawned++;
	}

	return SpawnedBuildingMeshBoxes;
}
