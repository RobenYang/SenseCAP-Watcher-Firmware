# 06 Compatibility Log

## 目标
沉淀版本变更与验证结果，保证“允许升级但可回退”的长期可维护性。

## 适用范围
- 适用于 ESP-IDF、组件依赖、分区策略、BSP 初始化路径的任何变更。
- 适用于成功升级与失败回退两类结果。

## 固化事实
| 项目 | 结论/要求 | 证据 |
| --- | --- | --- |
| 记录时机 | 发生任何版本或关键配置变更后必须记录 | 项目流程约束 |
| 记录粒度 | 必须可定位到“变更项 + 结果 + 风险 + 回退点” | 本文件模板 |
| 脱敏要求 | 记录中禁止完整 MAC/SN/UUID/Token/URL | `watcher_devkit_baseline/README.md` |

## 操作流程或约束
### 填写步骤
1. 记录变更前基础版本（例如 `FW 1.1.7 + ESP-IDF v5.2.1`）。
2. 记录变更项（版本号或配置项）。
3. 记录验证结果（通过/失败 + 关键现象）。
4. 记录风险等级和影响面。
5. 记录可执行回退点（tag、commit、镜像包、分区快照）。
6. 填写证据路径（日志、配置文件、构建输出路径）。

### 标准记录表头（固定）
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |

### 示例：升级失败后回退（示例条目）
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | ESP-IDF 从 v5.2.1 试升到 v5.2.x（小版本） | FW 1.1.7 + ESP-IDF v5.2.1 | 失败：启动后外设链路异常，已回退 | 中 | 回退到 `ESP-IDF v5.2.1` 与起跑线依赖清单 | `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults`，`watcher_devkit_baseline/04_dependency_floor_and_fallback.md` |

### 后续记录区
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 | 待填写 |

## 证据来源
- `watcher_devkit_baseline/README.md`
- `watcher_devkit_baseline/04_dependency_floor_and_fallback.md`
- `watcher_reference/use_cases/factory_firmware/sdkconfig.defaults`

## 变更记录
| 日期 | 变更项 | 基础版本 | 验证结果 | 风险 | 回退点 | 证据路径 |
| --- | --- | --- | --- | --- | --- | --- |
| 2026-02-27 | 初始化兼容记录模板 | FW 1.1.7 + ESP-IDF v5.2.1 | 已建立 | 低 | 使用本模板持续追加 | `watcher_devkit_baseline/06_compatibility_log.md` |

