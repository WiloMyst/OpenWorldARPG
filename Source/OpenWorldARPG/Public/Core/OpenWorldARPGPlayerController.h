// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenWorldARPGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

	void ShowCursorTemporarily(const FInputActionValue& Value);
	void HideCursorTemporarily(const FInputActionValue& Value);

public:
    /** 统一的输入状态仲裁器 */
    UFUNCTION(BlueprintCallable, Category = "Input")
    void UpdateInputMode();

protected:
    // --- 配置：UI ---

    UPROPERTY(EditDefaultsOnly, Category = "Config|UI")
    TSubclassOf<UUserWidget> MainHUDClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> MainHUDInstance;

	// --- 输入仲裁变量 ---
    bool bIsAltKeyDown = false;
	
public:
	// --- 输入资产 ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* IA_ShowCursor;

};
