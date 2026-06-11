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
    // 地块生成配置 (Ground)
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
    // 全局限制配置 (Global Limits) - 【新增】
    // ==========================================

    // 玩家初始位置(0,0)的无干扰安全半径(单位:米)。该范围内仅生成地面，绝对不生成任何建筑、道具或敌人
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Global Limits")
    float InitialSafeZoneRadius_Meters;

    // ==========================================
    // 建筑生成配置 (Architectures)
    // ==========================================

    // 策划配置的建筑模型文件夹路径 (例如: /Game/Environment/Buildings)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures", meta = (RelativeToGameContentDir))
    FDirectoryPath ArchitecturePath;

    // 建筑之间的间隔距离 (X = 最小值, Y = 最大值) 单位: 米
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
    FVector2D BuildingSpacing_Meters;

    // 交界处禁止生成建筑的边缘距离 (X = 最小值, Y = 最大值) 单位: 米
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
    FVector2D EdgeMargin_Meters;

    // 每个区块最大尝试生成建筑的次数（防止策划把间隔设得太小导致死循环卡顿）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Architectures")
    int32 MaxBuildingsPerChunk;

    // ==========================================
    // 系统设置
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Map Generation|Settings")
    float CheckInterval;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Map Generation|Navigation")
    UNavigationInvokerComponent* NavInvokerComp;

private:
    UPROPERTY()
    TMap<FIntPoint, AActor*> ActiveChunks;

    // 缓存加载完毕的建筑模型
    UPROPERTY()
    TArray<UStaticMesh*> LoadedArchitectures;

    FIntPoint CurrentPlayerGrid;
    FIntPoint LastMoveDir;
    FTimerHandle MapUpdateTimer;

    UFUNCTION()
    void UpdateMap();

    void LoadArchitectures();
    void SpawnTileAtGrid(const FIntPoint& GridLocation);

    // 在当前生成的区块上利用包围盒防穿模算法生成建筑
    void SpawnArchitecturesOnChunk(AActor* ChunkActor);

    // 【新增】：通用检测函数 - 判断某个包围盒是否侵入了初始安全区
    bool IsInInitialSafeZone(const FBox2D& Bounds2D) const;

    bool DestroyFarthestTile(const TArray<FIntPoint>& ProtectedGrids);

    FIntPoint WorldToGrid(const FVector& WorldLocation) const;
    FVector GridToWorld(const FIntPoint& GridLocation) const;
};