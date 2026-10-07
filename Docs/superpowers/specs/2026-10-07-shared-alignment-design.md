# Shared Vehicle Alignment

The approved design shares a calibrated anchor between vehicle profiles while retaining each vehicle's imported-model offset and driver eye reference.

- Add an optional `Alignment Group` name to the vehicle Data Asset. `None` keeps current per-vehicle persistence. Matching group names select one shared alignment file, independent of vehicle class, asset path, model offset and marker-profile file name.
- Share only calibration marker poses in anchor-local space and validated restoration metadata. Each vehicle retains dynamic-object marker offsets, tracking settings and model authoring.
- Group members must use the same positive calibration marker IDs. Reject incompatible or mismatched group files before changing runtime state. Group saving requires a complete validated layout and cannot overwrite a valid file with an incomplete layout.
- Existing individual files remain untouched. Assigning a group does not silently import an individual/legacy file. The first group alignment follows the existing initial-align, fine-adjust, confirm, fresh-capture and save flow.
- After a confirmed shared alignment, switching among compatible group members in the same session preserves the accepted anchor and does not realign to the current HMD. A fresh session restores from current marker observations using the existing confirmation rules. Unsaved adjustments are not accepted as a saved group pose.
- Editing the group during capture invalidates that capture. A save failure preserves the previous file and runtime calibration.
- Legacy individual-marker Learn is rejected before mutation for grouped vehicles; grouped adjustment APIs use the complete fresh-capture workflow. A nonmanual re-save cannot copy an old layout into a changed group without reloading.
- The placement panel names the current save scope. A separate Setup action, `Reset Group Alignment`, deletes the group's save. `Reset Adjustment` retains its temporary-adjustment meaning.

Constraints: UE 5.7, English UI/tooltips, concise Korean code comments, no mesh edits, no asset-value guessing, no automatic commit/push/merge. Headset alignment remains unverified by synthetic tests.
