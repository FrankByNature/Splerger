# Splerger — design

A conveyor attachment, same 4 m × 4 m footprint as the vanilla splitter / merger,
with **two inputs and two outputs**.

## Port layout (actor local frame, top view)

The vanilla attachments have Input1 on −X and Output1 on +X (see `CA_Splitter` /
`CA_Merger` in `../../sbp_editor/definitions.py`). The Splerger keeps that
straight-through pair so the build gun can still snap it onto the middle of a belt,
and puts the second pair on the sides:

```
                 +Y
            ┌──────────┐
            │  Input1  │  (+Y face, Direction = Input)
  Input0 ──►│          │──► Output0
   (−X)     │          │     (+X)
            │  Output1 │  (−Y face, Direction = Output)
            └──────────┘
                 −Y
```

| Component | Location (cm) | Rotation | Direction |
|---|---|---|---|
| Input0  | (−200,    0, 100) | facing −X | Input  |
| Output0 | ( 200,    0, 100) | facing +X | Output |
| Input1  | (   0,  200, 100) | facing +Y | Input  |
| Output1 | (   0, −200, 100) | facing −Y | Output |

Copy the exact offsets (face distance and belt height) from the vanilla splitter
Blueprint in the starter project; the numbers above are the shape, not calibrated.
Every connection component's forward (X) axis must point **out** of the building.

## Behaviour

Same model as the vanilla attachments in game 1.x: the buffer inventory
(`mBufferInventory`) has one slot per output, and each output connection serves belts
straight from its slot (`SetInventory` + `SetInventoryAccessIndex`).

- **Merge**: each factory tick, items are taken from the inputs round-robin.
- **Split**: each item goes into the slot of the output whose turn it is. If that
  slot is still full (backed up) or the output isn't connected, it goes to the
  other output, so a blocked output sends everything to the other one.
- Net effect: up to 2 belts' worth in, split evenly across 2 belts out. Throughput
  is capped by the belts, not the building.

Code: `Mod/Splerger/Source/Splerger/{Public,Private}/BuildableSplerger.*`.

## Cost and unlock

- Recipe: 2 Iron Plate, 2 Cable, 2 Iron Rod (splitter + merger costs combined).
- Unlocked by a Tier 2 milestone (same tier as the vanilla Logistics milestone).

## Later ideas

- Mode switch (interaction UI): split-only / merge-only / priority input.
- Own mesh instead of the vanilla splitter's.
- sbp_editor support: add a `"Splerger"` entry to `BUILDING_DEFS` with its class path
  `/Splerger/Buildable/Build_Splerger.Build_Splerger_C` once the mod builds.
