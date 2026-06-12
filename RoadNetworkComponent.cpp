#include "RoadNetworkComponent.h"
#include "EndlessMapManager.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

URoadNetworkComponent::URoadNetworkComponent()
{
	// 移除实时 Tick 行为，完全转为事件/区块生成驱动
	PrimaryComponentTick.bCanEverTick = false;

	// 默认加载引擎的基础 Cube 作为道路视觉体
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeVisualAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeVisualAsset.Succeeded())
	{
		RoadMesh = CubeVisualAsset.Object;
	}
}

void URoadNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Seed == -1)
	{
		Seed = FMath::RandRange(0, 999999);
	}
}

uint32 URoadNetworkComponent::GetGridHash(int32 X, int32 Y) const
{
	uint32 a = (uint32)(X ^ Seed);
	uint32 b = (uint32)(Y ^ (Seed >> 16));
	a = (a ^ 61) ^ (b >> 16);
	a = a + (b << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}

uint32 URoadNetworkComponent::GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const
{
	if (X1 > X2 || (X1 == X2 && Y1 > Y2))
	{
		Swap(X1, X2);
		Swap(Y1, Y2);
	}
	return GetGridHash(X1 + Y1 * 1000, X2 + Y2 * 1000);
}

bool URoadNetworkComponent::DoesNodeExistAtGrid(int32 X, int32 Y) const
{
	return (X % 3 == 0) || (Y % 3 == 0);
}

bool URoadNetworkComponent::IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const
{
	if (!DoesNodeExistAtGrid(X1, Y1) || !DoesNodeExistAtGrid(X2, Y2))
	{
		return false;
	}
	if (FMath::Abs(X1 - X2) + FMath::Abs(Y1 - Y2) != 1)
	{
		return false;
	}
	uint32 EdgeHash = GetSegmentHash(X1, Y1, X2, Y2);
	return (EdgeHash % 100) >= (uint32)BlockChance;
}

ERoadType URoadNetworkComponent::GetInitialRoadTypeAt(int32 X, int32 Y) const
{
	if (!DoesNodeExistAtGrid(X, Y))
	{
		return ERoadType::Parking;
	}

	TArray<FIntPoint> Connected;
	FIntPoint Directions[4] = { FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) };

	for (const FIntPoint& Dir : Directions)
	{
		if (IsPathConnected(X, Y, X + Dir.X, Y + Dir.Y))
		{
			Connected.Add(FIntPoint(X + Dir.X, Y + Dir.Y));
		}
	}

	int32 ConnectionCount = Connected.Num();
	uint32 NodeHash = GetGridHash(X, Y);

	if (ConnectionCount == 4)
	{
		return ((NodeHash % 100) < (uint32)(CrossChance * 100.f)) ? ERoadType::Cross : ERoadType::Parking;
	}
	else if (ConnectionCount == 3)
	{
		return ((NodeHash % 100) < (uint32)(TRoadChance * 100.f)) ? ERoadType::TRoad : ERoadType::Parking;
	}
	else if (ConnectionCount == 2)
	{
		FIntPoint C1 = Connected[0];
		FIntPoint C2 = Connected[1];
		return (C1.X == C2.X || C1.Y == C2.Y) ? ERoadType::Straight : ERoadType::Turn;
	}
	else if (ConnectionCount == 1)
	{
		return ((NodeHash % 100) < (uint32)(ParkingChance * 100.f)) ? ERoadType::Parking : ERoadType::End;
	}

	return ERoadType::Parking;
}

