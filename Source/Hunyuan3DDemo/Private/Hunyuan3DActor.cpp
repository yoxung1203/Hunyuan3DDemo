// Fill out your copyright notice in the Description page of Project Settings.


#include "Hunyuan3DActor.h"
#include "HttpModule.h"
#include "SocketSubsystem.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"

// Sets default values
AHunyuan3DActor::AHunyuan3DActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AHunyuan3DActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AHunyuan3DActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AHunyuan3DActor::DiagnoseConnection()
{
    UE_LOG(LogTemp, Display, TEXT("===== 开始网络诊断 ====="));
    
    // 1. 检查互联网连接
    CheckInternetConnection();
    
    // 2. 解析DNS
    CheckDNSResolution();
    
    // 3. 检查API服务器状态
    CheckServerStatus();
    
    // 4. 测试简单API请求
    UE_LOG(LogTemp, Display, TEXT("发送测试请求..."));
    SubmitTextTo3DTask(TEXT("连接测试"));
}

void AHunyuan3DActor::CheckInternetConnection()
{
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    
    Request->SetURL(TEXT("http://www.qq.com"));
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(10);
    
    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) {
        if (bSuccess && Response.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("互联网连接正常 [状态码: %d]"), Response->GetResponseCode());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法访问互联网"));
        }
    });
    Request->ProcessRequest();
}

void AHunyuan3DActor::CheckDNSResolution()
{
    FString Host = TEXT("hunyuanapi.woa.com");
    UE_LOG(LogTemp, Display, TEXT("解析域名: %s"), *Host);
    
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    FAddressInfoResult Result = SocketSubsystem->GetAddressInfo(
        *Host, nullptr, EAddressInfoFlags::Default, NAME_None);
    
    if (Result.Results.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("域名解析成功:"));
        for (const auto& AddrInfo : Result.Results)
        {
            UE_LOG(LogTemp, Display, TEXT("  IP: %s"), *AddrInfo.Address->ToString(true));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("域名解析失败"));
    }
}

void AHunyuan3DActor::CheckServerStatus()
{
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    
    Request->SetURL(TEXT("http://hunyuanapi.woa.com"));
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(15);
    
    Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) {
        if (bSuccess && Response.IsValid())
        {
            UE_LOG(LogTemp, Display, TEXT("API服务器可达 [状态码: %d]"), Response->GetResponseCode());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("无法连接到API服务器"));
        }
    });
    Request->ProcessRequest();
}

void AHunyuan3DActor::SubmitTextTo3DTask(const FString& Prompt)
{
    // 记录开始时间
    StartTime = FDateTime::Now();
    
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    
    // 设置API请求
    Request->SetURL(TEXT("http://hunyuanapi.woa.com/openapi/v1/3d/generations/submission"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *API_KEY));
    
    // 构建JSON请求体
    TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
    RequestObj->SetStringField(TEXT("model"), TEXT("hunyuan-3d-dit-v2.5"));
    RequestObj->SetStringField(TEXT("prompt"), Prompt);
    RequestObj->SetBoolField(TEXT("enable_pbr"), true);
    
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
    
    Request->SetContentAsString(RequestBody);
    Request->OnProcessRequestComplete().BindUObject(this, &AHunyuan3DActor::HandleSubmissionResponse);
    Request->ProcessRequest();
}

void AHunyuan3DActor::HandleSubmissionResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
    FString RequestURL = Request->GetURL();
    static TMap<FString, int32> RetryCountMap; // 请求URL到重试次数的映射
    
    UE_LOG(LogTemp, Warning, TEXT("===== 请求响应 [%s] ====="), *RequestURL);
    
    // 记录详细响应信息
    /*if (Response.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("响应状态码: %d"), Response->GetResponseCode());
        UE_LOG(LogTemp, Warning, TEXT("响应头:"));
        for (const auto& Header : Response->GetAllHeaders())
        {
            UE_LOG(LogTemp, Warning, TEXT("  %s"), *Header);
        }
        
        FString Content = Response->GetContentAsString();
        UE_LOG(LogTemp, Warning, TEXT("响应体: %s"), *Content.Left(500));
    }*/
    
    // 处理502错误
    if (Response.IsValid() && Response->GetResponseCode() == 502)
    {
        int32& RetryCount = RetryCountMap.FindOrAdd(RequestURL, 0);
        
        if (RetryCount < 3)
        {
            RetryCount++;
            UE_LOG(LogTemp, Warning, TEXT("502错误 - 10秒后重试 (%d/3)"), RetryCount);
            
            // 延迟重试
            FTimerHandle RetryTimer;
            if (GWorld)
            {
                GWorld->GetTimerManager().SetTimer(RetryTimer, [Request]() {
                    UE_LOG(LogTemp, Warning, TEXT("执行重试请求..."));
                    Request->ProcessRequest();
                }, 10.0f, false);
            }
            return;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("502错误 - 已达到最大重试次数"));
            RetryCountMap.Remove(RequestURL);
        }
    }
    
    if (bSuccess && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
        
        if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
        {
            TaskID = JsonObject->GetStringField(TEXT("task_id"));
            QueryTaskResult();
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("提交任务失败: %s"), *Response->GetContentAsString());
    }
}

