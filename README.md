# samp-recon

A tiny `.asi` mod for SA-MP that adds a `/recon` command: disconnect and rejoin the
current server without closing the game.

- **Works on:** SA-MP 0.3.7 R1 only. On any other client version the mod loads but stays inactive.
- **Needs:** an ASI loader (most SA-MP installs with CLEO already have one). No SAMPFUNCS or CLEO required.

## Usage

| Command | What it does |
| --- | --- |
| `/recon` | Rejoin the same server after 3 seconds |
| `/recon 10` | Rejoin after a custom delay in seconds (0-120) |

Use a longer delay if the server rejects quick rejoins.

## Install

Copy `recon.asi` into your GTA San Andreas folder (next to `gta_sa.exe`).

## Build

Requires [Zig](https://ziglang.org/) (used only as a C cross-compiler):

```
build.bat
```

or directly:

```
zig cc -target x86-windows-gnu -O2 -shared -s -o recon.asi recon.c -lkernel32 -luser32
```

## How it works

A background thread waits for `samp.dll`, checks that it is 0.3.7 R1, and registers
`recon` in SA-MP's own chat command list. When the command runs, it calls the client's
own restart routine, tells the server it disconnected, and after the delay sets the
client state back to "waiting to connect" so SA-MP reconnects by itself.

## License

MIT
