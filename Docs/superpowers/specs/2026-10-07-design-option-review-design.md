# CXMR design-option review

Approved by the user: simplify the template and VehicleRoot, modernize the English control window, and compare interchangeable parts on one vehicle.

## Purpose
Support senior-leader decisions through Human Factors evaluation and vehicle analysis. Keep the physical rig and accepted vehicle alignment fixed while comparing design alternatives.

## Scope
- VehicleRoot contains VehicleAnchor, Placement, and Loader. Remove the turntable node/control after migrating its attachment to VehicleAnchor. Ergonomics remains an optional component, not a root default.
- Remove trim/CMF, hand grabbing/test cubes, wrist panel/pointer, hand-touch box alignment, estimated plug alignment, gaze dots and foveation debug overlays. Keep actual hand tracking/contact, calibration level-fit math, marker diagnostics, optional eye-reference profiles, and actual foveated rendering.
- Vehicle models may carry named part-slot SceneComponents. A stable slot ID identifies a fixed mount; multiple alternatives use the same mount. Slot additions/removals are authored in the vehicle Blueprint.
- A Design Option Data Asset contains a stable Option ID, display name, Slot ID, assembly Actor soft class and slot-local offset. Vehicle Profile owns its available options. IDs must be unique; an option with a missing/ambiguous slot or invalid model is rejected without changing the accepted part.
- A part assembly Actor owns its meshes, collision and USB targets. Prepare a candidate inactive, attach and validate it, then activate it and destroy the previous assembly. No alignment or eye-reference changes on part replacement. Vehicle replacement tears down its part assemblies.
- Root remains independent of specific console/door parts. A part controller on the loaded vehicle controls assemblies; Loader exposes option selection for the control window.
- Review/Alignment/Settings are the three visible navigation items. Keep enum values compatible where practical. Review uses direct named vehicle and part choices, never fake results or measurement panels. Settings contains Visual Quality, Real-object Occlusion, Hand Contact and Diagnostics.
- Header explicitly selects MR (Real environment) or VR (Virtual environment) and shows alignment status. Use a light neutral workspace, dark text and blue selection, coherent controls and spacing.
- MR/VR retain the shared Forward/QuadView/Foveated pipeline. Add independently persisted virtual exposure presets for the two modes; keep MR-only occlusion/masks out of VR. No runtime Forward/Deferred or QuadView/Stereo switch. Hardware camera exposure stays in Varjo Base.
- UI/tooltips English, new comments terse Korean. Do not edit or delete user .codex configuration. Do not guess vehicle CAD splits or authored slot locations. Do not claim headset validation.

## Verification
Build GMTCK_MREditor and run the surviving full CXMR suite, with meaningful actor/world tests for failed and successful part replacement, invalid IDs/mounts, transform preservation, stale contact cleanup, independent slots and exposure-mode isolation. Load existing example Blueprint assets via a commandlet and fix migration issues before completion. Capture the actual Slate panel for visual inspection when available. Document Blueprint/Data Asset authoring and remaining headset checks. No automatic push or main merge.
