# War Dogs 射表计算器：开发交接

更新时间：2026-09-11

这份文档用于在新的 Codex 任务中继续开发。当前应以 C++ 项目为准；Python 项目只是早期原型和行为参考。

## 新任务快速开始

工作目录：

```text
D:\C++\WarDogs_Distance_Calculator_cpp
```

远程仓库：

```text
https://github.com/Ricoz217/WarDogs_Distance_Calculator.git
```

新任务开始后先执行：

```powershell
Set-Location 'D:\C++\WarDogs_Distance_Calculator_cpp'
git fetch --prune
git switch develop
git pull --ff-only origin develop
git status
```

当前长期分支只有：

- `main`：已发布的稳定版本，目前为 v1.1.0。
- `develop`：下一轮开发集成分支，目前与 v1.1.0 的 `main` 对齐。

继续开发时从 `develop` 创建 `codex/<功能名>` 分支，在功能分支完成和验证后用 `--no-ff` 合并回 `develop`。合并并推送后删除本地和远程临时分支，避免 GitHub 首页堆积无用的 Compare 提示。只有正式发布时才将 `develop` 合并到 `main` 并创建 `vX.Y.Z` 标签，不直接在 `main` 上开发。

## 当前版本与发布状态

- CMake 项目版本：`1.1.0`。
- v1.1.0 已在 GitHub 合并并发布；`main`、`develop`、`origin/main` 和 `origin/develop` 在交接时都指向提交 `fb5813c`。
- `v1.1.0` 标签已创建并指向 `fb5813c`。
- 已构建的候选发布包：

```text
D:\C++\WarDogs_Distance_Calculator_cpp\out\WarDogsDistanceCalculator-v1.1.0-win-x64.zip
```

该 ZIP 约 33.7 MiB，包内 EXE 的 `FileVersion` 和 `ProductVersion` 都已验证为 `1.1.0`。`out/` 被 Git 忽略，换机器或源码改变后必须重新构建，不能把本地现有 ZIP 当成永远有效的产物。

仓库中还存在历史标签 `1.0.0` 和 `v1.0.0`；以后统一使用带 `v` 的标签。是否清理旧的无前缀标签应由维护者单独决定。

## 产品目标和行为

这是一个只支持 Windows x64 的游戏坐标 OCR 与射表计算工具。它不注入、不挂钩游戏进程，不读取游戏内存，也不模拟游戏输入。主要流程如下：

1. 用户框选聊天框中的坐标文字区域。
2. 全局热键从该区域截图并 OCR 基准点或目标点。
3. 获得目标点后立即计算射程和方位。
4. 主界面和置顶小窗以大字号显示结果，失败时显示红色边框。

坐标计算规则必须保持：

- `dx = target.x - base.x`
- `dy = target.y - base.y`
- 距离为 `hypot(dx, dy)`，界面按 `1 游戏单位 = 100 m` 转换。
- 方位角以正北（Y 轴正向）为 `0°`，顺时针一周，即 `atan2(dx, dy)`。
- 方位显示不补前导零，并附带 `N/NE/E/SE/S/SW/W/NW` 八方向。

## 已实现功能

- Qt 6 Widgets 深色桌面界面；布局和视觉来自经过用户验证的 Python Qt 原型。
- 大字号射程、方位角和方向标记，原始距离以次要文字显示。
- 手动输入基准点和目标点。
- RapidOCR 为默认和首选引擎，Windows 系统 OCR 为低内存兼容备用。
- 四个可配置全局热键，默认值：
  - `F8`：设置默认 OCR 区域。
  - `F9`：从默认区域识别并设置基准点。
  - `F10`：从默认区域识别目标点并立即计算。
  - `F11`：临时框选队友分享的目标坐标，立即识别并计算，不覆盖默认区域。
- 设置窗口打开期间注销四个全局热键，避免编辑热键时触发 OCR 或框选；关闭设置后恢复，保存后立即生效，无需重启。
- 坐标正则可以由用户修改；前两个捕获组依次为 x 和 y。
- 多显示器框选覆盖整个 Windows 虚拟桌面，自动判断起点所在显示器，无需先选择显示器。
- 默认 OCR 区域按显示器设备名和显示器内相对物理像素保存。
- 主界面右上角图钉进入精简置顶模式。小窗只显示射程与方位，按住拖动，双击恢复主界面。
- 截图、OCR 或解析失败时主界面和置顶小窗的外沿变红；下一次正常状态会清除。

## OCR 的关键设计决定

RapidOCR 路径没有使用文字检测和方向分类，只加载 `PP-OCRv6_rec_small.onnx` 识别模型。场景是一行清晰、横向的游戏坐标文字，直接识别更快，也避免小截图导致检测器返回空结果。推理由 ONNX Runtime CPU 完成，预处理和 CTC 解码在本项目中实现。

不要自动扩大用户框选的截图区域。此前尝试扩大区域会引入额外文字噪声，用户明确要求保持精确框选；当前 `make_capture_region` 默认 `padding = 0`。框选界面使用十字光标，也不显示跨屏居中的提示文字。

为了修复宽松框选时连续窄字符 `111` 被模型合并成 `11`，RapidOCR 会比较原图以及上下各收紧约 10% 的候选，采用平均置信度更高的结果。这个处理只在用户已选区域内部裁剪，不会向外扩张。对应回归图是：

```text
tests/data/coordinate_repeated_digit.png
```

另有混入中英文字符的回归图：

```text
tests/data/coordinate_with_extra_text.png
```

默认坐标解析会：

