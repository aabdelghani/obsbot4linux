# OBSBOT libdev SDK — the second "v2.1.0_8" drop (2026-05-29)

**Goal:** OBSBOT re-released the SDK under the *same* version label, `v2.1.0_8`, with a
noticeably larger archive. Determine what actually changed and whether to adopt it.

**Status:** ✅ Investigated. The two archives contain **two different builds of `libdev`**,
four and a half months apart, and the new archive ships **both of them**. The public ABI is
additive and safe, but the drop brings this project nothing usable and moves one command it
relies on.

**Decision (2026-09-06): do NOT adopt. Stay on the January build.** Rationale and the
conditions that would change it are in §9.

**No physical device was connected for this work.** Every finding below is static analysis of
the shipped binaries and headers — no behavioural claim about real hardware is made, and §10
lists what that leaves unverified.

- Old drop preserved at `sdk/old/libdev_v2.1.0_8/`
- New drop at `sdk/libdev_v2.1.0_8/`
- `sdk/` is gitignored, so this file is the only record in the repo.

---

## 1. Identity of the two builds

| | OLD | NEW |
|---|---|---|
| Build date (from build path) | 2026-01-19 | 2026-05-29 |
| SONAME | `libdev.so.1.0.3` | **`libdev.so.1.0.0`** |
| BuildID (x86_64) | `7963c410645f7c06a49b8eaefe3f41e570dc7538` | `718459eeeffa08ce47469de7f07a2c01ff44fa40` |
| Build root | `/home/qy/workspace/obsbot_device_2026_1_19/` | `/home/qy/workspace/make-target/2026-5-29/` |
| Compiler | GCC 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.2) | GCC 11.4.0 (…~22.04.3) — package bump only |
| Translation units | 75 | 75 (same set) |

**The soname went backwards.** The newer library declares the lower version. Neither binary
contains the string `2.1.0` anywhere — `v2.1.0_8` exists only in the vendor's directory name,
so the soname is the *only* machine-readable version identifier, and it now misorders these
two builds.

Same 75 translation units means no new subsystems — every change is an edit inside an
existing file.

## 2. Where the +65 MiB went

The archives differ by ~20 MB compressed (68,893,928 B → 89,132,440 B), which is what is
visible on download. Unpacked the gap is much larger: 202,973,083 B → 271,088,337 B =
**+68,115,254 B (+65.0 MiB)**.

| Contributor | Delta | Share |
|---|---:|---:|
| Linux — a second library set retained | +52,894,888 B | 77.7 % |
| Windows — `.pdb` debug databases | +11,558,912 B | 17.0 % |
| macOS — duplicated `macos/macos/` tree | +3,574,456 B | 5.3 % |
| Windows — dll, lib, sample exe | +116,724 B | 0.2 % |
| Headers (`devs.hpp` +1,124, `dev.hpp` −110) | +1,014 B | ~0 % |
| Five `.DS_Store` files removed | −30,740 B | −0.05 % |

Deltas sum exactly to the measured total. **Almost none of it is new code.**

The Linux mechanism: the old drop shipped three byte-identical copies per architecture
(`libdev.so`, `.so.1`, `.so.1.0.3`). The new drop ships **four** — three copies of the May
build plus the January build retained under its old versioned name:

```
sdk/libdev_v2.1.0_8/linux/x86_64-release/
  libdev.so, libdev.so.1, libdev.so.1.0.0   → May build   (26,281,856 B, md5 adc9fba6…)
  libdev.so.1.0.3                           → January build, byte-identical to sdk/old/
```

Verified from the zip's own file listing, so this is vendor packaging — not an artifact of
unpacking over an existing tree.

## 3. ABI: additive and safe

| | OLD | NEW | Δ |
|---|---:|---:|---|
| Linux x86_64 / arm64 dynamic symbols | 734 | 756 | +22, −0 |
| Windows DLL exports | 571 | 593 | +22, −0 |
| macOS dylib exported symbols | 611 | 633 | +22, −0 |

Identical additions on all three platforms. **No symbol was removed and no public struct
changed size or layout.** Seven internal structs grew (`DevMdnsInfo` 328→360,
`DevicePrivate` 6752→6832, `DevicesPrivate` 12776→12816, `_Remo_Cdc_Status_t` 112→248, and
three small capability/notify types) — none of them appears in any shipped header, and
`DevicePrivate`/`DevicesPrivate` are pimpl types that are only forward-declared. Confirmed by
grepping each name across `include/`: zero occurrences.

