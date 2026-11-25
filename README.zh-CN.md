# foo_output_switcher（中文说明）

把 **输出设备**（output device）选择器直接放进 foobar2000 界面里的组件——换音箱、换 USB DAC、换 ASIO 设备时不用每次打开 `Preferences → Playback → Output`。

[English README](README.md)

## 功能

在布局编辑器里新增一个叫 **Output device** 的 Default UI 元素（`Layout → Add element → Utilities → Output device`），是一个下拉框：

- 列出 foobar2000 已知的**所有**输出设备——WASAPI、ASIO、DirectSound、UPnP，装了哪个输出组件就有哪些，当前在用的那个已经被选中。
- 选中即生效，和设置页里改是一样的（换设备会重启输出，行为一致）。
- **每次点开下拉框都会重新枚举设备**，所以中途插上的 DAC、重启过的 ASIO 驱动立刻就能看到。
- 在别处（设置页、别的组件）改了设备，元素会跟着更新——它订阅了 `output_manager_v2` 的变更通知。
- 右键有「Refresh device list」。
- 跟随当前的深色/浅色主题和字体。

刻意不做自动切换，也没有自己的配置文件——这个元素没有任何设置项。

## 安装

1. 自己编译出 `foo_output_switcher.dll`（不提供预编译包）。
2. 放进 `%APPDATA%\foobar2000-v2\user-components-x64\foo_output_switcher\`。
3. 重启 foobar2000，然后 `Layout → Add element → Utilities → Output device` 添加，或在布局编辑模式下拖进工具栏。

需要 foobar2000 v2.x 64 位。

## 编译

需要 Visual Studio 2022（Build Tools 即可）+ C++ 工作负载 + ATL，以及 Windows SDK。

这个组件不内置 foobar2000 SDK，而是直接复用隔壁 `foo_openlyrics` 仓库里的那一份：

```
repos/
  foo_openlyrics/3rdparty/foo_SDK        <- SDK 和 WTL 在这里
  foo_output_switcher/
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build\build.ps1
# -> build\x64\Release\foo_output_switcher.dll
```

SDK 在别处的话用 `/p:Fb2kSdk=...`（以及 `/p:WtlInclude=...`）覆盖。`build.ps1` 里额外传了 `SolutionDir`——因为 SDK 自带的几个子工程是用 `$(SolutionDir)\..\3rdparty\WTL...` 找 WTL 的，不传就编不过（`atlapp.h` 找不到）。

## 实现要点

用的是 foobar2000 1.4+ 的 `output_manager_v2`，和设置页同一个 API：

| 调用 | 用途 |
|---|---|
| `listDevices()` | 枚举 `(名字, output GUID, device GUID)` |
| `getCoreConfig()` | 读当前选中的 output/device GUID |
| `setCoreConfigDevice()` | 切换设备 |
| `addCallback()` | 别处改了配置时通知我 |

界面是一个 `ui_element_instance`，里面只放一个 `CBS_DROPDOWNLIST` 下拉框。有个坑：下拉列表型 combo 会把**自己的窗口高度**当作点击后弹出列表的高度，所以创建时要给一个大得多的高度（400px），超出的部分会被元素的客户区裁掉——看起来仍然只有一行高。

## 许可

MIT。