- 在 OCR 原文中寻找完整的 `x..., y...` 坐标对，并使用最后一个匹配。
- 容忍坐标周围的额外字符和常见分隔符。
- 将 OCR 易混字符 `l/i/I/|` 归一化为 `1`，`o/O` 归一化为 `0`。
- 捕获完整数字串，但计算时只取小数点后两位。这是用户明确选择的规则，可容忍输入光标 `|` 或坐标后紧跟其他数字。

Windows OCR 在本机对小号游戏字体的识别准确率明显低于 RapidOCR，即使输入清晰也会漏数字或混淆字符，因此只作为节省内存的备用引擎。RapidOCR 的一次实测大约 45 ms，进程私有内存大约 59 MB；这些数字只作量级参考，应使用 `ocr_benchmark` 在目标构建上复测。

## 截图、显示器与全屏

截图使用 Win32 GDI `BitBlt`。框选起点决定物理显示器，终点限制在同一显示器内。配置保存到：

```text
%LOCALAPPDATA%\WarDogsDistanceCalculatorCpp\settings.ini
```

启动和每次 OCR 前会按设备名重新解析显示器并验证矩形范围。显示器断开或分辨率变化使保存区域失效时，程序要求重新框选，不能静默使用错误坐标。

Windows 11 x64 本机游戏实测已经确认窗口化、无边框和游戏的全屏模式都能截图识别。其他游戏的独占全屏或反捕获机制仍可能返回黑图。置顶小窗依赖普通 Windows 顶层窗口顺序，适用于窗口化和无边框全屏；真正的独占全屏可能覆盖它。项目不为此引入 DLL 注入或 DirectX Present Hook。

## 代码结构

- `src/app.cpp`：Qt 主窗口、设置窗口、框选覆盖层、置顶小窗、OCR 工作线程和应用 QSS。目前 UI 大多集中在这个文件。
- `src/calculator.cpp`：距离和以北为零的方位计算。
- `src/coordinates.cpp`：OCR 正则解析、字符归一化和手动输入解析。
- `src/ocr.cpp`：ONNX Runtime 识别、图像预处理、候选裁剪和 CTC 解码。
- `src/windows_ocr.cpp`：`Windows.Media.Ocr` 及高对比备用尝试。
- `src/capture.cpp`：显示器解析和 `BitBlt` 截图。
- `src/hotkeys.cpp`：热键字符串解析与冲突校验。
- `src/settings.cpp`：INI 配置读写。
- `src/presentation.cpp`：结果格式化。
- `include/wardogs/`：对应模块接口。
- `tests/`：计算、解析、设置和真实 OCR 截图回归测试。
- `tools/ocr_benchmark.cpp`：RapidOCR 性能与内存基准。
- `tools/windows_ocr_probe.cpp`：Windows OCR 诊断。
- `docs/ARCHITECTURE.md`：较短的架构决定和游戏实测结论。
- `assets/models/`：PP-OCRv6 识别模型及许可证。
- `third_party/onnxruntime/`：x64 ONNX Runtime 头文件、导入库、DLL 和许可文件。

早期 Python 原型位于：

```text
D:\Python\WarDogs_Distance_Calculator
```

只有需要比对旧界面或历史行为时才参考它，不要继续在 Python 版本实现新功能。

## 构建与验证

要求：

- Windows 10 1809+ 或 Windows 11 x64。
- Visual Studio 2022，安装“使用 C++ 的桌面开发”。
- Qt 6.8 MSVC 2022 x64；本机路径为 `D:\Qt\6.8.3\msvc2022_64`。
- C++20。

日常 Release 构建和测试：

```powershell
.\build.ps1 -Configuration Release
```

构建、测试、部署 Qt DLL 并生成 ZIP：

```powershell
.\build.ps1 -Configuration Release -Package
```

脚本会运行以下三组 CTest：

- `wardogs_core_tests`
- `wardogs_settings_tests`
- `wardogs_ocr_tests`

提交前至少运行与改动对应的测试；OCR、解析、截图或设置变更应运行完整 Release 测试。正式发布前还应启动 `out/package/WarDogsDistanceCalculator.exe` 做一次依赖冒烟测试，并检查 EXE 版本信息和 ZIP 文件名。

应用使用静态 MSVC 运行时，但 Qt 和 ONNX Runtime 仍以 DLL 部署。直接复制构建目录中的 EXE 可能出现“找不到 Qt6Widgets.dll”；可分发程序必须使用 `build.ps1 -Package` 生成。如果 `out/package` 内的程序正在运行，清理或覆盖目录会失败，应先关闭该实例。

## 当前已知限制与后续验证

- 游戏目前只在维护者的 Windows 11 x64 机器上实测；Windows 10 1809+ 是 API 兼容目标，但尚未在真实 Windows 10 机器验证。
- 只构建 x64，不支持 x86。
- 不承诺在真正的独占全屏中显示置顶小窗。
- 不同分辨率、DPI 缩放、显示器排列和游戏 UI 缩放仍需更多玩家反馈。
- OCR 失败的正确处理是保留原文、显示失败状态并亮红框，让用户再按一次热键；不要静默猜测缺失的整数位。
- 若以后出现新的稳定 OCR 失败样本，先保存原始截图并加入 `tests/data/` 做回归，再设计通用修复。不要只根据一次失败硬编码字符或扩大截图范围。

## 给新任务的建议开场

可以把下面这段直接发给新的 Codex 任务：

```text
请先阅读 D:\C++\WarDogs_Distance_Calculator_cpp\HANDOFF.md 和仓库中的 README.md、docs\ARCHITECTURE.md。以 develop 为基线继续开发，遵守文档里的分支流程，不要直接修改 main。开始前核对实际 Git 状态；如果文档和代码不一致，以代码和 Git 历史为准并更新交接文档。
```
