#include "RoadNetworkComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

URoadNetworkComponent::URoadNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // Enable Tick to allow real-time tracking and drawing
}

void URoadNetworkComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URoadNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World) return;

	// Automatically track the local player's position
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
		UpdateAndDrawRoadNetwork(PlayerLocation);
	}
}

// Classic deterministic Jenkins hash variant. Inputs coordinates, returns a fixed pseudo-random number.
uint32 URoadNetworkComponent::GetGridHash(int32 X, int32 Y) const
{
	uint32 a = (uint32)X;
	uint32 b = (uint32)Y;
	a = (a ^ 61) ^ (b >> 16);
	a = a + (b << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}

// Ensures the calculated hash is identical for bidirectional road segments (A->B and B->A)
uint32 URoadNetworkComponent::GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const
{
	if (X1 > X2 || (X1 == X2 && Y1 > Y2))
	{
		Swap(X1, X2);
		Swap(Y1, Y2);
	}
	// Combine both coordinates into the hash calculation
	return GetGridHash(X1 + Y1 * 1000, X2 + Y2 * 1000);
}

bool URoadNetworkComponent::DoesNodeExistAtGrid(int32 X, int32 Y) const
{
	// Basic rule: Lay out an infinite road network of 3x3 block grids (i.e., a main road every 3 cells)
	return (X % 3 == 0) || (Y % 3 == 0);
}

bool URoadNetworkComponent::IsPathConnected(int32 X1, int32 Y1, int32 X2, int32 Y2) const
{
	// Both end nodes must exist
	if (!DoesNodeExistAtGrid(X1, Y1) || !DoesNodeExistAtGrid(X2, Y2))
	{
		return false;
	}

	// Must be adjacent grid cells
	if (FMath::Abs(X1 - X2) + FMath::Abs(Y1 - Y2) != 1)
	{
		return false;
	}

	// Use segment hash for deterministic blocking, creating random dead ends and branches in the road network
	uint32 EdgeHash = GetSegmentHash(X1, Y1, X2, Y2);
	return (EdgeHash % 100) >= (uint32)BlockChance;
}

// Deterministically resolves the base road type of any grid coordinate.
ERoadType URoadNetworkComponent::GetInitialRoadTypeAt(int32 X, int32 Y) const
{
	if (!DoesNodeExistAtGrid(X, Y))
	{
		return ERoadType::Parking;
	}

	TArray<FIntPoint> Connected;
	FIntPoint Directions[4] = {
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1)
	};

	for (const FIntPoint& Dir : Directions)
	{
		if (IsPathConnected(X, Y, X + Dir.X, Y + Dir.Y))
		{
			Connected.Add(FIntPoint(X + Dir.X, Y + Dir.Y));
		}
	}

	int32 ConnectionCount = Connected.Num();
	if (ConnectionCount == 4)
	{
		return ERoadType::Cross;
	}
	else if (ConnectionCount == 3)
	{
		return ERoadType::TRoad;
	}
	else if (ConnectionCount == 2)
	{
		FIntPoint C1 = Connected[0];
		FIntPoint C2 = Connected[1];
		if (C1.X == C2.X || C1.Y == C2.Y)
		{
			return ERoadType::Straight;
		}
		else
		{
			return ERoadType::Turn;
		}
	}
	else if (ConnectionCount == 1)
	{
		uint32 NodeHash = GetGridHash(X, Y);
		if (NodeHash % 100 < 35)
		{
			return ERoadType::Parking;
		}
		else
		{
			return ERoadType::End;
		}
	}

	return ERoadType::Parking;
}

