// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "Editor/Experimental/EditorInteractiveToolsFramework/Public/Behaviors/2DViewportBehaviorTargets.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Interfaces/IHttpRequest.h"
#include "Hunyuan3DActor.generated.h"

UCLASS()
class HUNYUAN3DDEMO_API AHunyuan3DActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AHunyuan3DActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	// 提交文本生成3D任务
	UFUNCTION(BlueprintCallable)
	void SubmitTextTo3DTask(const FString& Prompt);
	
	UFUNCTION(BlueprintCallable)
	void DiagnoseConnection();
    
private:
	const FString API_KEY = TEXT("lbGU59kOGNVZLYExHgnhzXxprkumRXbt");
	const FString SAVE_PATH = TEXT("H:/UGCAssetEditorTMR/3DAsset/");

	FTimerHandle TimerHandle;
	FString TaskID;
	int32 TotalDownloads; // 总共需要下载的资源数量
	int32 CompletedDownloads; // 已完成的下载数量
	FDateTime StartTime; // 记录任务开始时间
	
	UFUNCTION()
	void CheckInternetConnection();
	UFUNCTION()
	void CheckDNSResolution();
	UFUNCTION()
	void CheckServerStatus();
	void HandleSubmissionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	UFUNCTION()
	void QueryTaskResult();
	void DownloadAllAssets(const TSharedPtr<FJsonObject>& Data);
	void HandleResultQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	UFUNCTION()
	void DownloadAsset(const FString& URL, const FString& FileName);
	void HandleAssetDownload(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString FilePath);
	void OnSingleAssetDownloaded(bool bSuccess);
	void OnAllDownloadsCompleted();
};
