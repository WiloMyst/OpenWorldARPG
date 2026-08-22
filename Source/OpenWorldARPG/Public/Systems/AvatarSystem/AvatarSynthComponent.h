// Copyright 2025 WiloMyst. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Containers/Queue.h"
#include "AvatarSynthComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class OPENWORLDARPG_API UAvatarSynthComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	// 供网络线程/主线程调用的喂数据接口
	void QueueAudio(const TArray<uint8>& InPCMData);

	// 获取绝对精准的真实音频播放时间
	float GetCurrentAudioTime() const;

	// 清空队列并重置时间
	void ResetAudioState();

protected:
	// 初始化音频参数（采样率、声道数）
	virtual bool Init(int32& SampleRate) override;

	// 引擎音频线程每帧会回调这里，索要 NumSamples 个音频点
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	// TQueue 是 UE 原生的线程安全（无锁）队列，完美适合 1写(网络) 1读(声卡) 场景
	TQueue<int16> PCMQueue;

	// 原子计数器，记录已经被声卡消费掉的真实采样点数量
	// 使用原子变量是因为：音频线程在 +1，而主线程在读取，必须保证线程安全
	std::atomic<uint64_t> TotalSamplesConsumed{ 0 };

	// 当前采样率，用于时间计算
	int32 CurrentSampleRate = 22050;

	// ====================================================================
	// 起播水位 (对话开头的底部抗抖动缓冲)
	// ====================================================================

	// 起播前需攒够的缓冲时长 (秒)。对话开头流水线领先量最薄, 是抗抖最脆弱的时刻;
	// 首块到达后先静音攒够该时长再开播, 用约 0.5s 首响换取首句期间的抖动吸收
	UPROPERTY(EditAnywhere, Category = "Avatar|Audio")
	float StartupBufferSeconds = 0.5f;

	// 起播水位 (采样点数), 由 Init 按 StartupBufferSeconds × 采样率换算
	int32 StartupBufferSamples = 0;

	// 是否已越过起播水位进入正常播放。仅对话开头关门一次,
	// 句中饥饿不重新关门, 避免每次网络抖动都补一段静音
	std::atomic<bool> bPlaybackStarted{ false };

	// 队列当前积压采样点数 (入队累加 / 消费递减), 供起播水位判断
	std::atomic<uint64_t> TotalSamplesQueued{ 0 };
};
