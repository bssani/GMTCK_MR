# CXMR design-option review implementation plan

> For agentic workers: use subagent-driven-development with task-specific file ownership and a final integration review.

**Goal:** simplify CXMR and support safe part-level design comparisons in a clear MR/VR control window.

**Architecture:** fixed VehicleAnchor owns the spawned vehicle. A vehicle-local part controller resolves stable named slot components and swaps complete assembly actors atomically. Slate presents Review, Alignment and Settings.

**Tech Stack:** Unreal Engine 5.7 C++, Blueprint/Data Assets, Slate, automation commandlet.

## Global constraints
- Follow the approved design at `Docs/superpowers/specs/2026-10-07-design-option-review-design.md`.
- English UI/tooltips; terse Korean comments. Preserve physical alignment, driver-eye references, USB hand contact and actual foveated rendering.
- Work on `feature/design-option-review`; do not push, merge or modify .codex.
- Parent coordinates build/test runs; agents must not run concurrent Unreal builds.

## Task 1: Root and retired capabilities
Files: VehicleRoot, Pawn, VehicleLoader/Profile/Types/Subsystem, VarjoInput, regression/editor tests, retired class and input assets.
- [x] Remove turntable, trim/CMF and wrist-panel runtime wiring; migrate Loader attachment directly to VehicleAnchor.
- [x] Remove root default ergonomics while retaining optional component authoring.
- [x] Parent removes estimated plug, hand grabbing/box alignment and debug overlays; keep contact/level-fit consumers.
- [x] Keep calibration/public migration semantics usable for authored Blueprint assets; provide explicit migration docs and asset-load checks.
- [x] Review surviving regression fixtures and remove tests only for retired behavior.

## Task 2: Vehicle-local part options
Create `CXMRDesignOption.h/.cpp`, `CXMRPartSlotComponent.h/.cpp`, `CXMRPartAssembly.h/.cpp`, `CXMRPartVariantComponent.h/.cpp`, `Tests/CXMRPartVariantTests.cpp`.
Parent integrates `UCXMRVehicleProfile::DesignOptions` and Loader delegates `SelectDesignOption(UCXMRDesignOption*)`, `GetPartVariants()`.
- [x] Write/run a failing runtime contract test before implementation (reflection initially permits compiling without new headers).
- [x] Add slot ID and option data validation; reject duplicate/absent/ambiguous slots and invalid transforms/classes before state changes.
- [x] Implement `bool SelectOption(UCXMRDesignOption*)`, `AActor* GetActiveAssembly(FName SlotId) const`, `UCXMRDesignOption* GetActiveOption(FName SlotId) const`, `void ClearSlot(FName SlotId)`, and `bool RemoveSlot(FName SlotId)` with explicit failure messages.
- [x] Prepare candidate inactive, verify hierarchy/USB targets, switch activity, and destroy the old part. Parent vehicle destruction must destroy assemblies.
- [x] Verify same slot A/B transform differences with a nonzero CAD/vehicle offset, failure preservation, independent slots, missing references and cleanup.

## Task 3: Review window and mode presets
Files: SCXMRControlPanel, CXMRPanelUI, CXMRControlPanelWidget, CXMRTuningWindowComponent, editor panel tests.
- [x] Write/run failing mode-isolation and interaction tests; do not test exact colors or text constants.
- [x] Implement bright neutral Slate styling with consistent readable buttons, inputs, selected states, grouping and spacing.
- [x] Present Review, Alignment, Settings. Review directly chooses catalog vehicles and authored DesignOptions via the Loader API; list available options only, no unimplemented results.
- [x] Header MR/VR choices plus truthful aligned/pending/confirmation/error state; retain reopen routes and guided alignment.
- [x] Register independent `Display.MRExposure` and `Display.VRExposure` presets, apply the active one and retain values across mode changes. Keep renderer/view-configuration fixed and MR-only controls unavailable in VR.
- [x] Parent verifies generated Slate image and commandlet asset loads, updates operating guide/checklist, and runs the full suite.

## Integration commands
```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' GMTCK_MREditor Win64 Development '-Project=C:\Git\GMTCK_MR\GMTCK_MR.uproject' -WaitMutex -NoHotReloadFromIDE
# Run CXMR automation using UnrealEditor-Cmd.exe, -nohmd -nocef -RenderOffscreen, and the local cache setup documented by the prior alignment work.
```

## Progress
- Implementation and local verification complete on 2026-10-08.
- Editor Development build succeeded; all 70 CXMR automation tests passed.
- Bundled Blueprint and L_Main load/structure verification completed with zero errors or warnings.
- Actual Slate previews inspected. Headset and authored vehicle-part content validation remain external checks.
- Changes remain uncommitted on feature/design-option-review; no push or merge.
