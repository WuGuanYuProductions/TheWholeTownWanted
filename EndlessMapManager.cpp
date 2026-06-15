#include "EndlessMapManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

AEndlessMapManager::AEndlessMapManager()
{
	PrimaryActorTick.bCanEverTick = false;

	// 【核心控制：区块大小】由于路网拉大（30m），区块大小也默认由 10m 增大到 50m
	// 50x50米的区块能够完美容纳 30 米网格的马路，并在网格内生成数栋房子
	GroundScale_Meters = FVector2D(50.0f, 50.0f);
	MaxBlocks = 10;
	CheckInterval = 0.5f;

	InitialSafeZoneRadius_Meters = 20.0f;

	// 建筑默认参数设置
	BuildingSpacing_Meters = FVector2D(2.0f, 5.0f);
	EdgeMargin_Meters = FVector2D(1.0f, 2.0f);
	BuildingToRoadDistance_Meters = 0.5f; // 默认建筑物离道路 0.5 米，现在可支持设为 0.1 以下
	MaxBuildingsPerChunk = 25; // 增大了区块面积，所以适度调高单个区块的可承载建筑上限

	CurrentPlayerGrid = FIntPoint(-99999, -99999);
	LastMoveDir = FIntPoint(0, 0);

	NavInvokerComp = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvokerComp"));
	NavInvokerComp->SetGenerationRadii(3000.f, 5000.f);

	// 实例化路网组件
	RoadNetworkComp = CreateDefaultSubobject<URoadNetworkComponent>(TEXT("RoadNetworkComp"));

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
		ActorToDestroy->Destroy(); // 销毁区块 Actor，挂载其上的路网组件与建筑会自动一并销毁
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

	// 1. 创建区块 Actor (Ground 容器)
	AActor* ChunkActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), ChunkCenter, FRotator::ZeroRotator);
	USceneComponent* RootComp = NewObject<USceneComponent>(ChunkActor);
	ChunkActor->SetRootComponent(RootComp);
	RootComp->RegisterComponent();
	ChunkActor->SetActorLocation(ChunkCenter);

	// 2. 创建 Ground 静态网格组件
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

	// 计算区块的 2D 范围
	FBox2D ChunkBounds(
		FVector2D(ChunkCenter.X - TargetCM_X / 2.f, ChunkCenter.Y - TargetCM_Y / 2.f),
		FVector2D(ChunkCenter.X + TargetCM_X / 2.f, ChunkCenter.Y + TargetCM_Y / 2.f)
	);

	// 3. 在 Ground 上生成路网，并返回路网的 2D 占用盒子
	TArray<FBox2D> RoadBoxes;
	if (RoadNetworkComp)
	{
		RoadBoxes = RoadNetworkComp->GenerateRoadNetworkOnChunk(ChunkActor, ChunkBounds);
	}

	// 4. 生成建筑并传入路网占用盒子以控制避让
	SpawnArchitecturesOnChunk(ChunkActor, RoadBoxes);
}

bool AEndlessMapManager::IsInInitialSafeZone(const FBox2D& Bounds2D) const
{
	if (InitialSafeZoneRadius_Meters <= 0.0f) return false;

	float SafeRadiusCM = InitialSafeZoneRadius_Meters * 100.0f;
	FVector2D Origin(0.0f, 0.0f);

	FVector2D ClosestPoint(
		FMath::Clamp(Origin.X, Bounds2D.Min.X, Bounds2D.Max.X),
		FMath::Clamp(Origin.Y, Bounds2D.Min.Y, Bounds2D.Max.Y)
	);

	return ClosestPoint.SizeSquared() < (SafeRadiusCM * SafeRadiusCM);
}

