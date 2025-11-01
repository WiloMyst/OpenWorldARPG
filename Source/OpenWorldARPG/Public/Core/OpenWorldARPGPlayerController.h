// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenWorldARPGPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * 
 */
UCLASS()
class OPENWORLDARPG_API AOpenWorldARPGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	// --- 输入资产 ---

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	// 光标显示 输入动作
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* ShowCursorAction;

protected:
    // 在游戏开始时调用
    virtual void BeginPlay() override;

    // 绑定输入回调
    virtual void SetupInputComponent() override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for Jumping input */
	void Jump(const FInputActionValue& Value);
	void StopJumping(const FInputActionValue& Value);

	/** 当按下Alt键时调用 */
	void ShowCursorTemporarily(const FInputActionValue& Value);

	/** 当松开Alt键时调用 */
	void HideCursorTemporarily(const FInputActionValue& Value);

	
};
