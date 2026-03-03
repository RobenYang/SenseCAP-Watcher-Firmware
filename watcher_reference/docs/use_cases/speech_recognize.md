# speech_recognize

## 目标
演示本地语音识别能力接入。

## 实现路径
- 用例目录: watcher_reference/use_cases/speech_recognize
- 构建入口: watcher_reference/use_cases/speech_recognize/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/speech_recognize/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/speech_recognize/main/speech_recognize.c
- 资源目录: spiffs/

## 依赖组件
- idf
- sensecap-watcher
- espressif/esp-sr

## 最小构建方式
- cd watcher_reference/use_cases/speech_recognize
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
音频采集与唤醒链路可沿用，命令词与动作路由建议重新定义。

## 证据来源
- watcher_reference/use_cases/speech_recognize/main/idf_component.yml
- watcher_reference/use_cases/speech_recognize/main/speech_recognize.c
- watcher_reference/use_cases/speech_recognize/CMakeLists.txt
