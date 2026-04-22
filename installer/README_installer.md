# BMP 图像查看器安装包制作说明

这个目录提供的是 **Inno Setup 安装包方案**。  
它会把你已经编译完成并经过 `windeployqt` 处理的 Qt 程序，打成一个可以发给别人的 `Setup.exe` 安装包。

## 目录说明

- `BuildInstaller.iss`：Inno Setup 安装脚本
- `make_installer.bat`：自动构建发布目录并生成安装包
- `app.ico`：安装包图标占位文件说明

## 你需要先安装

1. Qt（你已经安装）
2. VS2022（你已经安装）
3. Inno Setup 6

安装 Inno Setup 后，默认编译器通常在：

```text
C:\Program Files (x86)\Inno Setup 6\ISCC.exe
```

## 推荐目录结构

把 `installer` 文件夹放在你的工程根目录中，形成：

```text
bmp_viewer_cpp/
├─ build/
├─ installer/
│  ├─ BuildInstaller.iss
│  ├─ make_installer.bat
│  └─ README_installer.md
├─ release/
└─ src/
```

## 一键生成安装包

先在工程根目录确保项目已经能编译通过，然后双击或在终端运行：

```bat
cd installer
make_installer.bat
```

执行完成后，安装包会生成到：

```text
installer\output\BMPViewerQt_Setup.exe
```

## 如果你暂时不想装 Inno Setup

也可以直接把 `release` 文件夹整体压缩发给别人。  
但更推荐安装包方式，因为：

- 用户体验更好
- 能创建开始菜单和桌面快捷方式
- 更像正式软件

## 需要你改的地方

### 1. Qt 路径
打开 `make_installer.bat`，修改这一行：

```bat
set QT_DIR=D:\AZB\Qt.11.0\msvc2022_64
```

改成你自己电脑上的 Qt 安装路径。

### 2. 发布者名称
打开 `BuildInstaller.iss`，修改：

```iss
#define MyAppPublisher "YourName"
```

### 3. 图标
如果你有自己的 `.ico` 文件，把它命名为 `app.ico` 放到 `installer` 目录即可。  
如果没有图标，可以先把 `SetupIconFile=app.ico` 这一行删除或注释掉。

## 典型流程

1. 在 VSCode 中编译工程
2. 把 `installer` 目录放进工程根目录
3. 修改 `make_installer.bat` 里的 Qt 路径
4. 安装 Inno Setup
5. 运行 `make_installer.bat`
6. 在 `installer/output` 中拿到安装包

## 备注

你的项目是 Qt Widgets 应用，因此 `windeployqt` 是必须步骤。  
否则安装到别人电脑后，很可能因为缺少 Qt 运行库而打不开。