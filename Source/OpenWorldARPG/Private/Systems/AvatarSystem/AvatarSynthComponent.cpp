// Copyright 2025 WiloMyst. All Rights Reserved.


#include "Systems/AvatarSystem/AvatarSynthComponent.h"

bool UAvatarSynthComponent::Init(int32& SampleRate)
{
	NumChannels = 1;       // Piper TTS 是单声道
	SampleRate = 22050;    // 严格对齐大模型的采样率
	CurrentSampleRate = SampleRate;
	// 起播水位按采样率换算成采样点数, 0 表示关闭起播缓冲 (立即开播)
	StartupBufferSamples = FMath::Max(0, FMath::RoundToInt(StartupBufferSeconds * SampleRate));
	return true;
}

void UAvatarSynthComponent::QueueAudio(const TArray<uint8>& InPCMData)
{
	// 网络发来的是 uint8 字节流，实际上是 16-bit PCM，需要强转
	const int16* PcmData = reinterpret_cast<const int16*>(InPCMData.GetData());
	int32 SampleCount = InPCMData.Num() / 2; // 2 byte = 1 int16

	// 推入无锁队列（在主线程或网络线程执行）
	for (int32 i = 0; i < SampleCount; ++i)
	{
		PCMQueue.Enqueue(PcmData[i]);
	}
	TotalSamplesQueued.fetch_add(SampleCount, std::memory_order_relaxed);
}

int32 UAvatarSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	// 注意：此函数运行在极高优先级的音频渲染线程，严禁执行耗时操作和锁

	// 起播水位: 对话开头先攒够 StartupBufferSamples 再开播。
	// 未达水位期间输出静音且 TotalSamplesConsumed 不前进, 音频主时钟暂停,
	// AvatarStreamingComponent 的表情帧消费随之等待, 开播后音画仍严格对齐
	if (StartupBufferSamples > 0 && !bPlaybackStarted.load(std::memory_order_acquire))
	{
		if (TotalSamplesQueued.load(std::memory_order_relaxed) < static_cast<uint64_t>(StartupBufferSamples))
		{
			FMemory::Memzero(OutAudio, sizeof(float) * NumSamples);
			return NumSamples;
		}
		bPlaybackStarted.store(true, std::memory_order_release);
	}

	int32 SamplesConsumed = 0;
	for (int32 i = 0; i < NumSamples; ++i)
	{
		int16 Sample = 0;
		// 尝试从队列拿一点数据
		if (PCMQueue.Dequeue(Sample))
		{
			// USynthComponent 要求输出 -1.0 到 1.0 的 float。
			// int16 范围是 -32768 到 32767，除以 32768.0f 刚好归一化
			OutAudio[i] = static_cast<float>(Sample) / 32768.0f;
			++SamplesConsumed;
		}
		else
		{
			// 发生网络饥饿，直接输出静音
			OutAudio[i] = 0.0f;
		}
	}

	// 批量更新计数器：已消费数推进音频主时钟，同时扣减队列积压水位
	if (SamplesConsumed > 0)
	{
		// memory_order_relaxed 表示最轻量级的原子操作，不阻塞音频线程
		TotalSamplesConsumed.fetch_add(SamplesConsumed, std::memory_order_relaxed);
		TotalSamplesQueued.fetch_sub(SamplesConsumed, std::memory_order_relaxed);
	}
	return NumSamples;
}

float UAvatarSynthComponent::GetCurrentAudioTime() const
{
	if (CurrentSampleRate == 0) return 0.0f;
	// 真实时间 = 消耗的点数 / 采样率
	return static_cast<float>(TotalSamplesConsumed.load(std::memory_order_relaxed)) / CurrentSampleRate;
}

void UAvatarSynthComponent::ResetAudioState()
{
	// 先关门再排水: 关闭起播闸门后清空队列与计数器, 新对话重新从水位攒起
	bPlaybackStarted.store(false, std::memory_order_relaxed);
	int16 Temp;
	while (PCMQueue.Dequeue(Temp)) {}
	TotalSamplesQueued.store(0, std::memory_order_relaxed);
	TotalSamplesConsumed.store(0, std::memory_order_relaxed);
}