Practical consequence: the new library is **drop-in** for anything compiled against the old
headers.

## 4. What is new

26 new enum names, 22 new exported functions, nothing withdrawn.

| Feature | New API | Declared in a header? |
|---|---|---|
| Device permission check | `Devices::setDevCheckPermission`, `getNeedCheckPermissionDevBySn`, `askCheckResult`, `deleteDevBySn`; `Device::isSupportPermissionCheck`, `isPubulicAccess` *(vendor typo)*, `setPublicAccess`, `get/setUserPermission`, `userId`, `setUserId` | 4 of 11 |
| On-camera presets | `Device::cameraGet/SetCameraPreset`, `cameraTriCameraPreset`, `setPresetArrivedNotifyCallback` | none |
| USB audio parameters | `Device::cameraSetUSBAudioParam`, `cameraGetUSBAudioParamR` | none |
| Relative zoom | `Device::cameraSetZoomRelativeR` | none |
| Mobile mode | `Device::cameraGet/SetMobileEnabledR` | none |
| Image test mode | `Device::cameraGet/SetImageTestEnabledR` | none |

**New hardware:** `ObsbotProdMeetFlip = 25` (`kPidMeetFlip = 0xFF06`) and a new platform
enumerator `DevPlatPw206 = 25`. The OBSBOT Meet Flip is the only product added; nothing in
the existing range changed. Not relevant to a Tiny 3.

**Fixes and hardening:**

- **`setDevConnectFailedCallback` is finally declarable.** The symbol was already exported by
  the January build, but the old `devs.hpp` only referenced it in a doc comment and never
  declared it — linkable, not callable. The new header declares it, outside the
  `ENABLE_BLE_FUNC` guard. This project does not currently use it, but it would give
  `CameraWorker::startDiscovery` a real failure signal instead of inferring one from a poll
  timeout.
- **Firmware-update path hardened** — new exception guards around copy/rename/remove in the
  MTP upgrade path (`extractFirmware failed, copy file exception: %s` and three siblings).
- **Better network diagnostics** — `can not get response for register route.` became
  `… devName: %s, wiredIp: %s, wirelessIp: %s.`
- **mDNS discovery carries more** — `DevMdnsInfo` gained `device_sn` and `permission_check`,
  so a device advertises its serial and whether it requires authorisation before connecting.
- The old drop's five `.DS_Store` files are gone.

**No third-party dependency was upgraded.** Every embedded version banner (bzip2, Boost regex,
nlohmann/json, nanopb, LZMA SDK, bsdiff, KCP, ftplib) is byte-identical.

## 5. Hazard: 18 of the 22 new APIs cannot be called

Only the four `Devices::` permission entry points made it into `devs.hpp`. Every new
`Device::` method is exported by the binary but **declared in no shipped header** — and
neither are their parameter types. `dev.hpp` is API-identical between the drops; the only
edits are deleted Chinese comments (verified by a whitespace-normalised diff: 34 lines, all
comment removals).

The definitions are recoverable from the shipped DWARF — both builds are `RelWithDebInfo` and
unstripped. Recorded here so they do not have to be re-derived:

```cpp
enum Device::ZoomRelativeType { ZoomRelativeTypeOut = -1, ZoomRelativeTypeStop = 0,
                                ZoomRelativeTypeIn = 1 };

struct Device::USBAudioParam      { uint8_t channel; uint8_t raw_src; };
struct Device::PresetActionsIndex { int8_t main_idx; int8_t sub_idx; };

struct Device::PresetActions {          // 44 bytes
    int8_t  main_idx, sub_idx, operation_type, ai_landscape_mode, ai_portrait_mode;
    int8_t  auto_focus, auto_focus_priority;
    int16_t manual_focus_val;
    int8_t  auto_exposure, auto_exposure_priority, ev_compensation;
    int8_t  auto_exposure_min_iso, auto_exposure_max_iso;
    int16_t manual_shutter, manual_iso;
    int8_t  wb_type, bax, gmy, wb_manual_mode;
    int16_t red_gain, blue_gain, manual_wb_color_temp;
    int8_t  contrast, saturation, sharpness, hue;
    uint8_t background[4];
    int16_t out_x1, out_y1, out_x2, out_y2;
};
```

