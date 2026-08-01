# Bambu Studio 2.8.1.55 — plain text logs

This is the official Bambu Studio **2.8.1.55** release, unmodified, with a single
line changed so that local log files are written as plain text.

Nothing else is different. No features added, no behaviour changed, no telemetry
removed beyond the one request described below. The diff against the upstream release
tag is one source file.

## What this is not

This is an **unofficial build**. It is not produced, endorsed or supported by Bambu Lab.
Do not report problems with it to Bambu support — reproduce them on an official build
first. Bambu Studio is AGPL-3.0; this fork carries the same licence.

It also does not remove telemetry or network activity in general. One request disappears
as a side effect (the log encryption key fetch); everything else the application does is
unchanged.

Note that `debug_network_*.log.enc` stays encrypted. That log is written by the
closed-source `libbambu_networking` plugin, not by the application's own log sink, so
this change does not reach it. Application logs — `studio_*.log` — are plain text.

## Why

Upstream encrypts every local log with AES-256-CBC. The key is fetched at startup from
`v1/analysis-st/tag/` on Bambu's API, is different for every session, and is never
written to disk — so a log produced before you started capturing your own network
traffic cannot be decrypted by the owner of the machine at all.

This has been raised as [bambulab/BambuStudio#10087](https://github.com/bambulab/BambuStudio/issues/10087),
open since March 2026 and labelled a bug. The vendor's documented way to read your own
crash log is to open a support ticket.

## The change

One line at the end of `s_get_log_enc_opts()` in `src/slic3r/GUI/GUI_App.cpp`:

```cpp
enc_options.enc_type = LogEncOptions::LOG_ENC_NONE;
```

`LOG_ENC_NONE` is upstream's own enum value: `LogSinkBackend::consume()` already has a
plaintext path for it. Because the key is only requested on the encrypted path, this
also stops the unauthenticated key request being made at every launch.

To go back to upstream behaviour, delete that line.

## Verifying a build

Start the application, then look at the newest file in `~/.config/BambuStudio/log/`:

- it is readable text
- it has no `BEGIN_HEADER` block — that header is only written on the encrypted path
- the filename has no `_enc` suffix

## Building

```bash
./BuildLinux.sh -d      # dependencies, first time only, takes a while
./BuildLinux.sh -s      # the application
./build/src/bambu-studio
```

Built and verified on Linux with `BBL_RELEASE_TO_PUBLIC=1`, i.e. the same configuration
as a public release build.
