# bytetrack

## 目标
演示 ByteTrack 目标跟踪算法在设备侧的集成方式。

## 实现路径
- 用例目录: watcher_reference/use_cases/bytetrack
- 构建入口: watcher_reference/use_cases/bytetrack/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/bytetrack/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/bytetrack/main/bytetrack.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- byte_track

## 最小构建方式
- cd watcher_reference/use_cases/bytetrack
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
将跟踪算法适配层抽象为独立模块，避免与 UI/网络耦合。

## 证据来源
- watcher_reference/use_cases/bytetrack/main/idf_component.yml
- watcher_reference/use_cases/bytetrack/main/bytetrack.c
- watcher_reference/use_cases/bytetrack/CMakeLists.txt
