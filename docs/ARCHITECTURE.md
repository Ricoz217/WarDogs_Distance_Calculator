# 架构与已验证结论

## 选择

- 使用 C++20 和 Qt 6 Widgets 构建界面，复用经过验证的 Python Qt 原型布局与 QSS 风格。Qt 以动态库形式部署，不携带 Python 运行时。
- RapidOCR 路径仅保留 PP-OCRv6 文字识别模型。游戏坐标是一行清晰文字，因此不执行文字检测和方向分类。
- 推理由 ONNX Runtime CPU 执行，预处理和 CTC 解码在本项目内实现。
- Windows OCR 通过 `Windows.Media.Ocr` 直接调用，保留原图与青绿色高对比掩码两次尝试。
- 全局热键使用 `RegisterHotKey`；截图使用按显示器绑定的 GDI `BitBlt`。
- 跨显示器框选覆盖整个 Windows 虚拟桌面，保留原生 Win32 实现以使用物理像素坐标；Qt 负责主窗口与设置窗口。

## Windows ML 验证结果

Windows 11 23H2 能读取 `PP-OCRv6_rec_small.onnx` 及其字符元数据，但在创建 `LearningModelSession` 时返回 `0x80070057`。这说明系统自带 Windows ML 无法编译当前模型图，因此主引擎改用官方 ONNX Runtime。发布包只增加一个约 16 MB 的运行库。

## 坐标与显示器

框选窗口覆盖整个 Windows 虚拟桌面。拖动起点决定物理显示器，终点限制在同一显示器内；保存值由显示器设备名及显示器内相对物理像素组成，并写入 `%LOCALAPPDATA%\\WarDogsDistanceCalculatorCpp\\settings.ini`。启动和每次 OCR 前都会按设备名重新解析显示器并验证矩形范围，防止显示器断开或分辨率变化后静默截错位置。

## 置顶结果窗口

主窗口继续持有全局热键和 OCR 状态。置顶模式使用独立的无边框 `Qt::Tool` 窗口，只镜像射程与方位，并通过 `WindowStaysOnTopHint` 映射为 Windows 顶层窗口。小窗隐藏时不会注销热键；框选结束后会恢复进入框选前的主窗口或置顶窗口。所有失败状态同步为窗口外沿红框，下一次正常状态更新时清除。

`WindowStaysOnTopHint` 依赖普通 Windows 顶层窗口顺序，适用于经过 DWM 合成的窗口化和无边框全屏。真正的独占全屏可能直接占用显示输出并覆盖置顶小窗；本项目不会为此引入进程注入或 DirectX Present Hook。

## 游戏实测

- Windows 11 x64 本机已验证窗口、无边框窗口和全屏模式可通过 `BitBlt` 捕获并识别。
- 当前实现不注入、不挂钩游戏进程，只使用全局热键和桌面截图。
- 截图成功与置顶覆盖是两件事：独占全屏即使能够截图，也可能覆盖普通顶层窗口。
- 其他游戏的独占全屏或反捕获机制仍可能返回黑图；遇到实际案例后再考虑 Desktop Duplication 后端。
