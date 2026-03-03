# battery_standby_test

## 目标
验证电池电压读取、充电检测与待机相关行为。

## 实现路径
- 用例目录: watcher_reference/use_cases/battery_standby_test
- 构建入口: watcher_reference/use_cases/battery_standby_test/CMakeLists.txt
- 组件依赖清单: watcher_reference/use_cases/battery_standby_test/main/idf_component.yml

## 入口与资源
- 主要入口文件: watcher_reference/use_cases/battery_standby_test/main/battery_standby_test.c
- 资源目录: 无额外资源目录

## 依赖组件
- idf
- sensecap-watcher

## 最小构建方式
- cd watcher_reference/use_cases/battery_standby_test
- idf.py set-target esp32s3
- idf.py build

## 与新固件解耦建议
可复用电源状态检测与电池采样逻辑；将业务流程与电源策略拆分。

## 证据来源
- watcher_reference/use_cases/battery_standby_test/main/idf_component.yml
- watcher_reference/use_cases/battery_standby_test/main/battery_standby_test.c
- watcher_reference/use_cases/battery_standby_test/CMakeLists.txt
