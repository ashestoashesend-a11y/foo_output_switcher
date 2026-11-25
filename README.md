# foo_output_switcher

A foobar2000 component that puts the **output device** selector into your layout, so you can switch between speakers, USB DACs and ASIO devices without opening `Preferences → Playback → Output` every time.

[中文说明](README.zh-CN.md)

## What it does

Adds a Default UI element called **Output device** to the layout editor (`Layout → Add element → Utilities → Output device`). It is a drop-down that lists every output device foobar2000 knows about — WASAPI, ASIO, DirectSound, UPnP, whatever the installed output components provide — with the current one already selected.

- Picking a device applies it immediately, exactly as the Preferences page would (playback restarts, as usual for a device change).
- The list is rebuilt every time the drop-down opens, so a DAC plugged in or an ASIO driver restarted since the last time shows up right away.
- Changing the device anywhere else (Preferences, another component) updates the element, because it subscribes to `output_manager_v2`'s change notifications.
- Right-click for *Refresh device list*.
- Follows the current dark/light theme and font.

There is deliberately no automatic switching and no config file: the element has no settings of its own.

## Install

1. Build `foo_output_switcher.dll` (see below). No prebuilt binaries are published.
2. Copy it to `%APPDATA%\foobar2000-v2\user-components-x64\foo_output_switcher\`.
3. Restart foobar2000, then add the element: `Layout → Add element → Utilities → Output device`, or drag it into a toolbar in layout editing mode.

Requires foobar2000 v2.x, 64-bit.

## Build

Needs Visual Studio 2022 (Build Tools are enough) with the C++ workload and ATL, plus a Windows SDK.

The component does not vendor the foobar2000 SDK; it consumes the copy inside a neighbouring `foo_openlyrics` checkout:

```
repos/
  foo_openlyrics/3rdparty/foo_SDK        <- SDK + WTL live here
  foo_output_switcher/
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build\build.ps1
# -> build\x64\Release\foo_output_switcher.dll
```

Override the SDK location with `/p:Fb2kSdk=...` (and `/p:WtlInclude=...`) if yours lives elsewhere. `SolutionDir` is passed through to the SDK's own projects, which find WTL relative to it — see the comment in `build.ps1`.

## How it works

`output_manager_v2` (foobar2000 1.4+) is the same API the Preferences page uses:

| Call | Purpose |
|---|---|
| `listDevices()` | enumerate `(name, output GUID, device GUID)` |
| `getCoreConfig()` | read the currently selected output/device GUIDs |
| `setCoreConfigDevice()` | select a device |
| `addCallback()` | get told when the configuration changes elsewhere |

The UI is a `ui_element_instance` hosting a single `CBS_DROPDOWNLIST` combo box. The combo is created much taller than it looks: a drop-down list combo uses its own window height as the height of the list that opens on click, and the excess is clipped by the element's client area.

## Licence

MIT.
