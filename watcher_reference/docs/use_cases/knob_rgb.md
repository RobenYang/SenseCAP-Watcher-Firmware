# knob_rgb

## 目标
演示旋钮输入与 RGB 指示灯联动控制。

## 实现路径
- 用例目录: watcher_reference/use_cases/knob_rgb
- 构建入口: watcher_reference/use_cases/knob_rgb/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/knob_rgb/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/knob_rgb/main/knob_rgb.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/knob_rgb
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
输入事件与灯效控制建议拆成独立服务，后续可替换为新交互策略。

## 证据来源
- watcher_reference/use_cases/knob_rgb/main/idf_component.yml
- watcher_reference/use_cases/knob_rgb/main/knob_rgb.c
- watcher_reference/use_cases/knob_rgb/CMakeLists.txt