TArray<FBox2D> URoadNetworkComponent::GenerateRoadNetworkOnChunk(AActor* ChunkActor, const FBox2D& ChunkBounds)
{
	TArray<FBox2D> RoadOccupiedBoxes;
	if (!ChunkActor) return RoadOccupiedBoxes;

	// 根据当前区块的2D包围盒，计算出可能涵盖的路网单元范围
	int32 MinGridX = FMath::FloorToInt(ChunkBounds.Min.X / CellSize);
	int32 MaxGridX = FMath::CeilToInt(ChunkBounds.Max.X / CellSize);
	int32 MinGridY = FMath::FloorToInt(ChunkBounds.Min.Y / CellSize);
	int32 MaxGridY = FMath::CeilToInt(ChunkBounds.Max.Y / CellSize);

	// 稍微抬高路段，防止和地面产生 Z-Fighting（闪烁）
	float RoadZHeight = ChunkActor->GetActorLocation().Z + 5.f;

	for (int32 x = MinGridX; x <= MaxGridX; ++x)
	{
		for (int32 y = MinGridY; y <= MaxGridY; ++y)
		{
			if (!DoesNodeExistAtGrid(x, y)) continue;

			FVector2D NodeWorldPos(x * CellSize, y * CellSize);

			// 1. 生成路口节点（如果路口坐标完全落在当前 Chunk 范围内）
			if (ChunkBounds.IsInside(NodeWorldPos))
			{
				// 检查该节点是否是有效通路（非孤立点）
				bool bHasConnections = false;
				FIntPoint Directions[4] = { FIntPoint(1,0), FIntPoint(-1,0), FIntPoint(0,1), FIntPoint(0,-1) };
				for (const auto& Dir : Directions)
				{
					if (IsPathConnected(x, y, x + Dir.X, y + Dir.Y))
					{
						bHasConnections = true;
						break;
					}
				}

				if (bHasConnections)
				{
					// 创建路口 2D 避让范围
					FBox2D NodeBox(NodeWorldPos - FVector2D(RoadWidth / 2.f, RoadWidth / 2.f), NodeWorldPos + FVector2D(RoadWidth / 2.f, RoadWidth / 2.f));
					RoadOccupiedBoxes.Add(NodeBox);

					// 视觉：在 ChunkActor 下创建路口网格
					if (RoadMesh)
					{
						UStaticMeshComponent* RoadComp = NewObject<UStaticMeshComponent>(ChunkActor);
						RoadComp->SetStaticMesh(RoadMesh);
						if (RoadMaterial) RoadComp->SetMaterial(0, RoadMaterial);
						RoadComp->SetupAttachment(ChunkActor->GetRootComponent());
						RoadComp->SetWorldLocation(FVector(NodeWorldPos.X, NodeWorldPos.Y, RoadZHeight));

						// Cube 默认大小 100x100x100，将其缩放到 (RoadWidth, RoadWidth, 10.f)
						RoadComp->SetWorldScale3D(FVector(RoadWidth / 100.f, RoadWidth / 100.f, 0.1f));
						RoadComp->SetCanEverAffectNavigation(true);
						RoadComp->RegisterComponent();
					}
				}
			}

			// 2. 生成连通道路（跨区块去重核心：利用“路段中点判定法”）
			FIntPoint DirectionTargets[4] = {
				FIntPoint(x + 1, y),
				FIntPoint(x - 1, y),
				FIntPoint(x, y + 1),
				FIntPoint(x, y - 1)
			};

			for (const FIntPoint& Target : DirectionTargets)
			{
				if (IsPathConnected(x, y, Target.X, Target.Y))
				{
					FVector2D TargetWorldPos(Target.X * CellSize, Target.Y * CellSize);
					FVector2D RoadMidpoint = (NodeWorldPos + TargetWorldPos) / 2.f;

					// 只有在路段中点落在当前区块范围内时，该区块才负责生成并记录这条路段
					if (ChunkBounds.IsInside(RoadMidpoint))
					{
						// 双向去重：确保 (A->B) 和 (B->A) 只生成一次
						if (Target.X > x || (Target.X == x && Target.Y > y))
						{
							float MinX = FMath::Min(NodeWorldPos.X, TargetWorldPos.X);
							float MaxX = FMath::Max(NodeWorldPos.X, TargetWorldPos.X);
							float MinY = FMath::Min(NodeWorldPos.Y, TargetWorldPos.Y);
							float MaxY = FMath::Max(NodeWorldPos.Y, TargetWorldPos.Y);

							// 赋予道路物理宽度
							if (MinX == MaxX) // 垂直道路
							{
								MinX -= RoadWidth / 2.f;
								MaxX += RoadWidth / 2.f;
							}
							else if (MinY == MaxY) // 水平道路
							{
								MinY -= RoadWidth / 2.f;
								MaxY += RoadWidth / 2.f;
							}

							// 记录道路占用盒子，用于避开建筑
							FBox2D LinkBox(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));
							RoadOccupiedBoxes.Add(LinkBox);

							// 视觉：生成连接道路段
							if (RoadMesh)
							{
								UStaticMeshComponent* LinkComp = NewObject<UStaticMeshComponent>(ChunkActor);
								LinkComp->SetStaticMesh(RoadMesh);
								if (RoadMaterial) LinkComp->SetMaterial(0, RoadMaterial);
								LinkComp->SetupAttachment(ChunkActor->GetRootComponent());

								FVector LinkCenter((MinX + MaxX) / 2.f, (MinY + MaxY) / 2.f, RoadZHeight);
								LinkComp->SetWorldLocation(LinkCenter);

								float ScaleX = (MaxX - MinX) / 100.f;
								float ScaleY = (MaxY - MinY) / 100.f;
								LinkComp->SetWorldScale3D(FVector(ScaleX, ScaleY, 0.1f));
								LinkComp->SetCanEverAffectNavigation(true);
								LinkComp->RegisterComponent();
							}
						}
					}
				}
			}
		}
	}

	return RoadOccupiedBoxes;
}
