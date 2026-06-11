#include "EndlessMapManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"

AEndlessMapManager::AEndlessMapManager()
{
    PrimaryActorTick.bCanEverTick = false;

    GroundScale_Meters = FVector2D(10.0f, 10.0f);
    MaxBlocks = 10;
    CheckInterval = 0.5f;

    // 全局安全区默认值 (20米范围内干干净净)
    InitialSafeZoneRadius_Meters = 20.0f;

    // 建筑生成默认值
    BuildingSpacing_Meters = FVector2D(2.0f, 5.0f);
    EdgeMargin_Meters = FVector2D(1.0f, 2.0f);
    MaxBuildingsPerChunk = 15;

    CurrentPlayerGrid = FIntPoint(-99999, -99999);
    LastMoveDir = FIntPoint(0, 0);

    NavInvokerComp = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvokerComp"));
    NavInvokerComp->SetGenerationRadii(3000.f, 5000.f);

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

    // 自动修正策划填写的路径格式为 /Game/...
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

    // ==========================================
    // 地块生成完后，触发防穿模建筑生成算法
    // ==========================================
    SpawnArchitecturesOnChunk(ChunkActor);
}

// ==========================================
// 【新增核心算法】：判断是否在全局安全区内
// ==========================================
bool AEndlessMapManager::IsInInitialSafeZone(const FBox2D& Bounds2D) const
{
    // 如果策划配了 0，代表不需要安全区
    if (InitialSafeZoneRadius_Meters <= 0.0f) return false;

    float SafeRadiusCM = InitialSafeZoneRadius_Meters * 100.0f;

    // 我们约定原点 (0,0) 为玩家初始位置
    FVector2D Origin(0.0f, 0.0f);

    // 计算从原点到目标 2D 包围盒的最近点
    // 数学原理：通过将原点坐标钳制在矩形边界内，得到的就是矩形上离原点最近的那个点
    FVector2D ClosestPoint(
        FMath::Clamp(Origin.X, Bounds2D.Min.X, Bounds2D.Max.X),
        FMath::Clamp(Origin.Y, Bounds2D.Min.Y, Bounds2D.Max.Y)
    );

    // 计算最近点到原点的距离平方，如果小于安全半径的平方，说明矩形侵入了圆圈！
    return ClosestPoint.SizeSquared() < (SafeRadiusCM * SafeRadiusCM);
}

void AEndlessMapManager::SpawnArchitecturesOnChunk(AActor* ChunkActor)
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

    TArray<FBox2D> OccupiedBoxes;

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

        float TotalExtentX = RotatedExtentX + (ActualSpacing / 2.0f);
        float TotalExtentY = RotatedExtentY + (ActualSpacing / 2.0f);

        float MinLocX = ChunkSafeBox.Min.X + TotalExtentX;
        float MaxLocX = ChunkSafeBox.Max.X - TotalExtentX;
        float MinLocY = ChunkSafeBox.Min.Y + TotalExtentY;
        float MaxLocY = ChunkSafeBox.Max.Y - TotalExtentY;

        if (MinLocX > MaxLocX || MinLocY > MaxLocY) continue;

        FVector2D RandomLoc2D(FMath::RandRange(MinLocX, MaxLocX), FMath::RandRange(MinLocY, MaxLocY));

        FBox2D CandidateBox(
            RandomLoc2D - FVector2D(TotalExtentX, TotalExtentY),
            RandomLoc2D + FVector2D(TotalExtentX, TotalExtentY)
        );

        // ==========================================
        // 【新增检查 1】：验证是否闯入了全局初始安全区
        // 这段逻辑非常通用，后期加树木/怪物/道具生成时，直接加这一句代码就行
        // ==========================================
        if (IsInInitialSafeZone(CandidateBox))
        {
            continue; // 如果侵入原点圆圈，直接抛弃本次生成！
        }

        // ==========================================
        // 检查 2：防穿模检测（和地块上现存的其他建筑）
        // ==========================================
        bool bOverlaps = false;
        for (const FBox2D& ExistingBox : OccupiedBoxes)
        {
            if (CandidateBox.Intersect(ExistingBox))
            {
                bOverlaps = true;
                break;
            }
        }

        if (!bOverlaps)
        {
            OccupiedBoxes.Add(CandidateBox);

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
}