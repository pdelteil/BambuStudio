![image](https://user-images.githubusercontent.com/106916061/179006347-497d24c0-9bd6-45b7-8c49-d5cc8ecfe5d7.png)
# About this fork

This is a fork of [Bambu Studio](https://github.com/bambulab/BambuStudio). It keeps
everything upstream does — the rest of this README is upstream's and still applies —
and adds the features below.

They are all slicer-side changes: new settings, new toolpath decisions and new UI. No
G-code post-processing and no wrapper scripts, so they behave like native options, are
saved in presets and survive re-slicing. [`MODS.md`](MODS.md) documents each one in
detail, including the edge cases and the compatibility caveats.

### Ironing

- **Iron internal top surfaces only** — a fourth ironing type that irons exactly what
  *Top surfaces* irons, except the final visible top of the print. Pockets, cavities,
  engravings and terraces get smoothed while the topmost face keeps its normal finish.
  A surface counts as the global top when it sits on the object's topmost layer, so it
  works with variable and adaptive layer height, multi-material and modifier meshes.
- **Skip ironing over a layer range** — `Skip ironing from layer` / `to layer`, an
  inclusive band that is never ironed whatever the ironing type is. Either end can be
  left open, and the settings live in the region config so they can be overridden per
  part, per modifier mesh and per height range.

### Reporting

- **Print settings card (PDF)** — *File → Export → Export print settings (PDF)*, also
  on the plate right-click menu. A single-page A4 traveler / QA card with the plate
  thumbnail, the presets in use, layer height, line widths, every ironing parameter,
  and print time and filament usage once the plate has been sliced.
- **Exclude timelapse time from the print estimate** — an option that subtracts the
  timelapse blocks from the reported time, applied both to the preview statistics and
  to the estimate written into the G-code header.

### In progress

- [`mods/wip-preview-arrange`](../../tree/mods/wip-preview-arrange) — a before/after
  slice comparison in the preview legend (per line type and totals, with the changed
  values highlighted), an *All* master checkbox for the Line Type legend, and an
  arrange option that keeps objects on the plates that already exist instead of
  spilling onto new ones. Not compiled yet.
- [`mods/offline-mode`](../../tree/mods/offline-mode) — keeps the application off the
  public internet: a host gate on every request it makes, a navigation veto on the
  embedded web views, and the startup update / telemetry / sync calls skipped. Partially
  working — the embedded browser's own in-page requests still get out.

Branch [`mods/consolidated`](../../tree/mods/consolidated) carries the finished
features listed above.

---

# BambuStudio
Bambu Studio is a cutting-edge, feature-rich slicing software.  
It contains project-based workflows, systematically optimized slicing algorithms, and an easy-to-use graphic interface, bringing users an incredibly smooth printing experience.

Prebuilt Windows, macOS 64-bit and Linux releases are available through the [github releases page](https://github.com/bambulab/BambuStudio/releases/).

Bambu Studio is based on [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community.

See the [wiki](https://github.com/bambulab/BambuStudio/wiki) and the [documentation directory](https://github.com/bambulab/BambuStudio/tree/master/doc) for more information.

# What are Bambu Studio's main features?
Key features are:
- Basic slicing features & GCode viewer
- Multiple plates management
- Remote control & monitoring
- Auto-arrange objects
- Auto-orient objects
- Hybrid/Tree/Normal support types, Customized support
- multi-material printing and rich painting tools
- multi-platform (Win/Mac/Linux) support
- Global/Object/Part level slicing parameters

Other major features are:
- Advanced cooling logic controlling fan speed and dynamic print speed
- Auto brim according to mechanical analysis
- Support arc path(G2/G3)
- Support STEP format
- Assembly & explosion view
- Flushing transition-filament into infill/object during filament change

# How to compile
Following platforms are currently supported to compile:
- Windows 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Windows-Compile-Guide)
- Mac 64-bit, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Mac-Compile-Guide)
- Linux, [Compile Guide](https://github.com/bambulab/BambuStudio/wiki/Linux-Compile-Guide)
  - currently we only provide linux appimages on [github releases](https://github.com/bambulab/BambuStudio/releases) for Ubuntu/Fedora, and a [flathub version](https://flathub.org/apps/com.bambulab.BambuStudio) can be used for all the linux platforms

# Report issue
You can add an issue to the [github tracker](https://github.com/bambulab/BambuStudio/issues) if **it isn't already present.**

# License
Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

The bambu networking plugin is based on non-free libraries. It is optional to the Bambu Studio and provides extended networking functionalities for users.
By default, after installing Bambu Studio without the networking plugin, you can initiate printing through the SD card after slicing is completed.

