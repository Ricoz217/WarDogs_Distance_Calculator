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

框选窗口覆盖整个 Windows 虚拟桌面。拖动起点决定物理显示器，终点限制在同一显示器内；保存值由显示器设备名及显示器内相对物理像素组成。每次 OCR 前重新按设备名解析显示器，防止显示器布局改变后静默截错位置。区域只驻留内存，重启后需要重选。

## 游戏实测

- Windows 11 x64 本机已验证窗口、无边框窗口和全屏模式可通过 `BitBlt` 捕获并识别。
- 当前实现不注入、不挂钩游戏进程，只使用全局热键和桌面截图。
- 其他游戏的独占全屏或反捕获机制仍可能返回黑图；遇到实际案例后再考虑 Desktop Duplication 后端。