void AHunyuan3DActor::QueryTaskResult()
{
    UE_LOG(LogTemp, Warning, TEXT("QueryTaskResult"));
    UE_LOG(LogTemp, Warning, TEXT("Query TaskID：%s"), *TaskID);
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    
    Request->SetURL(TEXT("http://hunyuanapi.woa.com/openapi/v1/3d/generations/task"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *API_KEY));
    
    TSharedPtr<FJsonObject> RequestObj = MakeShareable(new FJsonObject);
    RequestObj->SetStringField(TEXT("task_id"), TaskID);
    
    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestObj.ToSharedRef(), Writer);
    
    Request->SetContentAsString(RequestBody);
    Request->OnProcessRequestComplete().BindUObject(this, &AHunyuan3DActor::HandleResultQueryResponse);
    Request->ProcessRequest();
}

void AHunyuan3DActor::DownloadAllAssets(const TSharedPtr<FJsonObject>& Data)
{
    // 获取所有格式的URL
    FString GLBUrl, OBJUrl, GIFUrl;
    Data->TryGetStringField(TEXT("glb_url"), GLBUrl);
    Data->TryGetStringField(TEXT("obj_url"), OBJUrl);
    Data->TryGetStringField(TEXT("gif_url"), GIFUrl);
    
    // 生成基础文件名（使用任务ID或时间戳）
    FString BaseName = FString::Printf(TEXT("asset_%s"), *FGuid::NewGuid().ToString());
    
    // 初始化下载计数器
    TotalDownloads = 0;
    CompletedDownloads = 0;
    
    // 准备下载列表
    TArray<TPair<FString, FString>> DownloadList;
    
    // 确定哪些格式可用并添加到下载列表
    if (!GLBUrl.IsEmpty())
    {
        DownloadList.Add(TPair<FString, FString>(GLBUrl, BaseName + TEXT(".glb")));
        TotalDownloads++;
    }
    
    if (!OBJUrl.IsEmpty())
    {
        DownloadList.Add(TPair<FString, FString>(OBJUrl, BaseName + TEXT(".obj")));
        TotalDownloads++;
    }
    
    if (!GIFUrl.IsEmpty())
    {
        DownloadList.Add(TPair<FString, FString>(GIFUrl, BaseName + TEXT(".gif")));
        TotalDownloads++;
    }
    
    UE_LOG(LogTemp, Display, TEXT("开始下载 %d 个资源"), TotalDownloads);
    
    // 开始下载所有资源
    for (const auto& DownloadItem : DownloadList)
    {
        DownloadAsset(DownloadItem.Key, DownloadItem.Value);
    }
}