void AEndlessMapManager::SpawnArchitecturesOnChunk(AActor* ChunkActor, const TArray<FBox2D>& RoadOccupiedBoxes)
{
	if (LoadedArchitectures.Num() == 0 || !ChunkActor) return;

	float SafeScaleX = FMath::Max(0.01f, GroundScale_Meters.X);
	float SafeScaleY = FMath::Max(0.01f, GroundScale_Meters.Y);
	float ChunkSizeX_CM = SafeScaleX * 100.0f;
	float ChunkSizeY_CM = SafeScaleY * 100.0f;

	FVector2D ChunkCenter2D(ChunkActor->GetActorLocation().X, ChunkActor->GetActorLocation().Y);

	float ActualEdgeMargin = FMath::RandRange(EdgeMargin_Meters.X, EdgeMargin_Meters.Y) * 100.0f;

	float SafeHalfX = (ChunkSizeX_CM / 2.0f) - ActualEdgeMargin;
	float SafeHalfY = (ChunkSizeY_CM / 2.0f) - ActualEdgeMargin;

	if (SafeHalfX <= 0 || SafeHalfY <= 0) return;

	FBox2D ChunkSafeBox(
		ChunkCenter2D - FVector2D(SafeHalfX, SafeHalfY),
		ChunkCenter2D + FVector2D(SafeHalfX, SafeHalfY)
	);

	// 1. 将米转换为厘米
	float RoadBufferCM = BuildingToRoadDistance_Meters * 100.0f;

	// 2. 将路网占用盒进行“外扩”后存入单独的列表中
	// 该列表【只】用于对候选建筑的【真实物理大小】进行碰撞检测，实现物理级别的完美避让
	TArray<FBox2D> ExpandedRoadBoxes;
	for (const FBox2D& RoadBox : RoadOccupiedBoxes)
	{
		FBox2D ExpandedBox(
			RoadBox.Min - FVector2D(RoadBufferCM, RoadBufferCM),
			RoadBox.Max + FVector2D(RoadBufferCM, RoadBufferCM)
		);
		ExpandedRoadBoxes.Add(ExpandedBox);
	}

	// 3. 另外声明一个独立的列表，存储已经生成建筑的【带额外建筑间距】的盒子
	// 该列表【只】用于建筑物与建筑物之间的防重叠与间距控制
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

		// 计算建筑间距
		float ActualSpacing = FMath::RandRange(BuildingSpacing_Meters.X, BuildingSpacing_Meters.Y) * 100.0f;

		// a) 建筑物的真实边界半长宽 (不包含建筑间距)
		float HalfMeshX = RotatedExtentX;
		float HalfMeshY = RotatedExtentY;

		// b) 建筑物的间距边界半长宽 (包含了建筑间距)
		float HalfSpacingX = RotatedExtentX + (ActualSpacing / 2.0f);
		float HalfSpacingY = RotatedExtentY + (ActualSpacing / 2.0f);

		// 为了防止穿出 Chunk，根据真实大小计算可用随机生成范围
		float MinLocX = ChunkSafeBox.Min.X + HalfMeshX;
		float MaxLocX = ChunkSafeBox.Max.X - HalfMeshX;
		float MinLocY = ChunkSafeBox.Min.Y + HalfMeshY;
		float MaxLocY = ChunkSafeBox.Max.Y - HalfMeshY;

		if (MinLocX > MaxLocX || MinLocY > MaxLocY) continue;

		FVector2D RandomLoc2D(FMath::RandRange(MinLocX, MaxLocX), FMath::RandRange(MinLocY, MaxLocY));

		// 创建两个独立的候选碰撞检测盒：
		// 1. 用于跟道路做检测的【真实物理大小盒】
		FBox2D CandidateMeshBox(
			RandomLoc2D - FVector2D(HalfMeshX, HalfMeshY),
			RandomLoc2D + FVector2D(HalfMeshX, HalfMeshY)
		);

		// 2. 用于跟其他建筑做检测的【带间距大小盒】
		FBox2D CandidateSpacingBox(
			RandomLoc2D - FVector2D(HalfSpacingX, HalfSpacingY),
			RandomLoc2D + FVector2D(HalfSpacingX, HalfSpacingY)
		);

		// 规则 1：检查是否在安全区内
		if (IsInInitialSafeZone(CandidateMeshBox))
		{
			continue;
		}

		// 规则 2：路网碰撞检查（完美过滤！此时与建筑的额外间距参数完全解耦）
		// 建筑物到路网边缘的实际物理距离，将绝对受控于策划配置的 BuildingToRoadDistance_Meters
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

		// 规则 3：建筑间防重叠与间距检查（使用带额外间距的碰撞盒子）
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

		// 两个检查均通过，确认生成，将带间距盒子加入列表
		SpawnedBuildingSpacingBoxes.Add(CandidateSpacingBox);

		UStaticMeshComponent* BuildingComp = NewObject<UStaticMeshComponent>(ChunkActor);
		BuildingComp->SetStaticMesh(SelectedMesh);
		BuildingComp->SetupAttachment(ChunkActor->GetRootComponent());

		FVector WorldLoc(RandomLoc2D.X, RandomLoc2D.Y, ChunkActor->GetActorLocation().Z + MeshZOffset);

		BuildingComp->SetWorldLocationAndRotation(WorldLoc, SpawnRotation);
		BuildingComp->SetCanEverAffectNavigation(true);
		BuildingComp->RegisterComponent();

		BuildingsSpawned++;
	}
}
