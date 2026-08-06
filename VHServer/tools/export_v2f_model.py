"""
Audio2Face V2F 模型导出脚本

导出基于音频能量驱动的面部动画生成模型为 ONNX 格式。
模型通过分析 PCM 音频的短时能量, 驱动 ARKit 52 维 BlendShape 中的关键口型参数。

模型输入: [batch, seq_len] float32 PCM 音频 (-1.0 ~ 1.0)
模型输出: [batch, num_frames, 52] float32 BlendShape 权重 (0.0 ~ 1.0)

架构说明:
- 按帧切分音频 (22050Hz / 30FPS = 735 samples/frame)
- 计算每帧能量 (绝对值均值), 乘以增益因子后截断到 [0, 1]
- 线性映射到 52 维 BlendShape, 关键权重:
  - JawOpen (idx 17): 主驱动, 正比于能量
  - mouthClose (idx 18): 反比于能量
  - mouthSmileLeft/Right (idx 23/24): 轻微微笑联动

运行方式:
  python export_v2f_model.py
输出:
  ../models/v2f_audio2face.onnx
"""

import torch
import torch.nn as nn
import torch.nn.functional as F
import os
import onnx


class Audio2FaceV2F(nn.Module):
    """音频驱动面部动画生成模型"""

    def __init__(self, fps=30, sample_rate=22050):
        super().__init__()
        self.frame_size = sample_rate // fps  # 735 samples per frame

        # 线性映射层: 1维能量 -> 52维 BlendShape
        self.mapper = nn.Linear(1, 52, bias=False)
        nn.init.zeros_(self.mapper.weight)

        # 初始化关键口型联动权重
        with torch.no_grad():
            self.mapper.weight[17, 0] = 1.2   # JawOpen
            self.mapper.weight[18, 0] = -0.5  # mouthClose
            self.mapper.weight[23, 0] = 0.2   # mouthSmileLeft
            self.mapper.weight[24, 0] = 0.2   # mouthSmileRight

    def forward(self, audio_pcm):
        # audio_pcm: [B, seq_len]
        x = audio_pcm.unsqueeze(1)                              # [B, 1, seq_len]
        x_abs = torch.abs(x)                                    # 取绝对值作为能量
        energy_frames = F.avg_pool1d(
            x_abs, kernel_size=self.frame_size, stride=self.frame_size
        )                                                       # [B, 1, num_frames]
        energy_frames = torch.clamp(energy_frames * 6.0, 0.0, 1.0)  # 增益 + 截断
        energy_frames = energy_frames.transpose(1, 2)           # [B, num_frames, 1]
        blendshapes = self.mapper(energy_frames)                # [B, num_frames, 52]
        blendshapes = torch.clamp(blendshapes, 0.0, 1.0)        # 输出范围约束
        return blendshapes


if __name__ == '__main__':
    model = Audio2FaceV2F()
    model.eval()

    # 构造示例输入: 1秒音频
    dummy_input = torch.randn(1, 22050)

    os.makedirs("../models", exist_ok=True)
    onnx_path = "../models/v2f_audio2face.onnx"

    print(f"Exporting Audio2Face V2F model to {onnx_path}...")

    torch.onnx.export(
        model,
        dummy_input,
        onnx_path,
        export_params=True,
        opset_version=13,
        input_names=['audio_pcm'],
        output_names=['blendshapes'],
        dynamic_axes={
            'audio_pcm': {0: 'batch_size', 1: 'seq_len'},
            'blendshapes': {0: 'batch_size', 1: 'num_frames'}
        }
    )

    # 覆写 IR 版本为 8 (兼容旧版 ONNX Runtime)
    onnx_model = onnx.load(onnx_path)
    onnx_model.ir_version = 8
    onnx.save(onnx_model, onnx_path)

    onnx.checker.check_model(onnx.load(onnx_path))
    print(f"ONNX model verified: {onnx_path}")
    print(f"  Input:  audio_pcm    [batch, seq_len] float32")
    print(f"  Output: blendshapes  [batch, num_frames, 52] float32")