Hand-declaring these is possible but unsupported, and would break silently if the vendor
changes a layout. Not recommended — wait for real headers.

## 6. Hazard: the shipped header no longer matches the binary

`dev.hpp` still ends the product enum at `ObsbotProdButt` = 20. The new binary has
`ObsbotProdMeetFlip = 25` and `ObsbotProdButt = 26`.

```
dev.hpp   ObsbotProdTiny3Lite = 19,  ObsbotProdButt              → 20
new .so   ObsbotProdTiny3Lite = 19,  ObsbotProdMeetFlip = 25,
                                     ObsbotProdButt              → 26
```

Old header and old binary agreed (both 20), so this mismatch is **new in this drop**. Any
`type < ObsbotProdButt` bounds check or `array[ObsbotProdButt]` sizing compiled against the
header is now wrong by six. This project does not do either — it switches on specific product
values (`CameraWorker.cpp:160,163`) — but it is a trap worth knowing about.

## 7. Hazard: wire protocol command IDs were renumbered

26 command names were added, but several were **inserted mid-enum** rather than appended, so
115 pre-existing commands shifted value.

| Group | Shifted | Cause |
|---|---:|---|
| `CmdCamera` | 76 | AF test params inserted at 109–110, USB audio params at 133–134 (+2 then +4) |
| `CmdAi` | 31 | `AI_NTY_PRESET_ARRIVED` inserted at 102 (+1 above it) |
| `CmdSysMg` | 8 | `SYS_MG_GET_DEVICE_INFO` inserted at 29 |
| `CmdRoute` | 0 | 5 permission commands appended cleanly at 8–12 |

These are **real wire values**, not internal indices. The command byte is packed straight into
the packet header — proved by disassembling a function whose opcode moved:

```
Device::cameraSetPowerCtrlActionR — immediate constant
  OLD   $0x8b000b00010000        0x8b = 139 = CAM_SET_POWER_CTRL (old)
  NEW   $0x8f000b00010000        0x8f = 143 = CAM_SET_POWER_CTRL (new)
```

**This couples the SDK to the camera firmware.** Whether it breaks anything depends on the
firmware using the same table, which is not visible in these files. The risk is *symmetric*
and easy to miss: today old-SDK + current-firmware works, but **updating the camera firmware
may be what breaks the old SDK.** See §9.

## 8. Impact on this project, per call

Every SDK call `qt/src/CameraWorker.cpp` makes was disassembled in both builds and its
immediate constants compared.

| Call | Drives | Command | Status |
|---|---|---|---|
| `cameraSetDevRunStatusR` | Wake & sleep (`:296`, `:307`) | `0x02` | unchanged |
| `cameraSetSuspendTimeU` | Auto-sleep timer (`:545`) | XU `0x20b` | unchanged |
| `cameraSetMicrophoneDuringSleepU` | Mic during sleep (`:559`) | XU `0x113` | unchanged |
| **`aiSetGestureParaR`** | Gesture settings (`:460`) | `0x7c` → **`0x7d`** | **changed** |
| **`aiGetGestureParaR`** | Gesture readback (`:482`, `:489`) | `0x7d` → **`0x7e`** | **changed** |
| `cameraGet/SetImage…R` | Brightness/contrast/saturation/sharpness (`:575-590`) | `0x59 0x5a 0x5c 0x5d` | unchanged |
| `gimbalSetSpeedPositionR` | Preset recall (`:356`, `:620`) | `0x03` / `0x04` | unchanged |
| `gimbalGetAttitudeInfoR`, `gimbalSpeedCtrlR`, `gimbalRstPosR` | Pan/tilt read, jog, reset | — | unchanged |
| `cameraGet/SetZoomAbsoluteR`, `cameraSetFaceFocusR`, `cameraSetWdrR` | Zoom, face focus, WDR | — | unchanged |
| `aiGetAiStatusR`, `aiSetGestureCtrlIndividualR` | AI status, per-gesture toggles | `0x13` | unchanged |

The image-control functions showed one changed constant, `0x14b0` → `0x1500`. That is a member
offset inside `DevicePrivate`, which grew by exactly 80 bytes (`0x1500 − 0x14b0 = 80`) — a
recompilation artefact, **not** a protocol change.

