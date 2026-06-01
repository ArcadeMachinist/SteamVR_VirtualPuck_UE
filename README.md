# ArcadeCabVR

Virtual SteamVR tracker driver for motion platform arcade cabinets, plus an Unreal Engine plugin to consume tracker data in games.

## Overview

ArcadeCabVR exposes a motion platform's physical pitch and roll as a standard SteamVR generic tracker. Games see it as a hardware tracking device — no special integration needed beyond reading a tracker pose. The motion platform's motion controller reads shaft angles from rotary encoders on the driveshafts and streams them over UDP.

This project was created specifically for ASHLAND Technologies X5 simulator, more widely known as Dave & Busters 4 seater VR ride platform.

The project has two components:

- **SteamVR Driver** — a Windows DLL that registers a virtual tracker in SteamVR and feeds it pose data either from a random-walk mock (for development) or from UDP packets sent by the motion controller.
- **Unreal Engine Plugin** (`UnrealPlugin/ArcadeCabVRSDK`) — a Blueprint Function Library for querying connected trackers by serial number or motion source within UE projects.

### Why a SteamVR driver instead of direct UDP in Unreal

The driver approach makes the motion platform look like standard hardware to SteamVR. Any game on the platform — regardless of engine — can read the tracker pose without modification. Pose data is also temporally aligned with HMD pose at the SteamVR compositor level. The Unreal plugin is a convenience layer on top, not a requirement.

### Why rotary encoders, not IMUs

The driveshafts have rotary encoders that read absolute shaft angle directly. This gives zero drift, zero noise, and no integration error — properties an IMU cannot match for positional tracking. The encoder zero point is set at motion controller startup; the platform must be at rest at power-on for the reference to be accurate.

---

## Repository Structure

```
ArcadeCabVR/
├── src/
│   ├── driver_main.cpp       # Driver entry point, SteamVR provider, DLL exports
│   ├── tracker_device.h/.cpp # Tracker device: mock and network pose modes
│   ├── config.h/.cpp         # Config file loader (JSON)
│   ├── udp_listener.h/.cpp   # UDP multicast receiver, packet parser
│   └── log.h                 # VRDriverLog() helper
│
├── UnrealPlugin/
│   └── ArcadeCabVRSDK/       # Drop into your UE project's Plugins/ folder
│       ├── ArcadeCabVRSDK.uplugin
│       ├── ArcadeCabVRSDK.Build.cs
│       └── Source/ArcadeCabVRSDK/
│           ├── Public/TrackerBFL.h
│           └── Private/
│               ├── TrackerBFL.cpp
│               └── ArcadeCabVRSDKModule.cpp
│
├── tools/
│   └── device_list.cpp       # Utility: lists all connected SteamVR devices
│
├── resources/
│   ├── input/
│   │   └── arcadecabvr_tracker_profile.json  # SteamVR input profile
│   └── settings/
│       └── default.vrsettings                # Driver enable flag
│
├── vendor/
│   ├── openvr/               # OpenVR SDK (git submodule)
│   └── nlohmann/json.hpp     # JSON parsing (single header)
│
├── driver_stage/
│   └── arcadecabvr/          # Staged build output, ready for install
│
├── cmake/
│   └── copy_if_missing.cmake # Helper: copies default config without overwriting
│
├── config.json.default       # Default config template (source-controlled)
├── CMakeLists.txt
├── build.ps1                 # Build script (Visual Studio, x64)
├── install.ps1               # Install script (copies to SteamVR drivers folder)
└── driver.vrdrivermanifest
```

---

## Building

Requirements:
- Visual Studio 2022 (Desktop C++ workload)
- CMake 3.20+ (`winget install Kitware.CMake`)

```powershell
.\build.ps1           # Release build (default)
.\build.ps1 Debug     # Debug build
```

