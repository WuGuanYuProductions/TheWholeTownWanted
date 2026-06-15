#include "PropGenerator.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"

UPropGenerator::UPropGenerator()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 默认参数初始化
	PropSpacing_Meters = FVector2D(1.0f, 3.0f);
	EdgeMargin_Meters = FVector2D(0.5f, 1.5f);
	PropToRoadDistance_Meters = 0.2f;      // 默认离马路 0.2 米
	PropToBuildingDistance_Meters = 0.3f;  // 默认离建筑 0.3 米
	MaxPropsPerChunk = 15;
	InitialSafeZoneRadius_Meters = 15.0f;  // 【需求3】默认 15 米安全区
}

void UPropGenerator::BeginPlay()
{
	Super::BeginPlay();
	LoadProps();
}

void UPropGenerator::LoadProps()
{
	if (PropPath.Path.IsEmpty()) return;

	FString FinalPath = PropPath.Path;
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
			LoadedProps.Add(LoadedMesh);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PropGenerator: Loaded %d props from auto-fixed path: %s"), LoadedProps.Num(), *FinalPath);
}

bool UPropGenerator::IsInInitialSafeZone(const FBox2D& Bounds2D) const
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

void UPropGenerator::GeneratePropsOnChunk(
	AActor* ChunkActor,
	const FBox2D& ChunkBounds,
	const TArray<FBox2D>& RoadOccupiedBoxes,
	const TArray<FBox2D>& BuildingOccupiedBoxes)
{
	if (LoadedProps.Num() == 0 || !ChunkActor) return;

	float ActualEdgeMargin = FMath::RandRange(EdgeMargin_Meters.X, EdgeMargin_Meters.Y) * 100.0f;

	// 计算当前区块内排除边缘后，合法的道具采样生成 2D 包围盒
	FBox2D ChunkSafeBox(
		ChunkBounds.Min + FVector2D(ActualEdgeMargin, ActualEdgeMargin),
		ChunkBounds.Max - FVector2D(ActualEdgeMargin, ActualEdgeMargin)
	);

	if (ChunkSafeBox.Min.X >= ChunkSafeBox.Max.X || ChunkSafeBox.Min.Y >= ChunkSafeBox.Max.Y) return;

	// 1. 【需求1】外扩路网检测盒 (单位转换为厘米)
	float RoadBufferCM = PropToRoadDistance_Meters * 100.0f;
	TArray<FBox2D> ExpandedRoadBoxes;
	for (const FBox2D& RoadBox : RoadOccupiedBoxes)
	{
		FBox2D ExpandedBox(
			RoadBox.Min - FVector2D(RoadBufferCM, RoadBufferCM),
			RoadBox.Max + FVector2D(RoadBufferCM, RoadBufferCM)
		);
		ExpandedRoadBoxes.Add(ExpandedBox);
	}

	// 2. 【需求2】外扩建筑检测盒 (单位转换为厘米)
	float BuildingBufferCM = PropToBuildingDistance_Meters * 100.0f;
	TArray<FBox2D> ExpandedBuildingBoxes;
	for (const FBox2D& BuildingBox : BuildingOccupiedBoxes)
	{
		FBox2D ExpandedBox(
			BuildingBox.Min - FVector2D(BuildingBufferCM, BuildingBufferCM),
			BuildingBox.Max + FVector2D(BuildingBufferCM, BuildingBufferCM)
		);
		ExpandedBuildingBoxes.Add(ExpandedBox);
	}

	// 3. 道具与自身（已生成的道具）防重叠的盒子列表
	TArray<FBox2D> SpawnedPropSpacingBoxes;

	int32 PropsSpawned = 0;
	int32 MaxAttempts = MaxPropsPerChunk * 5;

	for (int32 i = 0; i < MaxAttempts && PropsSpawned < MaxPropsPerChunk; ++i)
	{
		UStaticMesh* SelectedMesh = LoadedProps[FMath::RandRange(0, LoadedProps.Num() - 1)];
		if (!SelectedMesh) continue;

		FBox Bounds = SelectedMesh->GetBoundingBox();
		FVector MeshExtents = Bounds.GetExtent();
		float MeshZOffset = -Bounds.Min.Z;

		int32 RotIndex = FMath::RandRange(0, 3);
		float Yaw = RotIndex * 90.0f;
		FRotator SpawnRotation(0.f, Yaw, 0.f);

		float RotatedExtentX = (RotIndex % 2 == 0) ? MeshExtents.X : MeshExtents.Y;
		float RotatedExtentY = (RotIndex % 2 == 0) ? MeshExtents.Y : MeshExtents.X;

		// 道具自身间距
		float ActualSpacing = FMath::RandRange(PropSpacing_Meters.X, PropSpacing_Meters.Y) * 100.0f;

		float HalfMeshX = RotatedExtentX;
		float HalfMeshY = RotatedExtentY;

		float HalfSpacingX = RotatedExtentX + (ActualSpacing / 2.0f);
		float HalfSpacingY = RotatedExtentY + (ActualSpacing / 2.0f);

		// 防止生成至外部，结合道具大小缩减随机采样区间
		float MinLocX = ChunkSafeBox.Min.X + HalfMeshX;
		float MaxLocX = ChunkSafeBox.Max.X - HalfMeshX;
		float MinLocY = ChunkSafeBox.Min.Y + HalfMeshY;
		float MaxLocY = ChunkSafeBox.Max.Y - HalfMeshY;

		if (MinLocX > MaxLocX || MinLocY > MaxLocY) continue;

		FVector2D RandomLoc2D(FMath::RandRange(MinLocX, MaxLocX), FMath::RandRange(MinLocY, MaxLocY));

		// 真实物理碰撞检测盒
		FBox2D CandidateMeshBox(
			RandomLoc2D - FVector2D(HalfMeshX, HalfMeshY),
			RandomLoc2D + FVector2D(HalfMeshX, HalfMeshY)
		);

		// 带自身防重叠间距的盒子
		FBox2D CandidateSpacingBox(
			RandomLoc2D - FVector2D(HalfSpacingX, HalfSpacingY),
			RandomLoc2D + FVector2D(HalfSpacingX, HalfSpacingY)
		);

		// 规则 1: 【需求3】安全区过滤
		if (IsInInitialSafeZone(CandidateMeshBox))
		{
			continue;
		}

		// 规则 2: 【需求1】路网碰撞检查
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

		// 规则 3: 【需求2】建筑碰撞检查
		bool bOverlapsBuilding = false;
		for (const FBox2D& BuildingBox : ExpandedBuildingBoxes)
		{
			if (CandidateMeshBox.Intersect(BuildingBox))
			{
				bOverlapsBuilding = true;
				break;
			}
		}
		if (bOverlapsBuilding) continue;

		// 规则 4: 道具自身重叠与间距检查
		bool bOverlapsProp = false;
		for (const FBox2D& ExistingBox : SpawnedPropSpacingBoxes)
		{
			if (CandidateSpacingBox.Intersect(ExistingBox))
			{
				bOverlapsProp = true;
				break;
			}
		}
		if (bOverlapsProp) continue;

		// 通过所有检测，执行生成
		SpawnedPropSpacingBoxes.Add(CandidateSpacingBox);

		UStaticMeshComponent* PropComp = NewObject<UStaticMeshComponent>(ChunkActor);
		PropComp->SetStaticMesh(SelectedMesh);
		PropComp->SetupAttachment(ChunkActor->GetRootComponent());

		FVector WorldLoc(RandomLoc2D.X, RandomLoc2D.Y, ChunkActor->GetActorLocation().Z + MeshZOffset);

		PropComp->SetWorldLocationAndRotation(WorldLoc, SpawnRotation);
		PropComp->SetCanEverAffectNavigation(true);
		PropComp->RegisterComponent();

		PropsSpawned++;
	}
}