# Shared Vehicle Alignment Implementation Plan

> Execute inline in the existing development checkout; use test-driven-development and requesting-code-review. The user approved the shared-anchor design and requested implementation.

**Goal:** Vehicles with the same Alignment Group reuse one saved calibrated anchor, with individual model offsets intact.

**Architecture:** Add `FName AlignmentGroup` to `UCXMRVehicleProfile`. Placement resolves persistence by group when assigned, validates the group's calibration marker set, and retains an already accepted in-session pose only when the loaded layout matches. Keep per-vehicle loading unchanged for `None`.

**Tech Stack:** Unreal Engine 5.7 C++, UPROPERTY Data Assets, JSON persistence, Unreal automation.

## Global Constraints

- English UI/tooltips, concise Korean code comments.
- No mesh edits, no guessed authored eye/offset values.
- No automatic commit/push/merge for this implementation request.
- Synthetic validation does not prove headset alignment accuracy.

## Task 1: Persistence and switching

Files: `CXMRVehicleProfile.h`, `CXMRPlacementComponent.h/.cpp`, `CXMRPlacementPersistence.cpp`, `CXMRPlacementAlignment.cpp`, `Tests/CXMRDriverEyeAlignmentTests.cpp` under `Plugins/CXMR/Source/CXMR`.

Interfaces: `FName AlignmentGroup = NAME_None`; `FName UCXMRPlacementComponent::GetAlignmentGroup() const`; existing `GetCalibrationFilePath`, `LoadCalibrationFromDisk`, `SaveAlignment`, `SetMarkerProfile` remain the runtime entry points.

- [x] Add real-world fixture tests for three group vehicles with different offsets/classes/profile IDs, marker restoration and resave propagation, individual/group isolation, marker-set mismatch, capture-scope invalidation and group-file identity validation. Assign the not-yet-existing reflected name through `FNameProperty` so tests compile before implementation.

```cpp
Fixture.Root->Loader->LoadVehicle(Other);
TestTrue(TEXT("A compatible group member keeps the accepted anchor"), Fixture.Root->GetActorTransform().Equals(AcceptedPose, 0.01));
TestTrue(TEXT("The new model retains its authored offset"), Fixture.Root->Loader->GetSpawnedVehicle()->GetRootComponent()->GetRelativeTransform().Equals(Other->VehicleRootOffset, 0.001));
```

- [x] Build GMTCK_MREditor and run `Automation RunTests CXMR.Alignment.Group` to verify missing sharing causes test failures.
- [x] Implement group-scoped path/metadata, strict calibration-only layout validation, capture scope invalidation and identical-layout session preservation. Do not bypass existing fresh capture or restoration confirmation gates.

```cpp
if (!GetAlignmentGroup().IsNone())
{
    // Shared files require the current group's complete validated calibration IDs.
    // Only calibration offsets are serialized/applied; vehicle metadata stays per vehicle.
}
```

- [x] Build and run the new tests, then the full CXMR suite; fix any observed regression with a targeted test.

## Task 2: Scope visibility and usage

Files: `SCXMRControlPanel.cpp`, `CXMRTuningWindowComponent.h/.cpp`, `Docs/Operating-Guide.md`, `Docs/Headset-Test-Checklist.md`.

- [x] Show `Alignment: Vehicle only` or `Alignment Group: <name>. Save Alignment and Reset Group Alignment affect this group` on the placement page. Author grouping in vehicle Data Assets rather than adding a runtime group editor.
- [x] Document group setup, common anchor/model offsets, shared marker-ID requirements, first-save procedure, resave/reset scope, legacy isolation, and hardware checks.
- [x] Inspect the complete patch and request a read-only review. Record build/test evidence and any untested runtime behavior; leave the feature branch and changes available for review.

## Verification

- GMTCK_MREditor Win64 Development build succeeded.
- Full CXMR automation: 68 passed, 0 failed, process exit code 0. Log: Saved/AlignmentGroupFinal.log.
- 12 new group tests exercise real actor loading, placement, JSON files and control-panel commands. New sharing, scope, learning and reset failures were observed before their fixes.
- Read-only review findings were fixed and verified; final reviewer reported no remaining concrete P1/P2 in the reviewed change.
- Driver eye/model offset authoring and headset alignment accuracy are not tested on hardware. No project Data Assets were assigned speculative group values.
- Implementation verified on feature/shared-alignment-groups; publication follows the user request.