Output is staged to `driver_stage\arcadecabvr\`.

---

## Installing

Close SteamVR completely before installing.

```powershell
.\install.ps1
```

This copies the staged driver to:
```
C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\arcadecabvr\
```

`config.json` is copied on first install only. Subsequent installs preserve your existing config.

---

## Configuration

After the first install, edit:
```
C:\Program Files (x86)\Steam\steamapps\common\SteamVR\drivers\arcadecabvr\config.json
```

| Key | Default | Description |
|-----|---------|-------------|
| `VirtualPuckDriver_Mode` | `"mock"` | `"mock"` for random-walk testing, `"network"` for live motion data |
| `VirtualPuckDriver_Debug` | `0` | Set to `1` to log every received UDP packet to vrserver.txt |
| `VirtualPuckDriver_TrackerName` | `"ARCADECABVR_01"` | Serial number exposed to SteamVR — used by games to identify the device |
| `VirtualPuckDriver_TrackerModel` | `"ArcadeCabVR Tracker v1"` | Model name shown in SteamVR device list and settings |
| `VirtualPuckDriver_ListenIP` | `"239.255.42.120"` | Multicast group to join |
| `VirtualPuckDriver_LocalIP` | `"0.0.0.0"` | Local network interface to join the multicast group on. Set to your LAN adapter IP when receiving from external hosts |
| `VirtualPuckDriver_ListenPort` | `8205` | UDP port to listen on |
| `VirtualPuckDriver_ClientIP` | `"0.0.0.0/0"` | CIDR filter — only packets from this range are accepted |
| `VirtualPuckDriver_InvalidatePoseAfter` | `2000` | Milliseconds of silence before pose is marked invalid. `-1` to never invalidate |

Restart SteamVR after editing the config.

---

## Network Mode

In `"network"` mode the driver listens for UDP packets multicasted to `VirtualPuckDriver_ListenIP:VirtualPuckDriver_ListenPort`. The motion controller sends these packets at ~90 Hz.

### Packet Format

All fields are little-endian. Total size: **21 bytes**.

| Offset | Size | Field | Notes |
|--------|------|-------|-------|
| 0 | 1 | `Version` | Must be `1` |
| 1 | 3 | `Preamble` | Must be `0x00 0x00 0x00` |
| 4 | 1 | `PacketType` | `1` = ResetPuckIdentity, `2` = TrackingFrame |
| 5 | 8 | `FrameId` | `uint64`, monotonically increasing. Late frames (lower ID than last seen) are discarded |
| 13 | 4 | `Pitch` | `float`, radians |
| 17 | 4 | `Roll` | `float`, radians |

### Packet Types

- **ResetPuckIdentity (1)** — Zeroes pitch and roll and resets the frame ID counter. Send this on motion controller startup.
- **TrackingFrame (2)** — Normal tracking data. Pitch and roll in radians, no magnitude limit enforced by the driver.

### Firewall

Windows Firewall blocks inbound UDP by default. Add a rule:

```powershell
New-NetFirewallRule -DisplayName "ArcadeCabVR UDP" -Direction Inbound -Protocol UDP -LocalPort 8205 -Action Allow
```

### Testing with Python

```python
import socket, struct, time, math

MULTICAST_GROUP = '239.255.42.120'
PORT = 8205

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 32)

def make_packet(frame_id, pitch, roll, pkt_type=2):
    return struct.pack('<B3sBQff', 1, b'\x00\x00\x00', pkt_type, frame_id, pitch, roll)

# Send reset first
sock.sendto(make_packet(0, 0.0, 0.0, pkt_type=1), (MULTICAST_GROUP, PORT))

frame_id = 0
t = 0.0
while True:
    pitch = math.sin(t) * 0.3
    roll  = math.sin(t * 0.7) * 0.3
    sock.sendto(make_packet(frame_id, pitch, roll), (MULTICAST_GROUP, PORT))
    frame_id += 1
    t += 0.011
    time.sleep(0.011)
```

---

## Logs

Driver logs go to SteamVR's log file:
```
C:\Program Files (x86)\Steam\logs\vrserver.txt
```

Search for `[ArcadeCabVR]` to filter driver output. Enable `VirtualPuckDriver_Debug: 1` in the config to log individual packets.

---

## Clearing a Stale Device Entry

SteamVR remembers past devices by serial. If you change `VirtualPuckDriver_TrackerName` or need to clear an old entry:

1. Close SteamVR (`vrserver.exe` must not be running)
2. Open `C:\Program Files (x86)\Steam\config\steamvr.vrsettings`
3. Remove any section referencing the old serial
4. Delete `C:\Program Files (x86)\Steam\config\driver_arcadecabvr.vrconfig` if it exists
5. Restart SteamVR

---

## Unreal Engine Plugin

Copy `UnrealPlugin/ArcadeCabVRSDK` into your project's `Plugins/` folder. Enable the plugin in the Unreal Editor. The SteamVR plugin must also be enabled.

### Blueprint API

**`GetAllTrackers()`** — Returns an array of `FVRTrackerInfo` for all connected generic trackers.

**`FindMotionSourceBySerial(Serial)`** — Returns the motion source name (e.g. `Special_1`) for a given serial number.

**`GetSerialForMotionSource(MotionSource)`** — Reverse lookup.

**`IsTrackerActiveBySerial(Serial)`** — Returns true if the tracker has a valid pose.

### `FVRTrackerInfo`

| Field | Type | Description |
|-------|------|-------------|
| `MotionSource` | `FName` | e.g. `Special_1` — use with `Get Motion Controller Data` |
| `Serial` | `FString` | Matches `VirtualPuckDriver_TrackerName` in config |
| `Model` | `FString` | Matches `VirtualPuckDriver_TrackerModel` in config |
| `bIsTracked` | `bool` | True if pose is currently valid |

To get pitch and roll in a Blueprint, use `Get Motion Controller Data` with the `MotionSource` from `FindMotionSourceBySerial`, then decompose the returned rotation.
