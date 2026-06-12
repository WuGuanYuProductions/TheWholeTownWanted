#include "RoadNetworkComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

URoadNetworkComponent::URoadNetworkComponent()
{
	// Enable Tick to support real-time position tracking and road network debug drawing
	PrimaryComponentTick.bCanEverTick = true;
}

void URoadNetworkComponent::BeginPlay()
{
	Super::BeginPlay();

	// If seed is set to -1, generate a completely random seed using current system time cycles
	if (Seed == -1)
	{
		Seed = FMath::RandRange(0, 999999);
	}
}

void URoadNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	if (!World) return;

	// Automatically get the local player coordinates and generate/draw the road network in real-time centered on them
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC && PC->GetPawn())
	{
		FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
		UpdateAndDrawRoadNetwork(PlayerLocation);
	}
}

// Classic deterministic Jenkins hash variant, now incorporating the generation seed
uint32 URoadNetworkComponent::GetGridHash(int32 X, int32 Y) const
{
	// Scramble inputs using the seed to ensure different layouts per seed
	uint32 a = (uint32)(X ^ Seed);
	uint32 b = (uint32)(Y ^ (Seed >> 16));

	a = (a ^ 61) ^ (b >> 16);
	a = a + (b << 3);
	a = a ^ (a >> 4);
	a = a * 0x27d4eb2d;
	a = a ^ (a >> 15);
	return a;
}

// Ensure that the calculated hash value for bidirectional road segments (A->B and B->A) is identical
uint32 URoadNetworkComponent::GetSegmentHash(int32 X1, int32 Y1, int32 X2, int32 Y2) const
{
	if (X1 > X2 || (X1 == X2 && Y1 > Y2))
	{
		Swap(X1, X2);
		Swap(Y1, Y2);
	}
	// Combine the two coordinates for hash calculation
	return GetGridHash(X1 + Y1 * 1000, X2 + Y2 * 1000);
}

bool URoadNetworkComponent::DoesNodeExistAtGrid(int32 X, int32 Y) const
{
	// Base rule: Infinite road tiling in a 3x3 block grid layout (i.e., a main road every 3 grid units)
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

	// Use segment hash to achieve deterministic blocking, creating random dead ends and branches
	uint32 EdgeHash = GetSegmentHash(X1, Y1, X2, Y2);
	return (EdgeHash % 100) >= (uint32)BlockChance;
}

// Core function: Input any coordinates, return the base road type through deterministic logic
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
	uint32 NodeHash = GetGridHash(X, Y); // Get the seed-based deterministic hash of the current coordinates

	if (ConnectionCount == 4)
	{
		// Determine Cross probability: hash range 0-99. If less than the configured percentage, keep it; otherwise, degrade to Parking
		if ((NodeHash % 100) < (uint32)(CrossChance * 100.f))
		{
			return ERoadType::Cross;
		}
		else
		{
			return ERoadType::Parking;
		}
	}
	else if (ConnectionCount == 3)
	{
		// Determine TRoad probability: if it fails the probability check, degrade to Parking
		if ((NodeHash % 100) < (uint32)(TRoadChance * 100.f))
		{
			return ERoadType::TRoad;
		}
		else
		{
			return ERoadType::Parking;
		}
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
		// Use the configured ParkingChance to decide whether a dead end becomes a Parking or remains an End
		if ((NodeHash % 100) < (uint32)(ParkingChance * 100.f))
		{
			return ERoadType::Parking;
		}
		else
		{
			return ERoadType::End;
		}
	}

	// If ConnectionCount == 0 (no physical connections at all), it does not belong to any road.
	// The external UpdateAndDrawRoadNetwork will filter and discard it directly. Default to Parking here.
	return ERoadType::Parking;
}

void URoadNetworkComponent::UpdateAndDrawRoadNetwork(const FVector& PlayerLocation)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 1. Calculate the grid cell where the player is currently located
	int32 PlayerGridX = FMath::RoundToInt(PlayerLocation.X / CellSize);
	int32 PlayerGridY = FMath::RoundToInt(PlayerLocation.Y / CellSize);

	// Adjust the grid height slightly below the player's feet to prevent floating
	float GridHeight = PlayerLocation.Z - 90.f;

	TMap<FIntPoint, FProceduralRoadNode> ActiveNodes;

	// 2. Generate local active nodes within the GridRadius
	for (int32 x = PlayerGridX - GridRadius; x <= PlayerGridX + GridRadius; ++x)
	{
		for (int32 y = PlayerGridY - GridRadius; y <= PlayerGridY + GridRadius; ++y)
		{
			if (DoesNodeExistAtGrid(x, y))
			{
				FProceduralRoadNode Node;
				Node.GridCoords = FIntPoint(x, y);
				Node.WorldPosition = FVector(x * CellSize, y * CellSize, GridHeight);

				FIntPoint Directions[4] = {
					FIntPoint(1, 0), // Right
					FIntPoint(-1, 0), // Left
					FIntPoint(0, 1), // Up
					FIntPoint(0, -1) // Down
				};

				for (const FIntPoint& Dir : Directions)
				{
					if (IsPathConnected(x, y, x + Dir.X, y + Dir.Y))
					{
						Node.ConnectedCoords.Add(FIntPoint(x + Dir.X, y + Dir.Y));
					}
				}

				// [Core Bug Fix]: If all directions of a node are blocked (physical connection count is 0),
				// then this node should not be generated as a road network node; skip it directly
				if (Node.ConnectedCoords.Num() == 0)
				{
					continue;
				}

				// Calculate the base road type
				Node.RoadType = GetInitialRoadTypeAt(x, y);

				ActiveNodes.Add(FIntPoint(x, y), Node);
			}
		}
	}

	// 3. Post-processing: Convert eligible Straight roads to StraightToNode
	// Logic: When both neighbors connected to a straight road are also T-Road, Cross, or Straight, upgrade its specification to StraightToNode
	for (auto& Elem : ActiveNodes)
	{
		FProceduralRoadNode& Node = Elem.Value;
		if (Node.RoadType == ERoadType::Straight)
		{
			if (Node.ConnectedCoords.Num() == 2)
			{
				FIntPoint NeighborA = Node.ConnectedCoords[0];
				FIntPoint NeighborB = Node.ConnectedCoords[1];

				// Get the base road type of the neighbor (safely resolved without boundary dependencies)
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

	// 4. Real-time Debug Drawing
	const float LifeTime = 0.f;

	for (const auto& Elem : ActiveNodes)
	{
		const FProceduralRoadNode& Node = Elem.Value;
		FColor NodeColor = GetColorForRoadType(Node.RoadType);

		// Draw node debug sphere
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

		// Draw road network label text (slightly offset vertically to prevent overlapping with the sphere)
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

		// Draw lines (using a unidirectional drawing method to prevent drawing the line twice between two nodes)
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
	case ERoadType::Straight: return FColor::Green;
	case ERoadType::StraightToNode: return FColor::Cyan;
	case ERoadType::Turn: return FColor::Yellow;
	case ERoadType::Cross: return FColor::Red;
	case ERoadType::TRoad: return FColor::Orange;
	case ERoadType::End: return FColor::Blue;
	case ERoadType::Parking: return FColor::Magenta;
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
