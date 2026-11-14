# Splerger

A Satisfactory mod adding a hybrid splitter / merger: **2 belt inputs, 2 belt outputs**.
Design and port layout: [docs/DESIGN.md](docs/DESIGN.md).

## What's in this folder

```
Splerger/
├── Mod/Splerger/                 ← the mod plugin (linked into the SML project's Mods/)
│   ├── Splerger.uplugin          mod manifest (ModReference = "Splerger", needs SML ^3.12)
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

### 1. Install the toolchain (one-off, 30+ GB, a few hours)

Checked against the docs source (satisfactorymodding/Documentation) in Nov 2026:
game on **Unreal 5.6.1-CSS**, **SML 3.12.0**. The community tool
[SMEH](https://github.com/mklierman/SMEH) can automate most of these steps.

1. **Visual Studio 2022 Community** (not 2026; the docs say 2026 needs different
   components and breaks the editor's Create C++ Class wizard).
   Installer: <https://aka.ms/vs/17/release/vs_community.exe> (17.14 channel).
   - Workloads: *.NET desktop development*, *Desktop development with C++*,
     *Game development with C++*.
   - Individual components: *MSVC v143 – VS 2022 C++ x64/x86 build tools (v14.38-17.8)*
     (marked "Out of Support", still required), *.NET 8.0 Runtime (LTS)*,
     *.NET Framework 4.8.1 SDK*.
   - Or import the docs' `SML.vsconfig` (after a default 2022 install: More → Import configuration).
   - JetBrains Rider works as the editor, but VS 2022 + those components must still be installed.
2. **Unreal Engine – CSS 5.6.1** (Coffee Stain's fork + modding patches):
   1. Link GitHub to your Epic account (<https://www.unrealengine.com/en-US/ue-on-github>),
      accept the EpicGames org invite, and check you can see github.com/EpicGames/UnrealEngine.
   2. Link to the modding repo with <https://linker.ficsit.app/link>.
   3. From the latest release of `satisfactorymodding/UnrealEngine`, download
      `UnrealEngine-CSS-Editor-Win64.exe` and `-1.bin`, `-2.bin`, `-3.bin` into one folder
      (keep the names exact) and run the `.exe`.
3. **Wwise 2025.1.6.9117** via the Audiokinetic Launcher (needs a free account).
   Packages: Authoring, SDK (C++). Platforms: Linux; Windows → Visual Studio 2019,
   Visual Studio 2022, Game Core. No plugins.
4. **Git**: already installed (plus Git LFS).
5. Optional: **Satisfactory Mod Manager** (to install SML/test mods), and the
   **Clang v25 cross-compile toolchain** only if you want Linux dedicated-server builds.

### 2. Get the starter project

```bash
git clone https://github.com/satisfactorymodding/SatisfactoryModLoader.git C:/Modding/SatisfactoryModLoader
```

Check out the branch the docs currently suggest (`dev` at the time of writing), run the Wwise integration step,
then link this mod in:

```bash
powershell -ExecutionPolicy Bypass -File scripts/link_mod.ps1 -SmlProject "C:\Modding\SatisfactoryModLoader"
```

Right-click `FactoryGame.uproject` → *Generate Visual Studio project files*, open the
`.sln`, build **Development Editor / Win64**, then open the project in Unreal.

### 3. Compile the C++ class

`BuildableSplerger` follows the 1.x conveyor-attachment model and was checked against
the starter project's headers (`Source/FactoryGame/Public/Buildables/FGBuildableConveyorAttachment.h`,
`FGFactoryConnectionComponent.h`, `FGInventoryComponent.h`, SML dev / CL517554), but the
first real compile is still the proof. Build **Development Editor / Win64** by
right-clicking *Games/FactoryGame → Build* (not *Build Solution*).

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
