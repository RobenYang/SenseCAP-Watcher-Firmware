# qrcode_reader

## 目标
演示二维码识别流程与图像处理接入。

## 实现路径
- 用例目录: watcher_reference/use_cases/qrcode_reader
- 构建入口: watcher_reference/use_cases/qrcode_reader/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/qrcode_reader/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/qrcode_reader/main/qrcode_reader.c
- 资源目录: 无额外资源目录

## 依赖组件
- espressif/quirc
- idf
- sensecap-watcher
- esp_jpeg_simd

## 最小构建方式
- cd watcher_reference/use_cases/qrcode_reader
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
图像采集与解码模块可保留，识别后业务动作建议重构为事件接口。

## 证据来源
- watcher_reference/use_cases/qrcode_reader/main/idf_component.yml
- watcher_reference/use_cases/qrcode_reader/main/qrcode_reader.c
- watcher_reference/use_cases/qrcode_reader/CMakeLists.txt
