# 用例归档索引

本目录按每个用例一文档归档官方示例实现路径，便于在重写固件时按需检索可复用内容。

| 用例 | 用途 | 入口文件 | 依赖摘要 | 资源目录 |
| --- | --- | --- | --- | --- |
| battery_standby_test | 验证电池电压读取、充电检测与待机相关行为。 | watcher_reference/use_cases/battery_standby_test/main/battery_standby_test.c | idf, sensecap-watcher | 无额外资源目录 |
| bytetrack | 演示 ByteTrack 目标跟踪算法在设备侧的集成方式。 | watcher_reference/use_cases/bytetrack/main/bytetrack.c | idf, byte_track | 无额外资源目录 |
| factory_firmware | 官方全功能固件示例，覆盖任务流、UI、音频、AI、联网等完整链路。 | watcher_reference/use_cases/factory_firmware/main/main.c | idf, sscma_client, sensecap-watcher, esp_io_expander_pca95xx_16bit, espressif/esp-sr, chmorgan/esp-audio-player, chmorgan/esp-file-iterator, esp_jpeg_simd, iperf | docs/, spiffs/ |
| get_started | 最小化上手示例，展示基础初始化、UI显示与资源读取。 | watcher_reference/use_cases/get_started/main/get_started.c | idf, sensecap-watcher | spiffs/ |
| helloworld | 最小板级 bring-up 示例，用于确认基础 BSP 可用。 | watcher_reference/use_cases/helloworld/main/helloworld.c | idf, sensecap-watcher | 无额外资源目录 |
| knob_rgb | 演示旋钮输入与 RGB 指示灯联动控制。 | watcher_reference/use_cases/knob_rgb/main/knob_rgb.c | idf, sensecap-watcher | 无额外资源目录 |
| lvgl_demo | 演示 LVGL 显示渲染与基础控件使用。 | watcher_reference/use_cases/lvgl_demo/main/lvgl_demo.c | idf, sensecap-watcher | 无额外资源目录 |
| lvgl_encoder_demo | 演示 LVGL 与编码器输入设备协同。 | watcher_reference/use_cases/lvgl_encoder_demo/main/lvgl_encoder_demo.c | idf, sensecap-watcher | 无额外资源目录 |
| openai-realtime | 演示实时语音链路（网络、音频、会话）与设备端集成。 | watcher_reference/use_cases/openai-realtime/src/main.cpp | idf, espressif/es8311, sensecap-watcher, esp_io_expander_pca95xx_16bit, esp_lvgl_port, sscma_client | components/, deps/, firmware/ |
| qrcode_reader | 演示二维码识别流程与图像处理接入。 | watcher_reference/use_cases/qrcode_reader/main/qrcode_reader.c | espressif/quirc, idf, sensecap-watcher, esp_jpeg_simd | 无额外资源目录 |
| rlottie_example | 演示 RLottie 动画资源加载与显示。 | watcher_reference/use_cases/rlottie_example/main/rlottie_example.c | idf, sensecap-watcher | spiffs/ |
| speech_recognize | 演示本地语音识别能力接入。 | watcher_reference/use_cases/speech_recognize/main/speech_recognize.c | idf, sensecap-watcher, espressif/esp-sr | spiffs/ |
| sscma_client_monitor | 演示 SSCMA 客户端监控与图像结果路径。 | watcher_reference/use_cases/sscma_client_monitor/main/sscma_client_monitor.c | idf, sensecap-watcher, esp_jpeg_simd | 无额外资源目录 |
| sscma_client_ota | 演示 SSCMA 固件/模型 OTA 更新流程。 | watcher_reference/use_cases/sscma_client_ota/main/sscma_client_ota.c | idf, sensecap-watcher | model/, spiffs/ |
| sscma_client_proxy | 演示 SSCMA 代理通信链路与透传处理。 | watcher_reference/use_cases/sscma_client_proxy/main/sscma_client_proxy.c | idf, sensecap-watcher, esp_jpeg_simd | 无额外资源目录 |