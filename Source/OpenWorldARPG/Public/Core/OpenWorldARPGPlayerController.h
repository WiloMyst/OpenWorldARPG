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

public:

	// --- 输入资产 ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* IA_ShowCursor;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

protected:
	void ShowCursorTemporarily(const FInputActionValue& Value);
	void HideCursorTemporarily(const FInputActionValue& Value);

	
};
