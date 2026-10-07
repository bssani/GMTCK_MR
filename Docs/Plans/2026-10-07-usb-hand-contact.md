# USB Hand Contact

**Goal:** Light the USB feedback when the generated tracked hand touches the port, without estimating a cable tip.

**Design:** CXMR generates its hand display from corrected OpenXR joints. Use the same joint positions, radii and finger connections for contact volumes. Joint spheres and finger capsules cover the displayed hand; a separate imported skeletal mesh or physics asset is not required. Both tracked hands can touch the port. Visualization visibility does not disable interaction.

The default USB input is hand contact. A small configurable contact margin accounts for tracking error; release hysteresis avoids flicker. Contact drives the existing Near light/sound, never the plug Aligned verdict. Tracking grace can preserve an existing response, but cannot create a new contact. Only the nearest eligible port reacts. Existing plug alignment remains an explicit compatibility option for existing Blueprint integrations and regression coverage.

**Constraints:** English UI, concise Korean comments, UE 5.7, unchanged vehicle geometry/materials, no hand cutouts, no guessed plug offset. Runtime hardware accuracy remains a headset check.

## Implementation

- [x] Add failing USB tests using the real port tick and a scoped hardware-tracker fixture: finger surface, bone surface, left hand, offset independence, release/loss, nearest handoff.
- [x] Build and run the new tests to confirm failure against plug-only detection.
- [x] Share hand geometry definitions between visualization and contact, then add default hand-contact input to the port.
- [x] Replace daily USB tip/angle controls with contact margin/release controls; retain legacy inputs in diagnostics.
- [x] Build and run the full CXMR automation suite, inspect the diff and request a read-only review.
- [x] Update operating instructions with contact setup and device checks.

## Verification

- GMTCK_MREditor Win64 Development build succeeded.
- Full CXMR automation: 56 passed, 0 failed; commandlet exit code 0. Log: Saved/HandContactFinal.log.
- New hand-contact regressions failed before their corresponding fixes.
- Read-only review found no remaining concrete P1/P2 defect in the reviewed change.
- Headset contact accuracy and subjective feedback are not tested.
