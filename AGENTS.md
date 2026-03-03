# AGENTS.md：Watcher 自研固件“强隔离”规范（根目录）

## 摘要
在仓库根目录建立“强隔离 + 独立工作区”规则：只把官方资料当作硬件/软件必要参数来源，不把官方实现当作设计蓝本。默认所有新固件开发落在 `firmware_custom/`，`watcher_reference/use_cases/` 视为只读且默认不读源码。

## 依据路径（已确认）
1. `watcher_reference/docs/hardware_modules/README.md`
2. `watcher_reference/docs/use_cases/README.md`
3. `watcher_devkit_baseline/README.md`
4. `components/sensecap-watcher/include/sensecap-watcher.h`
5. `components/sensecap-watcher/sensecap-watcher.c`

## 公共接口/契约变更（对协作方式的“接口”）
1. 新增三类引用级别定义并强制执行。
   - `RequiredSpec`：必要规格来源，可主动读取并引用事实。
   - `OptionalReference`：非必要参考，仅在用户明确要求时读取。
   - `BlockedReference`：默认禁止读取与复用实现逻辑。
2. 新增“例外读取单”格式（文档接口）。
   - 字段固定为：`原因`、`最小读取范围`、`提取事实`、`不复用声明`、`证据路径`。
3. 新增默认工作区契约。
   - 新开发目录固定为 `firmware_custom/`；官方目录默认只读，不作为实现起点。

## 目录分级表（原样策略）
| 分类 | 路径模式 | 默认策略 |
| --- | --- | --- |
| RequiredSpec | `watcher_devkit_baseline/**/*.md` | 可读，可提炼参数与约束 |
| RequiredSpec | `watcher_reference/docs/hardware_modules/**/*.md` | 可读，可提炼硬件时序/引脚/总线事实 |
| RequiredSpec | `components/sensecap-watcher/include/sensecap-watcher.h` | 可读，可提炼宏、接口、参数 |
| RequiredSpec(受限) | `components/sensecap-watcher/sensecap-watcher.c` | 仅可读硬件契约片段（上电、引脚、总线、时序）；禁止复用任务流/业务编排 |
| OptionalReference | `watcher_reference/docs/use_cases/**/*.md` | 仅索引级参考；默认不作为实现依据 |
| BlockedReference | `watcher_reference/use_cases/**` | 默认禁止读取源码/状态机/业务流程；仅在例外流程下最小化读取 |

## 独立工作区条款
1. `firmware_custom/` 是唯一默认开发区。
2. 新代码、设计文档、测试与验证记录统一放在 `firmware_custom/`。
3. 不在 `watcher_reference/use_cases/` 下派生、复制或改写新实现。

## 禁止事项
1. 禁止复制官方用例的函数结构、状态机、任务调度、UI 页面流转、命名体系。
2. 禁止以 `factory_firmware` 作为骨架进行改造。
3. 禁止把官方流程当作默认架构直接迁移到自研固件。

## 例外流程（缺少硬件事实时）
1. 先检索全部 `RequiredSpec` 来源。
2. 若仍缺失关键硬件事实，才可申请最小读取例外。
3. 例外读取后，必须附“例外读取单”。
4. 提取内容仅限参数/时序/接口事实，不得迁移实现结构。

### 例外读取单（固定模板）
| 字段 | 填写要求 |
| --- | --- |
| 原因 | 说明为何 `RequiredSpec` 无法覆盖当前问题 |
| 最小读取范围 | 精确到文件与最小必要片段 |
| 提取事实 | 仅记录参数、时序、引脚、接口、约束 |
| 不复用声明 | 明确不复用任务流/状态机/业务编排实现 |
| 证据路径 | 列出读取来源与对应定位 |

## 交付自检清单
每次任务交付必须说明以下三点：
1. 使用了哪些 `RequiredSpec` 来源。
2. 是否触发例外读取；若触发，附“例外读取单”。
3. 为何可证明当前实现为自主设计，而非官方实现迁移。

## 测试用例与验收场景
1. 场景：请求实现二维码功能。  
   预期：在 `firmware_custom/` 设计新模块；不读取 `qrcode_reader.c`；只引用硬件契约与必要参数。
2. 场景：请求确认 I2C/SPI 引脚。  
   预期：只引用 `hardware_modules` 与 `sensecap-watcher.h`；不给出官方业务代码路径。
3. 场景：请求“照着 factory_firmware 做一个一样的任务流”。  
   预期：按本规范拒绝复制实现，改为提供自主架构方案。
4. 场景：硬件上电顺序不明导致阻塞。  
   预期：触发例外流程，最小读取 `sensecap-watcher.c` 相关片段，只提取时序事实并出具例外读取单。
5. 场景：用户明确要求临时参考某 use_case 源码。  
   预期：允许一次性最小化读取，标注“用户授权覆盖默认隔离策略”，并保留不复用声明。

## 假设与默认值（已锁定）
1. 隔离级别采用“强隔离”。
2. 工作区采用“独立工作区”，默认目录名 `firmware_custom/`。
3. BSP 源码边界采用“仅硬件契约片段可读”。
4. AGENTS 文档语言采用中文，与当前基线文档风格一致。
