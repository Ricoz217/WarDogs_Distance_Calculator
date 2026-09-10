# War Dogs 射表计算器

Windows 游戏坐标 OCR 与射表计算工具。方位角以正北（Y 轴正向）为 `0°`，顺时针一周；距离按 `1 游戏单位 = 100 m` 显示。

当前版本：`1.0.0`

## 功能

- RapidOCR CPU 识别，Windows 系统 OCR 作为低内存备用
- 四个可配置全局热键：设置区域、基准点、目标点、快速目标
- 自动识别框选区域所在的显示器
- 支持手动输入坐标和自定义坐标正则
- 距离、方位角和八方向标记即时显示

## 使用

运行环境为 Windows 10 1809+ 或 Windows 11 x64，并需要 Microsoft Visual C++ 2015–2022 Redistributable (x64)。解压发布包后运行 `WarDogsDistanceCalculator.exe`。

默认热键：

1. `F8`：设置默认 OCR 区域。
2. `F9`：从默认区域识别并设置基准点。
3. `F10`：从默认区域识别目标点并立即计算。
4. `F11`：临时框选队友分享的坐标，立即识别并计算；不改变默认区域。

主界面支持手动输入 `x12.34, y56.78` 或 `12.34 56.78`。热键、OCR 引擎和坐标正则可在设置窗口修改。

## 构建

构建需要 Visual Studio 2022 的 C++ 桌面工作负载及 Qt 6.8 MSVC 2022 x64。脚本默认从 `D:\Qt\6.8.3\msvc2022_64` 读取 Qt，也可通过 `QT_ROOT` 指定其他位置。在 PowerShell 中进入项目目录并执行：

```powershell
.\build.ps1 -Configuration Release -Package
```

脚本会配置 Release 构建、运行测试、部署 Qt 运行库并生成：

```text
out\WarDogsDistanceCalculator-v1.0.0-win-x64.zip
```

仓库已携带 ONNX Runtime x64 运行文件和 PP-OCRv6 识别模型，构建过程不联网。

## 捕获兼容性

截图使用 Win32 GDI `BitBlt`。Windows 11 x64 本机游戏实测中，窗口、无边框窗口和全屏模式均可捕获。其他游戏的独占全屏或反捕获机制仍可能返回黑图。

## 目录

- `src/`：Qt Widgets UI、Win32 热键与截图、OCR 和计算实现
- `include/wardogs/`：模块接口
- `tests/`：纯计算、正则解析、CTC 解码和真实截图测试
- `tools/`：OCR 性能与 Windows OCR 诊断工具
- `assets/models/`：识别模型

## 许可证

项目源码采用 [MIT License](LICENSE)。随程序分发的 Qt、ONNX Runtime 和 PP-OCRv6 模型遵循各自许可证，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
