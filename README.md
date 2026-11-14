# Splerger

A Satisfactory mod adding a hybrid splitter / merger: **2 belt inputs, 2 belt outputs**.
Design and port layout: [docs/DESIGN.md](docs/DESIGN.md).

## What's in this folder

```
Splerger/
├── Mod/Splerger/                 ← the mod plugin (linked into the SML project's Mods/)
│   ├── Splerger.uplugin          mod manifest (ModReference = "Splerger", needs SML ^3.11)
│   ├── Source/Splerger/          C++ module
│   │   ├── Splerger.Build.cs
│   │   ├── Public/BuildableSplerger.h      the 2-in / 2-out building class
│   │   └── Private/BuildableSplerger.cpp
│   └── Content/README.md         list of the editor assets still to create
├── Art/reference/                vanilla splitter / merger icons (from FModel exports)
├── docs/DESIGN.md                ports, behaviour, cost, unlock
└── scripts/link_mod.ps1          links Mod/Splerger into the SML project
```

## The process

Satisfactory mods are Unreal Engine plugins built against **SML** (Satisfactory Mod
Loader) in Coffee Stain's custom Unreal Engine. Authoritative guide:
<https://docs.ficsit.app> → *Development → Beginner's Guide*. Versions below were
current at the time of writing; check the docs for the current ones.

### 1. Install the toolchain (one-off, ~100 GB, a few hours)

1. **Visual Studio 2022** (Community is fine) with the workloads *Game development with
   C++* and *.NET desktop development*, plus the MSVC/Windows SDK versions the docs list.
   (None is installed on this machine yet.)
2. **Unreal Engine – CSS** (Coffee Stain's fork, 5.3.2-CSS): link your Epic account to
   GitHub, join the Epic Games GitHub org, then download the installer from the
   `satisfactorymodding/UnrealEngine` releases.
3. **Wwise** (Audiokinetic launcher) — the version the docs list, integrated into the
   starter project.
4. **Git**, already installed.

### 2. Get the starter project

```bash
git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git C:/Modding/SatisfactoryModLoader
```

Check out the branch matching your game version, run the Wwise integration step,
then link this mod in:

```bash
powershell -ExecutionPolicy Bypass -File scripts/link_mod.ps1 -SmlProject "C:\Modding\SatisfactoryModLoader"
```

Right-click `FactoryGame.uproject` → *Generate Visual Studio project files*, open the
`.sln`, build **Development Editor / Win64**, then open the project in Unreal.

### 3. Compile the C++ class

The first build is where `BuildableSplerger` gets checked against the real headers
(`Source/FactoryGame/Public/Buildables/FGBuildableConveyorAttachment.h`,
`FGFactoryConnectionComponent.h`, `FGInventoryComponent.h`). It was written from those
APIs but hasn't been compiled yet, so expect to fix a signature or two.

### 4. Create the assets in the editor

Follow [Mod/Splerger/Content/README.md](Mod/Splerger/Content/README.md): the building
Blueprint (4 connection components), descriptor, recipe, schematic, and the
`RootGameWorld_Splerger` module that registers the schematic. Easiest start: open the
vanilla splitter's Blueprint in the starter project and copy its mesh, clearance and
connection-component offsets.

### 5. Package and test

1. In Unreal, open **Alpakit** (SML toolbar button), set the game path to
   `F:\SteamLibrary\steamapps\common\Satisfactory`, tick Splerger, *Alpakit Dev*.
   It builds the mod and copies it into the game's `FactoryGame/Mods/` folder.
2. Install SML itself into the game (Satisfactory Mod Manager is easiest).
3. Start a new save, unlock the milestone, place a Splerger on a belt and check:
   two inputs merge, two outputs split evenly, one blocked output sends everything
   to the other.

### 6. Release (optional)

*Alpakit Release* makes a zip; upload it to <https://ficsit.app> (SMR) so it shows up
in the mod manager.
