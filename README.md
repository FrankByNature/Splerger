# Splerger

A Satisfactory mod that adds the **Splerger**: a conveyor attachment that is a splitter
and a merger in one, with **two belt inputs and two belt outputs**.

Both inputs are merged and the combined flow is split evenly across both outputs, all in
the same 4 m × 4 m footprint as the vanilla splitter and merger.

## Features

- **Two in, two out.** One input and one output run straight through, so it snaps onto
  the middle of a belt like a vanilla splitter. The other input and output are on the
  sides.
- **Even merging.** Items are taken from the two inputs in turn.
- **Even splitting.** Items go to the two outputs in turn.
- **Overflow.** If one output is backed up or not connected, everything goes to the
  other output.
- **Mixed belts keep moving.** If an item from one input can't go anywhere yet, that
  input waits while the other one keeps flowing.
- **Full belt speed.** Throughput is limited by your belts, not by the Splerger
  (tested at 1200/min in and out).

## Unlock and cost

| | |
|---|---|
| Unlocked by | Splerger milestone, Tier 2 |
| Build cost | 2 Iron Plate, 2 Cable, 2 Iron Rod |
| Build menu | Logistics → Conveyor Attachments |

## Requirements

- Satisfactory 1.x
- [Satisfactory Mod Loader (SML)](https://ficsit.app/mod/SML) 3.12 or newer

## Installing

Install it with [Satisfactory Mod Manager](https://smm.ficsit.app/), which also installs
SML for you.

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md). How the building works internally is described
in [docs/DESIGN.md](docs/DESIGN.md).
