# mod-autotarget

An aim-driven targeting system for the WoW 3.3.5a (WotLK) client used by
AzerothCore-based realms. Inspired by modern Blizzard Action Targeting.

The mod pins the client's **mouseover** slot to whatever attackable enemy is
directly in front of your character every frame. Cast spells with
`/cast [target=mouseover] ...` macros and they go where you're aiming — no
tab, no click, no lost GCDs on a dead or stale target.

The 3.3.5a client has no such mechanic, and one cannot be added server-side
(`UNIT_FIELD_TARGET` is a display-only broadcast field; no server→client
opcode sets a target). AutoTarget is therefore implemented client-side as a
proxy DLL.

---

## Layout

- **[`client/`](client/)** — the deliverable. A proxy DLL for the 3.3.5a
  client. Build, install, configuration, and architecture documentation lives
  in [`client/README.md`](client/README.md).
- **[`server-module/`](server-module/)** — scaffolded AzerothCore companion
  module. Future scope only (per-realm enable / config push via addon
  messages); the client DLL works standalone on any realm.

---

## Status

`v0.1.0` — first public release. See [`CHANGELOG.md`](CHANGELOG.md).

---

## License

MIT. See [`LICENSE`](LICENSE).