**Sleep and wake are untouched.** No sleep/wake/power/standby/suspend function was added or
removed, no enumerator name in that area changed, and both USB control-transfer calls emit an
identical constant set including their XU selectors. The `CAM_SET_POWER_CTRL` renumbering
applies to `cameraSetPowerCtrlActionR`, which this project never calls. So this drop neither
causes nor fixes the behaviour in the open self-wake investigation.

**Gesture control is the one exposed spot**, and it is the same surface as the planned
hands-free / gesture-mode work.

## 9. Decision: stay on the January build

**Adopting it gains this project nothing:**

- The 18 interesting new APIs are uncallable (§5).
- The 4 callable ones are permission APIs for BLE/network devices. This app is USB-only by
  explicit choice — `CameraWorker.cpp:106` calls `setEnableMdnsScan(false)`, and
  `ENABLE_BLE_FUNC` is never defined.
- Meet Flip support is irrelevant to a Tiny 3.
- Sleep/wake is byte-identical, so it does not help the self-wake investigation.

**And it costs:** wiping stale build trees, fixing the AppImage recipe, updating six docs — to
gain a camera we do not own, while moving a command we do use.

**Timing argument:** starting gesture-mode work on a freshly-swapped SDK makes "my new code is
wrong" indistinguishable from "the opcode is wrong". Bad position to volunteer for.

**A build-quality smell, noted but not decisive:** the soname regressed; the Windows release
DLL has a CodeView age of 421, i.e. it came out of a repeatedly-relinked working directory
rather than a clean build; and it was built with an *older* MSVC (14.36.32532) than the
January drop (14.38.33130). Reads like a build cut off a working branch.

### Keep both drops

`sdk/old/` must stay. It is what makes the decision reversible, and §7 means the SDK and the
camera firmware are a matched pair:

- **Do not update the camera firmware casually.** If firmware moves to the new opcode table,
  the *old* SDK is the one that breaks gesture control.
- If firmware is updated, re-test gesture first and be ready to switch SDK.

### Revisit when any of these is true

