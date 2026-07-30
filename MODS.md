# Fork modifications

Local features added on top of upstream `bambulab/BambuStudio`. Everything here
is a slicer-side change: no G-code post-processing, no external scripts.

| Feature | Settings | Where |
| --- | --- | --- |
| [Exclude timelapse time from the print estimate](#exclude-timelapse-time-from-the-print-estimate) | `exclude_timelapse_from_estimate` | Others → Timelapse |
| [Iron internal top surfaces only](#iron-internal-top-surfaces-only) | `ironing_type = internal top` | Quality → Ironing |
| [Skip ironing over a layer range](#skip-ironing-over-a-layer-range) | `ironing_skip_layer_start`, `ironing_skip_layer_end` | Quality → Ironing |
| [One-page PDF print settings card](#one-page-pdf-print-settings-card) | — | File → Export, plate context menu |

An [offline mode](#not-merged-offline-mode) exists on a separate branch and is
deliberately **not** part of this one.

---

## Exclude timelapse time from the print estimate

`exclude_timelapse_from_estimate` (bool, default off) subtracts the duration of
the timelapse blocks from the reported print time.

The timelapse time comes from `skippable_part_time[stTimelapse]`, which is
accumulated across all enabled time modes, so it is spread evenly over them to
avoid over-subtracting when more than one mode is enabled. The subtraction is
applied both to the preview statistics (`update_estimated_times_stats`) and to
the estimate written into the G-code header (`TimeProcessor::post_process`).

---

## Iron internal top surfaces only

A fourth entry in **Ironing type**, between *Top surfaces* and *Topmost
surface*:

```
No ironing / Top surfaces / Iron internal top surfaces only / Topmost surface / All solid layer
```

It irons exactly what *Top surfaces* irons, **except the final visible top of
the print**. The topmost face keeps its normal top-surface finish, while every
top surface that only exists because of a pocket, cavity, engraving, embossed
region or terrace is smoothed.

```
layer 30   visible top face      -> not ironed
layer 27   floor of a cavity     -> ironed
layer 18   floor of an engraving -> ironed
```

### How a surface is classified

A top surface belongs to the *global top* when it lies on the object's topmost
layer, i.e. at the maximum printable Z. Every top surface below that has
printed layers above it somewhere else in the object, which is what makes it an
internal one.

In `Layer::make_ironing()` the test is `layer->upper_layer == nullptr`. The
region is then never added to `by_extruder`, so its `stTop` polygons are never
collected into `ironing_areas` and no toolpath is produced — nothing is
generated and later filtered out.

### Scope and compatibility

* **Per object.** On a plate holding objects of different heights, each keeps
  its own visible top unironed.
* **Within one object, only the tallest part's top is skipped.** A shorter
  tower's top face has printed layers above it elsewhere in the object, so by
  the rule above it counts as internal and is ironed.
* Variable and adaptive layer height: `upper_layer` is re-established after
  empty top layers are trimmed (`PrintObject::slice`), so the test holds
  without special-casing.
* Multi-material, multi-part objects and modifier meshes: the ironing type is
  read per `LayerRegion`, so per-region overrides keep working.
* Support interfaces are unaffected — support ironing is a separate code path
  (`SupportParameters`, `TreeSupport.cpp`).
* All ironing parameters (speed, spacing, flow, angle, inset, pattern) are read
  after the gate and are untouched.

### Preset compatibility

The new value is inserted mid-enum with the key `internal top`. Enum options
serialize by key, so presets and 3MF files holding `topmost` or `solid`
continue to load correctly.

---

## Skip ironing over a layer range

Two settings in **Quality → Ironing** that suppress ironing on a band of
layers, whatever the ironing type is set to:

| Setting | Label | Meaning |
| --- | --- | --- |
| `ironing_skip_layer_start` | Skip ironing from layer | first layer of the skipped range |
| `ironing_skip_layer_end` | Skip ironing to layer | last layer of the range, inclusive |

Layer 1 is the **bottom layer of the object**, matching the preview slider;
`Layer::id()` counts raft layers first, so the raft count is subtracted.

`0` means unset:

| start | end | Result |
| --- | --- | --- |
| 0 | 0 | iron everywhere (default, previous behaviour) |
| 12 | 18 | skip layers 12–18 |
| 12 | 0 | skip layer 12 up to the top of the print |
| 0 | 18 | skip layers 1–18 |
| 18 | 12 | ignored — the alternative would silently disable ironing entirely |

The check sits next to the ironing-type gate in `Layer::make_ironing()`, so
skipped layers produce no ironing toolpaths at all.

Both options live in `PrintRegionConfig`, so they can be overridden per part,
per modifier mesh and per height range, and changing them invalidates the
ironing step through the existing `PrintRegionConfig` diff.

**Caveat:** these are layer numbers, not heights. If you change layer height or
enable adaptive layers, the same range points at a different band of the model.
Use a height-range modifier if you need a band fixed in millimetres.

---

## One-page PDF print settings card

**Export print settings (PDF)** in File → Export and in the plate right-click
menu writes a single-page A4 traveler / QA card for the current plate: the
plate thumbnail, the selected printer / process / filament presets, key
settings (layer height, variable layer height range, ironing, wall loops,
infill density and pattern) and, once the plate has been sliced, print time and
filament usage.

`PrintCardUtils` holds a `PrintCardData` struct of pre-formatted strings plus
`build_print_card_pdf()`, so the PDF renderer stays independent of the slicer
config types. Fields that are unavailable — results on an unsliced plate, for
example — render as `-`.

---

## Not merged: offline mode

Branch `mods/offline-mode` keeps the application off the public internet: a
host gate in `Slic3r::Http` that rejects anything outside loopback / RFC1918 /
link-local / `*.local`, a `wxEVT_WEBVIEW_NAVIGATING` veto on every embedded web
view, and the startup version / privacy / certificate / preset-sync calls
skipped. The closed-source networking plugin stays loaded so LAN-mode printer
control keeps working; `BAMBU_ALLOW_INTERNET=1` lifts the gate for one run.

It is **under test and not merged**: it does not yet behave as intended in
practice. One trap already found and fixed on that branch is worth recording,
because it will bite any similar change: `LogSinkBackend::consume` holds a
non-recursive `std::mutex`, and log setup itself performs an HTTP request while
holding it. Logging from anywhere inside `Http::priv::http_perform()` therefore
deadlocks the whole application during `instance_check()`, before the GUI or
even the log file exists. `LogSink.cpp` carries four *"do not use
BOOST_LOG_TRIVIAL in this function to avoid deadlock"* warnings for the same
reason.

---

## Building this fork

```bash
cd build && ninja -j 6 bambu-studio
cd build/src && LC_ALL=C ./bambu-studio      # the build/bambu-studio wrapper needs the packaging step
```

Notes worth knowing before starting a build:

* Touching `libslic3r/PrintConfig.hpp` or `libslic3r/Config.hpp` invalidates
  both precompiled headers (`libslic3r/pchheader.hpp`,
  `slic3r/pchheader.hpp`) and rebuilds nearly everything — roughly 600 ninja
  steps, tens of minutes. Most other changes are a handful of files.
* Parallelism is limited by memory, not cores: the heavy GUI translation units
  peak above 3 GB each. On a 30 GB desktop `-j 6` stays resident while `-j 8`
  spills into swap and slows to a crawl.
* Ubuntu's `systemd-oomd` kills the heaviest cgroup under a user slice once
  memory pressure holds above 50 % for 20 s, which lands on the build scope.
  Raise `ManagedOOMMemoryPressureLimit` for `user@.service`, or mask
  `systemd-oomd.socket` *and* `systemd-oomd.service`, otherwise builds die with
  a bare `Killed`.
* Stop any running `bambu-studio` before the final link, or it fails with
  `Text file busy`.
