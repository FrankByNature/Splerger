# Splerger — design

A conveyor attachment, same 4 m × 4 m footprint as the vanilla splitter / merger,
with **two inputs and two outputs**.

## Port layout (actor local frame, top view)

The vanilla attachments have Input1 on −X and Output1 on +X (see the `CA_Splitter` /
`CA_Merger` Blueprints), and the vanilla conveyor-attachment hologram wires a belt it
cuts into the components **named** `Input1` / `Output1`. So the Splerger's
straight-through pair must carry those names; the side pair is `Input2` / `Output2`.
(1.0.0 had them as Input0/Output0 + Input1/Output1, which put a cut belt on the side
ports and left no free ports for new belts.)

```
                 +Y
            ┌──────────┐
            │  Output2 │  (+Y face, Direction = Output)
  Input1 ──►│          │──► Output1
   (−X)     │          │     (+X)
            │  Input2  │  (−Y face, Direction = Input)
            └──────────┘
                 −Y
```

| Component | Location (cm) | Rotation | Direction |
|---|---|---|---|
| Input1  | (−200,    0, 100) | facing −X | Input  |
| Output1 | ( 200,    0, 100) | facing +X | Output |
| Input2  | (   0, −200, 100) | facing −Y | Input  |
| Output2 | (   0,  200, 100) | facing +Y | Output |

Copy the exact offsets (face distance and belt height) from the vanilla splitter
Blueprint in the starter project; the numbers above are the shape, not calibrated.
Every connection component's forward (X) axis must point **out** of the building.
All four ports must be the same distance from the centre: flipping (below) turns
the building 90° on the belt and relies on Input2/Output2 landing where Input1/Output1 were.

## Placing on a belt: flipping the sides

`ASplergerHologram` (Blueprint child `Holo_Splerger`, a copy of the vanilla
`Holo_ConveyorAttachment`) adds one thing to the vanilla hologram: while snapped onto
a belt, rotating toggles between

- **normal**: belt runs Input1 → Output1, side input on the left of the belt, side output on the right (Unreal: +Y is right);
- **flipped**: building turned 90°, belt runs Input2 → Output2, side input on the right, side output on the left.

The turn is measured from where the hologram's Input1 and Input2 components are
(the angle that moves Input2 onto Input1), not hardcoded, and logged once per hologram.

The state is `mScrollRotation` (0 or 90), which the vanilla hologram already sends to
the server. When flipped, `ConfigureComponents` points the hologram's static
pass-through names at Input2/Output2 for the duration of the vanilla call.

As a safety net (and to repair 1.0.0 saves), `ABuildableSplerger::FixUpPortConnections`
runs on BeginPlay and the next tick: a belt connection is moved to the port the belt
end physically sits on, and a loose belt end sitting on a free port is connected.

## Behaviour

Same model as the vanilla attachments in game 1.x: the buffer inventory
(`mBufferInventory`) has one slot per output, and each output connection serves belts
straight from its slot (`SetInventory` + `SetInventoryAccessIndex`).

- **Merge**: inputs take turns. Only the input whose turn it is may top up a slot that
  already holds items; the other input may only fill an empty slot. (Otherwise the
  input matching a slot's item keeps it topped up and the other input never gets in.)
- **Demand first**: an item goes to an output with an **empty** slot first. An output
  whose slot is empty is ready for more, so a faster belt gets more items.
- **Mix**: among those, the item goes to the output that most needs that item type.
  For each output the building keeps recent counts per item type (halved once the
  total passes 100). An output's fair share of an item type is that item's total
  times the output's share of all items. Matching the overall mix instead of taking
  turns means the order items arrive in can't line up with the outputs and sort them
  (a copper/iron/copper/iron belt into two alternating outputs is perfectly sorted).
- **Tolerance**: an output more than 1 item over its fair share of a type is skipped
  for that type while another output is still taking items. That item waits for the
  other output instead of going here.
- **Overflow**: an output is backed up when it isn't connected, or its slot holds items
  its belt hasn't taken for 0.5 s. A full slot alone is not backed up (a fast belt's
  1-item slot is full most of the time, and counting that as backed up switched the
  mix protection off). Backed-up outputs hold nothing up, so a blocked output sends
  everything to the other one.
- **Slot size**: each output's slot holds what its belt takes in about two ticks,
  from its smoothed drain rate: 1 for slow belts, up to 3 for a fast belt at a low
  frame rate. Committing 3 items to a slow belt ahead of time made its mix drift.
- 1.0.0 history: one shared output turn locked in step with the input turns, so
  two different items each went to one output; the first fix (wait for your
  turn's output) let a slow belt set the pace for both.
- Tuned with `scripts/routing_sim.py` (an offline model of belts as rates) and
  stress-tested with `scripts/stress_test.py`: 32 scenarios with mixed input belts
  at different ratios, random and patterned orders, 2 to 10 item types, rare items,
  bursty and switching inputs, a stalled output. From 20 ticks/s: 98–100% of the
  best possible throughput, each output's mix within about 2.5 points of the overall
  mix, no runs of one item longer than the inputs already have.
- Known limit: a slot holds one item type, so with mixed items an output gets about
  one item per tick. Below 20 ticks/s that caps a mixed mk6 output: ~95% with 2
  types, ~80% (about 900–1000/min) with many types at 15 ticks/s. Slower belts and
  single-item flows are unaffected.
- Known edge case: a strictly repeating pattern (Cu Cu Cu Fe + Fe Fe Qz) into mk5 + mk1
  at exactly 20 ticks/s lines up with the mk1 taking one item a second: mk5 at 92%,
  mk1's mix 8.5 points off. Random mixes and other tick rates are fine.
- Rejected: holding an item back only for an output that could take it right now.
  It fixes that edge case but lets fast outputs take everything from slow ones
  (37.5 points off at 60 ticks/s) and brings back the lock-in on bursty inputs.
- **Why up to 3**: each output slot holds up to 3 items (one item type at a time).
  With 1, an output could pass at most one item per factory tick, which caps a
  belt below Mk6 (1200/min) under 20 ticks/s. With 3 the cap is ~3 items per tick
  (1800/min per output even at 10 ticks/s). Trade-off: up to 3 items are committed
  to an output early; on a jam they wait instead of rerouting.
- An input whose front item can't go anywhere (both slots hold a different item)
  is skipped that tick, so on mixed belts the other input keeps flowing.
- Net effect: up to 2 belts' worth in, split evenly across 2 belts out. Throughput
  is capped by the belts, not the building.

Code: `Mod/Splerger/Source/Splerger/{Public,Private}/BuildableSplerger.*`.

## Cost and unlock

- Recipe: 2 Iron Plate, 2 Cable, 2 Iron Rod (splitter + merger costs combined).
- Unlocked by a Tier 2 milestone (same tier as the vanilla Logistics milestone).

## Later ideas

- Mode switch (interaction UI): split-only / merge-only / priority input.
- Own icons instead of the vanilla splitter's.
