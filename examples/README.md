# Examples Migration Notice

`examples/` has been migrated to `watcher_reference/use_cases/`.

This directory is intentionally kept as a compatibility entry point and no longer hosts runnable demo source trees.

## Path Mapping

| Old Path | New Path |
| --- | --- |
| `examples/battery_standby_test` | `watcher_reference/use_cases/battery_standby_test` |
| `examples/bytetrack` | `watcher_reference/use_cases/bytetrack` |
| `examples/factory_firmware` | `watcher_reference/use_cases/factory_firmware` |
| `examples/get_started` | `watcher_reference/use_cases/get_started` |
| `examples/helloworld` | `watcher_reference/use_cases/helloworld` |
| `examples/knob_rgb` | `watcher_reference/use_cases/knob_rgb` |
| `examples/lvgl_demo` | `watcher_reference/use_cases/lvgl_demo` |
| `examples/lvgl_encoder_demo` | `watcher_reference/use_cases/lvgl_encoder_demo` |
| `examples/openai-realtime` | `watcher_reference/use_cases/openai-realtime` |
| `examples/qrcode_reader` | `watcher_reference/use_cases/qrcode_reader` |
| `examples/rlottie_example` | `watcher_reference/use_cases/rlottie_example` |
| `examples/speech_recognize` | `watcher_reference/use_cases/speech_recognize` |
| `examples/sscma_client_monitor` | `watcher_reference/use_cases/sscma_client_monitor` |
| `examples/sscma_client_ota` | `watcher_reference/use_cases/sscma_client_ota` |
| `examples/sscma_client_proxy` | `watcher_reference/use_cases/sscma_client_proxy` |

## Build Entry Example

```sh
cd watcher_reference/use_cases/helloworld
idf.py set-target esp32s3
idf.py build
```