void AHunyuan3DActor::HandleResultQueryResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
    // 检查响应是否有效
    if (!bSuccess || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("查询任务失败: 网络错误或无效响应"));
        return;
    }

    // 记录详细响应信息（调试用）
    UE_LOG(LogTemp, Display, TEXT("===== 任务查询响应 ====="));
    UE_LOG(LogTemp, Display, TEXT("状态码: %d"), Response->GetResponseCode());
    UE_LOG(LogTemp, Display, TEXT("响应体: %s"), *Response->GetContentAsString().Left(512));
    
    // 处理HTTP错误
    /*if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        UE_LOG(LogTemp, Error, TEXT("查询任务失败: HTTP %d"), Response->GetResponseCode());
        
        // 429错误处理（速率限制）
        if (Response->GetResponseCode() == 429)
        {
            UE_LOG(LogTemp, Warning, TEXT("API请求过频，30秒后重试"));
            FString TaskID = Request->GetContentAsString(); // 假设请求体是task_id
            
            FTimerHandle TimerHandle;
            if (GWorld)
            {
                GWorld->GetTimerManager().SetTimer(TimerHandle, [TaskID]() {
                    QueryTaskResult(TaskID);
                }, 30.0f, false);
            }
        }
        return;
    }*/

    // 解析JSON响应
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("响应JSON解析失败"));
        return;
    }

    // 获取任务状态
    FString Status;
    if (!JsonObject->TryGetStringField(TEXT("status"), Status))
    {
        UE_LOG(LogTemp, Error, TEXT("响应中缺少status字段"));
        return;
    }

    FString ID;
    JsonObject->TryGetStringField(TEXT("id"), ID); // 可选字段

    // 处理不同状态
    if (Status.Equals(TEXT("succeeded"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Display, TEXT("任务完成: %s"), *ID);
        
        // 处理任务结果
        const TArray<TSharedPtr<FJsonValue>>* DataArray;
        if (JsonObject->TryGetArrayField(TEXT("data"), DataArray) && DataArray && DataArray->Num() > 0)
        {
            /*TSharedPtr<FJsonObject> Data = (*DataArray)[0]->AsObject();
            
            // 下载GLB文件
            FString GLBUrl;
            if (Data->TryGetStringField(TEXT("glb_url"), GLBUrl) && !GLBUrl.IsEmpty())
            {
                // 生成唯一文件名
                FString FileName = FString::Printf(TEXT("asset_%s.glb"), *FGuid::NewGuid().ToString());
                DownloadAsset(GLBUrl, FileName);
            }*/

            if (DataArray->Num() > 0)
            {
                TSharedPtr<FJsonObject> Data = (*DataArray)[0]->AsObject();
                
                DownloadAllAssets(Data);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("任务成功但缺少data字段"));
        }
    }
    else if (Status.Equals(TEXT("queued"), ESearchCase::IgnoreCase) || 
             Status.Equals(TEXT("running"), ESearchCase::IgnoreCase))
    {
        // 任务仍在处理中，稍后重试
        UE_LOG(LogTemp, Display, TEXT("任务处理中[%s]: %s"), *Status, *ID);
        
        // 计算重试延迟（智能延迟）
        static TMap<FString, int> RetryCountMap;
        int& RetryCount = RetryCountMap.FindOrAdd(ID, 0);
        float RetryDelay = FMath::Min(120.0f, 0.5f + RetryCount * 0.5f);
        RetryCount++;

        UE_LOG(LogTemp, Display, TEXT("%.0f秒后重试查询"), RetryDelay);

        // 设置重试定时器
        FTimerDelegate TimerDel;
        TimerDel.BindUObject(this, &AHunyuan3DActor::QueryTaskResult);
        GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, RetryDelay, false);
    }
    else if (Status.Equals(TEXT("failed"), ESearchCase::IgnoreCase) || 
             Status.Equals(TEXT("cancelled"), ESearchCase::IgnoreCase))
    {
        // 任务失败或取消
        UE_LOG(LogTemp, Error, TEXT("任务失败[%s]: %s"), *Status, *ID);
        
        // 获取错误详情
        FString ErrorMessage = TEXT("未知错误");
        if (JsonObject->HasField(TEXT("error")))
        {
            TSharedPtr<FJsonObject> ErrorObj = JsonObject->GetObjectField(TEXT("error"));
            ErrorObj->TryGetStringField(TEXT("message"), ErrorMessage);
        }
        
        UE_LOG(LogTemp, Error, TEXT("错误详情: %s"), *ErrorMessage);
        
        // 清理重试计数器
        static TMap<FString, int> RetryCountMap;
        RetryCountMap.Remove(ID);
    }
    else if (Status.Equals(TEXT("unknown"), ESearchCase::IgnoreCase))
    {
        // 未知状态
        UE_LOG(LogTemp, Warning, TEXT("任务未知状态: %s"), *ID);
        
        // 尝试恢复
        /*if (GWorld)
        {
            FTimerHandle TimerHandle;
            GWorld->GetTimerManager().SetTimer(TimerHandle, [TaskID]() {
                QueryTaskResult(TaskID);
            }, 10.0f, false);
        }*/
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("未知任务状态: %s"), *Status);
    }
}