void URoadNetworkComponent::UpdateAndDrawRoadNetwork(const FVector& PlayerLocation)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 1. Calculate the player's current grid coordinates
	int32 PlayerGridX = FMath::RoundToInt(PlayerLocation.X / CellSize);
	int32 PlayerGridY = FMath::RoundToInt(PlayerLocation.Y / CellSize);

	// Fix the road network height slightly below the player's feet to prevent floating
	float GridHeight = PlayerLocation.Z - 90.f;

	TMap<FIntPoint, FProceduralRoadNode> ActiveNodes;

	// 2. Generate local nodes within the grid radius
	for (int32 x = PlayerGridX - GridRadius; x <= PlayerGridX + GridRadius; ++x)
	{
		for (int32 y = PlayerGridY - GridRadius; y <= PlayerGridY + GridRadius; ++y)
		{
			if (DoesNodeExistAtGrid(x, y))
			{
				FProceduralRoadNode Node;
				Node.GridCoords = FIntPoint(x, y);
				Node.WorldPosition = FVector(x * CellSize, y * CellSize, GridHeight);

				// Detect connections in four cardinal directions
				FIntPoint Directions[4] = {
					FIntPoint(1, 0),  // Right
					FIntPoint(-1, 0), // Left
					FIntPoint(0, 1),  // Up
					FIntPoint(0, -1)  // Down
				};

				for (const FIntPoint& Dir : Directions)
				{
					if (IsPathConnected(x, y, x + Dir.X, y + Dir.Y))
					{
						Node.ConnectedCoords.Add(FIntPoint(x + Dir.X, y + Dir.Y));
					}
				}

				// Deduce initial road type
				Node.RoadType = GetInitialRoadTypeAt(x, y);

				ActiveNodes.Add(FIntPoint(x, y), Node);
			}
		}
	}

	// 3. Post-Process: Convert Straight to StraightToNode
	// Logic: When both connected neighbors of a 'Straight' node are either 'TRoad', 'Cross', or 'Straight'
	for (auto& Elem : ActiveNodes)
	{
		FProceduralRoadNode& Node = Elem.Value;
		if (Node.RoadType == ERoadType::Straight)
		{
			// A Straight node always has exactly 2 connections
			if (Node.ConnectedCoords.Num() == 2)
			{
				FIntPoint NeighborA = Node.ConnectedCoords[0];
				FIntPoint NeighborB = Node.ConnectedCoords[1];

				// Safely resolve neighbor base road types (independent of active grid bounds)
				ERoadType TypeA = GetInitialRoadTypeAt(NeighborA.X, NeighborA.Y);
				ERoadType TypeB = GetInitialRoadTypeAt(NeighborB.X, NeighborB.Y);

				auto IsValidTarget = [](ERoadType Type) {
					return Type == ERoadType::TRoad || Type == ERoadType::Cross || Type == ERoadType::Straight;
					};

				if (IsValidTarget(TypeA) && IsValidTarget(TypeB))
				{
					Node.RoadType = ERoadType::StraightToNode;
				}
			}
		}
	}

	// 4. Real-time debug drawing
	const float LifeTime = 0.f;

	for (const auto& Elem : ActiveNodes)
	{
		const FProceduralRoadNode& Node = Elem.Value;
		FColor NodeColor = GetColorForRoadType(Node.RoadType);

		// Draw node sphere
		DrawDebugSphere(
			World,
			Node.WorldPosition,
			NodeSphereRadius,
			8,
			NodeColor,
			false,
			LifeTime,
			0,
			1.5f
		);

		// Draw road network label text (slightly offset upward to prevent overlapping)
		FVector TextPos = Node.WorldPosition + FVector(0.f, 0.f, NodeSphereRadius + 20.f);
		FString DebugStr = FString::Printf(TEXT("(%d,%d) %s"), Node.GridCoords.X, Node.GridCoords.Y, *GetTextForRoadType(Node.RoadType));
		DrawDebugString(
			World,
			TextPos,
			DebugStr,
			nullptr,
			FColor::White,
			LifeTime,
			true,
			1.1f
		);

		// Draw road connections (draw unidirectional to avoid duplicate lines)
		for (const FIntPoint& TargetCoords : Node.ConnectedCoords)
		{
			if (TargetCoords.X > Node.GridCoords.X || (TargetCoords.X == Node.GridCoords.X && TargetCoords.Y > Node.GridCoords.Y))
			{
				FVector TargetWorldPos = FVector(TargetCoords.X * CellSize, TargetCoords.Y * CellSize, GridHeight);
				DrawDebugLine(
					World,
					Node.WorldPosition,
					TargetWorldPos,
					FColor::Cyan,
					false,
					LifeTime,
					0,
					5.f // Line thickness
				);
			}
		}
	}
}

FColor URoadNetworkComponent::GetColorForRoadType(ERoadType Type) const
{
	switch (Type)
	{
	case ERoadType::Straight: return FColor::Green; // Green
	case ERoadType::StraightToNode: return FColor::Cyan; // Cyan (New category color)
	case ERoadType::Turn: return FColor::Yellow; // Yellow
	case ERoadType::Cross: return FColor::Red; // Red
	case ERoadType::TRoad: return FColor::Orange; // Orange
	case ERoadType::End: return FColor::Blue; // Blue
	case ERoadType::Parking: return FColor::Magenta; // Magenta (Parking)
	default: return FColor::White;
	}
}

FString URoadNetworkComponent::GetTextForRoadType(ERoadType Type) const
{
	switch (Type)
	{
	case ERoadType::Straight: return TEXT("Straight");
	case ERoadType::StraightToNode: return TEXT("StraightToNode");
	case ERoadType::Turn: return TEXT("Turn");
	case ERoadType::Cross: return TEXT("Cross");
	case ERoadType::TRoad: return TEXT("TRoad");
	case ERoadType::End: return TEXT("End");
	case ERoadType::Parking: return TEXT("Parking");
	default: return TEXT("Unknown");
	}
}