# Bambu Studio 2.8.1.55 — plain text logs

This is the official Bambu Studio **2.8.1.55** release, unmodified, with a single
line changed so that local log files are written as plain text.

Nothing else is different. No features added, no behaviour changed, no telemetry
removed beyond the one request described below. The diff against the upstream release
tag is one source file.

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