1. OBSBOT ships headers declaring the new APIs. On-camera presets would replace this app's
   host-side preset emulation (`Settings.h` `PresetData`, `CameraWorker.cpp:594-630`,
   `CameraController`'s `scheduleStartupPreset` / `aiReturnPreset` dance) with real device
   presets plus an arrival notification. That is the one genuinely attractive item.
2. The camera firmware is updated.
3. An MTP firmware-update failure appears that the new exception guards would catch.
4. Hardware arrives that needs Meet Flip support.

### If it is ever adopted, in this order

1. **Delete `qt/build`, `qt/build-appimage`, `qt/AppDir`.** `qt/build/obsbot4linux` is stale:
   it records a dependency on `libdev.so.1.0.3` and an rpath pointing at
   `/home/spawn/Apps/obsbot4linux/…`, which no longer exists since the repo moved. The
   launcher's `LD_LIBRARY_PATH` then resolves it to the **retained January library inside the
   new drop**, so any test of "the new SDK" silently exercises the old one. The cached
   `CMAKE_HOME_DIRECTORY` also points at the old path, so a rebuild cannot fix it in place.
2. **Make `qt/packaging/build-appimage.sh:73-74` soname-derived.** It hard-codes
   `libdev.so.1.0.3`. Because the new drop still contains that file the copy *succeeds* and
   quietly stages the January library, while linuxdeploy separately resolves the binary's real
   `DT_NEEDED` and stages the May one — shipping both plus a symlink to the unused one.
3. **Consider deleting the retained `libdev.so.1.0.3`** from the drop. That restores the safety
   net: a stale binary then fails loudly instead of loading a four-month-old library.
4. **Re-test gesture control on hardware** — the only command this app sends whose ID moved.
5. **Re-run the sleep/wake and mic-during-sleep readback checks** after step 1.
6. **Update the docs that state `libdev.so.1.0.3` as current fact:** `docs/INSTALL.md:19-21`,
   `gui/Makefile:3`, `sdk-probe/Makefile:8`, `sdk-probe/CMakeLists.txt:45`,
   `sdk-probe/README.md:14,72`, `gui/README.md:102`, and the stale soname comment at
   `qt/CMakeLists.txt:86` (which says `libdev.so.1` — never correct under either drop).
   Documenting only `libdev.so` as required keeps that listing from going stale again.

## 10. Non-Linux notes

**macOS — the obvious path is the stale one.** `macos/arm64-release/` and
`macos/x86_64-release/` in the new drop are **byte-identical to the old drop** and contain
**none** of the 22 new APIs (611 exported symbols; `isSupportPermissionCheck` absent). Only the
nested `macos/macos/` tree holds the May build (633 symbols, all 22 present). The nesting is a
packaging mistake and the outer directory is a trap.

**Windows.** Same +22/−0 exports. The `.pdb` growth is not explained by the code delta — the
build root moved `D:\workspace\` → `E:\code\`, and Boost 1.79 headers are now recorded from
*two* roots instead of one (source paths 1,228 → 2,016).

**File permissions.** No file in either tree carries the executable bit; the sample binaries
need `chmod +x`. Cross-tree mtimes are not comparable (a DST-aware timezone offset from how the
two archives were extracted), so staleness was judged on content, not dates.

## 11. Method, and what is NOT verified

- Headers compared after whitespace normalisation, so the reformat from tabs to 2-space
  clang-format in `devs.hpp` could not hide a change.
- Exported symbols from the dynamic symbol table (`nm -D`), cross-checked on Windows against
  both the DLL export table and the import library.
- Enum values, struct layouts, and the §5 type definitions from DWARF (`readelf --debug-dump`,
  `gdb ptype`) — both builds ship unstripped with full debug info, which is what made this
  possible.
- Command IDs by disassembling each function and comparing the immediate constants it emits.
- macOS required a hand-written Mach-O load-command parser; no Apple tooling exists on this
  machine (no `otool`, `lipo`, `llvm-nm`).
- All set comparisons re-run under `LC_ALL=C` after a locale-dependent `sort`/`comm` produced
  wrong counts on the first pass.

**Explicitly not verified:**

1. **Any behavioural claim about real hardware.** No camera was connected. §8 says which
   *commands* are unchanged, not that the camera responds identically.
2. **Whether the camera firmware uses the new or old opcode table** (§7). Not present in these
   files. This is a flagged risk, not a confirmed break.
3. **That linuxdeploy would pull `libdev.so.1.0.0` into the AppDir** (§9 step 2). Reasoned from
   its documented `DT_NEEDED` walk; the packaging script was not run.
4. **That `-DSDK_ROOT=` works as `docs/INSTALL.md:26-29` documents.** It appears not to for the
   Qt app — `qt/CMakeLists.txt:15` uses a plain `set()`, which shadows the cache entry `-D`
   creates, whereas `sdk-probe/CMakeLists.txt:17` correctly uses `CACHE PATH`. Reasoned from
   CMake scoping semantics; `cmake` was not available to run a repro. Tracked here rather than
   fixed, since it is out of scope for this investigation.

---

## Self code review

Documentation only — no code, build, packaging or runtime file was touched, so no application
test exercises anything that changed and inventing one would be noise. `git status` shows this
file as the single untracked addition.

What I checked instead, since the risk in a findings document is a wrong fact:

- **Re-verified every numeric claim against the captured evidence** rather than from memory —
  byte counts, symbol counts (734/756, 571/593, 611/633), struct sizes, opcode values and the
  shift arithmetic. The size table sums exactly to the measured 68,115,254 B.
- **Re-checked every `file:line` citation** resolves in the current tree.
- **Caught and corrected one mislabelled struct** during the investigation: an awk field-tracking
  bug attributed `DevicePrivate`'s new members (`user_id_`, `usertype_`, `preset_arrived_cb_`)
  to its nested `SendPacket` type. Confirmed the correct owner with `gdb ptype` before writing
  anything down.
- **Corrected a premature conclusion**: the retained `libdev.so.1.0.3` initially looked like a
  leftover from unpacking the new archive over the old tree. Checking the zip's own file
  listing proved it is genuinely vendor-shipped. The opposite conclusion would have made §2
  and §9 wrong.
- **Separated claims from inferences.** Everything I could not execute is listed in §11 rather
  than asserted — in particular the firmware-table question in §7, which is the one that would
  most change the recommendation if answered.

Known gap: the §7 risk cannot be closed without either vendor documentation or a hardware test.
The decision in §9 is deliberately the one that does not depend on the answer.
