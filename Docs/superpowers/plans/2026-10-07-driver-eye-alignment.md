# Driver Eye Alignment Implementation Plan

Goal: configure one driver eye reference per vehicle, align the vehicle once, adjust about that reference, and persist marker-relative alignment for subsequent sessions.

Architecture: keep the Pawn and imported geometry unchanged. Placement owns the vehicle anchor. The vehicle profile holds a vehicle-local eye transform; the loaded actor transform includes its authored offset. Manual alignment freezes automatic placement. Confirmation starts a fresh marker capture at the final pose. All configured calibration markers must be captured before one transactional save. Observations can arrive sequentially while the anchor is fixed. A saved single-marker restoration requires explicit confirmation; a valid multi-marker restoration can finish automatically.

Constraints: Unreal 5.7; English UI and tooltips; short Korean comments. Preserve existing work. Do not change Turntable geometry or trim/CMF implementation. Do not invent eye coordinates for existing assets. Commit, push and merge were authorized after implementation.

## 1. Alignment and pivot
- [x] Add failing synthetic tests under `Private/Tests/CXMRDriverEyeAlignmentTests.cpp` for missing configuration, distant imported origins, offsets, yaw-only placement, translation, fixed eye pivot and marker event isolation.
- [x] Add `bHasDriverEyeReference` and `DriverEyeReference` to `UCXMRVehicleProfile`.
- [x] Add `GetDriverEyeWorld(FTransform&)` and `AlignToDriverEyePose(FTransform)` to Placement. Resolve the reference through the loaded vehicle. Append DriverEyeReference to the pivot enum without changing old values. Manual alignment uses this pivot and latches the initial heading.

## 2. Save and restore
- [x] Test sequential captures, invalidation after adjustment, missing marker rejection, failed writes, single-marker confirmation and pose persistence after loss/reacquisition.
- [x] Add `BeginAlignmentCapture()`, `CanSaveAlignment()`, `SaveAlignment()`, capture progress and restoration state. Discard capture when the pose or vehicle changes. Save all configured calibration marker transforms relative to the fixed anchor; never use old unseen coordinates.
- [x] Write versioned calibration JSON through a temporary file. Commit profile data only after file replacement succeeds. Store validated marker IDs and vehicle identity; preserve legacy file loading.
- [x] Restore from saved IDs only. Validate multi-marker fit and candidate heading consistency. Freeze a single-marker candidate until explicitly accepted. Reapply vehicle-specific calibration even when two vehicles share a marker profile.

## 3. Operator workflow
- [x] Add `Initial Alignment` with a cancellable three-second countdown. Require positional HMD tracking and read the tracked head pose at expiry. Cancel if vehicle identity changes.
- [x] Show reference availability, workflow state, fresh capture progress and `Use Restored Alignment`. Keep Confirm/Save distinct and remove duplicate placement save/learn buttons from the unified panel.
- [x] Document the vehicle-local eye coordinate system, configuring the Data Asset, posture and marker recovery in `Docs/Operating-Guide.md`.

## 4. Verification
- [x] Build `GMTCK_MREditor Win64 Development` with UE 5.7 after ensuring no editor process is running.
- [x] Run the new tests and all `CXMR.` automation tests with offscreen rendering; inspect the generated Placement screenshot and `git diff --check`.
- [x] Report build/static automation separately from headset validation. Leave actual eye-point authoring and headset fitting explicit.

## Result

Implemented the eye reference, yaw-only anchor placement, driver-eye manual pivot, fresh sequential capture, per-vehicle transactional persistence and marker restoration. Hidden Trim/CMF from the main vehicle page; implementation remains available.

Verification on 2026-10-07:
- `GMTCK_MREditor Win64 Development` with UE 5.7: succeeded.
- Full offscreen `CXMR.` automation suite: 40 passed, 0 failed; exit code 0. Log: `Saved/DriverEyeFinal.log`.
- Placement preview inspected: `Saved/ControlPanelPreview_2.png`. Preview fixture has no vehicle, so initial alignment is disabled and explains the missing reference.
- `git diff --check`: passed.
- Independent static review findings fixed: shared marker-profile file collision, final-fit heading validation, configured model edits during capture, Unicode asset-path hashing.

Headset tracking/posture and physical fitting remain unverified. Existing vehicle assets need an authored driver eye reference; no coordinates were invented. The user subsequently authorized commit, push and merge.

Automation used a project-local DDC and shader working directory because the sandbox cannot write the default user cache. Test command:
```powershell
[System.Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', 'C:\Git\GMTCK_MR\Intermediate\DriverEyeDDC', 'Process')
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\GMTCK_MR\GMTCK_MR.uproject' -unattended -nop4 -nosplash -RenderOffscreen -nohmd -nocef -ddc=InstalledNoZenLocalFallback '-SharedDataCachePath=None' '-ShaderWorkingDir=C:\Git\GMTCK_MR\Intermediate\ShaderWorking' '-ExecCmds=Automation RunTests CXMR.;Quit' '-TestExit=Automation Test Queue Empty' '-abslog=C:\Git\GMTCK_MR\Saved\DriverEyeFinal.log'
```

Pre-commit verification repeated the full 40-test suite with `-nocef`. The initial rerun hit an engine CEF offscreen-render assertion; disabling the unrelated browser avoids it without changing project configuration.
