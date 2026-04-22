# BMP 图像查看器（C++ / Qt 版）

这是把你提供的 Python Tkinter 版 BMP 查看器，重新实现为 **C++ + Qt Widgets** 的 VSCode 可打开完整工程。

## 已实现功能

- 手动解析 BMP 文件头、DIB 头、调色板、位掩码
- 支持位深：1 / 4 / 8 / 16 / 24 / 32 位
- 支持压缩类型：BI_RGB / BI_BITFIELDS / BI_ALPHABITFIELDS
- 文件夹扫描 BMP 文件
- 单文件打开 BMP
- 左侧文件列表切换图片
- 右侧图片预览
- 状态栏实时显示鼠标所在像素坐标和原始 RGBA
- 单击像素弹窗查看信息
- 左键拖拽平移
- 滚轮垂直滚动，Shift + 滚轮横向滚动
- 缩放：放大 / 缩小 / 原始大小
- 滤镜：原图、灰度、反相、二值化、暖色、冷色、边缘增强、锐化
- 适合用 VSCode + CMake Tools 打开和构建

## 工程结构

```text
bmp_viewer_cpp/
├─ CMakeLists.txt
├─ README.md
└─ src/
   ├─ main.cpp
   ├─ BMPReader.h
   ├─ BMPReader.cpp
   ├─ ImageView.h
   ├─ ImageView.cpp
   ├─ MainWindow.h
   └─ MainWindow.cpp
```

## 环境要求

### 方案一：MSVC + Qt 6（Windows 推荐）

你需要安装：

1. **VSCode**
2. **CMake**
3. **Qt 6（含 Widgets）**
4. **Visual Studio Build Tools / MSVC 编译器**
5. VSCode 扩展：
   - C/C++
   - CMake Tools

### 方案二：MinGW + Qt 6

也可以，但 Qt 和编译器位数、ABI 必须匹配。

## VSCode 打开方式

1. 用 VSCode 打开 `bmp_viewer_cpp` 文件夹
2. 确保 Qt 和编译器已安装
3. 让 CMake Tools 选择 Kit（例如 MSVC 或 MinGW）
4. 点击 **Configure**
5. 点击 **Build**
6. 运行生成的 `BMPViewerQt`

## 命令行构建

### Windows (MSVC)

```bash
cmake -S . -B build
cmake --build build --config Release
```

### Windows (MinGW)

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## 打包 EXE（Windows）

构建完成后，可以使用 Qt 自带的部署工具：

```bash
windeployqt build\Release\BMPViewerQt.exe
```

如果是 MinGW 构建，路径可能是：

```bash
windeployqt build\BMPViewerQt.exe
```

执行后会把 Qt 运行库、平台插件等复制到 exe 同目录，形成可分发目录。

## 说明

- 为了尽可能贴近你原始 Python 版逻辑，这个版本 **没有直接调用 Qt 自带 BMP 解码器**，而是保留了“手动解析 BMP”的思路。
- `边缘增强` 和 `锐化` 使用 3x3 卷积核实现，效果与 Pillow 不会完全逐像素一致，但功能上等价。
- 如果你想继续升级，我可以在这个工程基础上再给你补：
  - `.vscode/tasks.json`
  - `.vscode/launch.json`
  - 程序图标 `.ico`
  - Windows 安装包脚本
  - Release 打包目录模板