void AHunyuan3DActor::DownloadAsset(const FString& URL, const FString& FileName)
{
    if (URL.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("下载URL为空"));
        return;
    }
    
    FHttpModule& HttpModule = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = HttpModule.CreateRequest();
    
    FString FullPath = FPaths::Combine(SAVE_PATH, FileName);
    
    UE_LOG(LogTemp, Display, TEXT("开始下载资源: %s"), *URL);
    UE_LOG(LogTemp, Display, TEXT("保存路径: %s"), *FullPath);
    
    Request->SetURL(URL);
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(240); // 设置较长超时时间
    
    Request->OnProcessRequestComplete().BindUObject(this, &AHunyuan3DActor::HandleAssetDownload, FullPath);
    Request->ProcessRequest();
}

void AHunyuan3DActor::HandleAssetDownload(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bSuccess,
    FString FilePath
)
{
    if (bSuccess && Response.IsValid() && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
    {
        // 确保目录存在
        FString Directory = FPaths::GetPath(FilePath);
        if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Directory))
        {
            FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Directory);
        }
        
        // 保存文件
        TArray<uint8> Content = Response->GetContent();
        if (FFileHelper::SaveArrayToFile(Content, *FilePath))
        {
            UE_LOG(LogTemp, Display, TEXT("3D资源已保存: %s (%d KB)"), 
                *FilePath, Content.Num() / 1024);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("文件保存失败: %s"), *FilePath);
        }
    }
    else
    {
        FString ErrorMsg;
        if (Response.IsValid())
        {
            ErrorMsg = FString::Printf(TEXT("HTTP %d: %s"), 
                Response->GetResponseCode(), 
                *Response->GetContentAsString().Left(128));
        }
        else
        {
            ErrorMsg = TEXT("无响应或网络错误");
        }
        
        UE_LOG(LogTemp, Error, TEXT("资源下载失败: %s"), *ErrorMsg);
        
        // 可选：添加重试逻辑
        /*static TMap<FString, int> RetryCountMap;
        int& RetryCount = RetryCountMap.FindOrAdd(FilePath, 0);
        
        if (RetryCount < 3)
        {
            RetryCount++;
            UE_LOG(LogTemp, Warning, TEXT("10秒后重试下载 (%d/3)"), RetryCount);
            
            FTimerHandle TimerHandle;
            if (GWorld)
            {
                GWorld->GetTimerManager().SetTimer(TimerHandle, [Url = Request->GetURL(), FilePath]() {
                    DownloadAsset(Url, FPaths::GetCleanFilename(FilePath));
                }, 10.0f, false);
            }
        }*/
    }

    OnSingleAssetDownloaded(bSuccess);
}

void AHunyuan3DActor::OnSingleAssetDownloaded(bool bSuccess)
{
    CompletedDownloads++;
    
    if (bSuccess)
    {
        UE_LOG(LogTemp, Display, TEXT("资源下载成功 (%d/%d)"), CompletedDownloads, TotalDownloads);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("资源下载失败 (%d/%d)"), CompletedDownloads, TotalDownloads);
    }
    
    // 检查是否所有下载都完成
    if (CompletedDownloads >= TotalDownloads)
    {
        OnAllDownloadsCompleted();
    }
}

void AHunyuan3DActor::OnAllDownloadsCompleted()
{
    // 计算总耗时
    FTimespan Duration = FDateTime::Now() - StartTime;
    FString TimeMsg = FString::Printf(TEXT("任务总耗时: %.2f秒"), Duration.GetTotalSeconds());
    
    // 显示通知
    UE_LOG(LogTemp, Display, TEXT("%s"), *TimeMsg);
    GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TimeMsg);
    
    // 打开资源文件夹
    FString AbsolutePath = FPaths::ConvertRelativePathToFull(SAVE_PATH);
    
    // 确保路径使用正确的斜杠
    AbsolutePath.ReplaceInline(TEXT("/"), TEXT("\\"), ESearchCase::CaseSensitive);
    
    // 在Windows上打开资源管理器
    FString PlatformPath = FString::Printf(TEXT("\"%s\""), *AbsolutePath);
    FPlatformProcess::ExploreFolder(*PlatformPath);
    
    UE_LOG(LogTemp, Display, TEXT("已打开资源文件夹: %s"), *AbsolutePath);
}

