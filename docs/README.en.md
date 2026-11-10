# AudioDuplo

**One sound. Two destinations.** A native Windows app that mirrors audio from one playback device to a second device, while leaving the first device's original playback path intact.

Built from an independent copy of Audio Duplo V4 QCY. This edition removes brand-specific restrictions and adds a desktop interface, destination volume, saved preferences, diagnostics, tray support, metadata and release tooling.

## Use

Windows 10 1703+ or Windows 11, x64. Connect two playback devices, extract the portable ZIP and run `AudioDuplo.exe`. Select the device your player is already using in the first card and the additional output in the second card. Start playback, then select **Compartilhar áudio**.

The compact graphite UI is currently in Brazilian Portuguese. The main screen contains two device selectors, destination volume, the share action and Settings. Buffer, diagnostics, tray and help are in Settings. Brief pointer hover and screen transitions honor Windows reduced motion; keyboard navigation is immediate. The app does not change your default output, pair devices, record audio, install virtual drivers or send telemetry. Settings are stored in `%LOCALAPPDATA%\AudioDuplo\settings.ini`.

## Compatibility

Brand-independent WASAPI shared-mode playback endpoints: wired, USB, Bluetooth and HDMI/DisplayPort. Two devices per session. Hardware and driver support still matters; this is not a claim of universal validation. Bluetooth delay and microphone-triggered call profiles cannot be eliminated by the app. Exclusive/ASIO playback and protected media are outside its scope.

Internal processing uses 48 kHz stereo float. Input supports common PCM/float formats, mono and conventional surround downmix. App buffering is adaptive from 20 to 80 ms, or manually selected up to 160 ms; this is not total end-to-end latency. No perfect synchronization or bit-perfect claim is made.

## Build

Install Visual Studio C++ Build Tools, a Windows SDK and CMake. Run:

```powershell
.\scripts\Build.ps1 -Package
```

The script builds Release x64, runs the tests and creates a portable ZIP plus SHA-256 checksum. No third-party audio runtime is required. GitHub workflows provide CI and a draft release on a version tag. Binaries are unsigned unless the maintainer adds signing.

Read [compatibility](COMPATIBILITY.md), [validation](VALIDATION.md), [architecture](ARCHITECTURE.md) and [publishing](PUBLISHING.md) for details. MIT licensed; independent of Microsoft and headphone manufacturers.
