# 05 Audio Codec I2S

## 目标
明确音频编解码、I2S、I2C 依赖关系与调试顺序。

## 适用范围
- 麦克风采集、扬声器播放。
- 语音识别、语音播报、实时会话等音频功能。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| I2S 默认参数 | 采样率 16000, 16bit, 单声道 | components/sensecap-watcher/include/sensecap-watcher.h |
| I2S 引脚 | MCLK=GPIO10, SCLK=GPIO11, LRCK=GPIO12, DSIN=GPIO15, DOUT=GPIO16 | 同上 |
| Speaker Codec | ES8311 通过通用 I2C 接入 | components/sensecap-watcher/sensecap-watcher.c |
| Mic Codec | 优先检测 ES7243，失败再探测 ES7243E | 同上 |
| 并发保护 | 读写 codec 使用 mutex 保护 | 同上 |

## 操作流程或约束
1. 先保证通用 I2C 可用，再初始化 I2S 与 codec。
2. 复用 bsp_codec_init，避免手工拼接不一致的 codec 流程。
3. 音频异常优先排查 I2C 地址探测、I2S 引脚与采样参数一致性。
4. 若改采样率，统一更新 codec 与 I2S slot 配置。

## 证据来源
- components/sensecap-watcher/sensecap-watcher.c
- components/sensecap-watcher/include/sensecap-watcher.h
- watcher_reference/use_cases/speech_recognize/main/speech_recognize.c

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-03-02 | 初始化音频模块调试文档 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 中 | 回退默认 16k/16bit/mono 配置 | components/sensecap-watcher/* |