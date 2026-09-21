# Combat Observation

Milestone 2 collects evidence for later attack-speed, cancellation, and buffering
work. It does not modify combat. The observer uses CommonLib event sources, not
executable patches, replacement input handlers, or hardcoded runtime offsets.

## Reference Setup

Inspected on 2026-09-21. Package metadata, DLL file versions, and SKSE plugin
versions are different identifiers; none should be substituted for another.

| Component | Installed evidence |
| --- | --- |
| Skyrim SE runtime | 1.6.1170.0, confirmed in the plugin startup log |
| SKSE64 | 2.2.6, confirmed in `skse64.log` |
| Attack MCO-DXP | Package metadata 1.6.0.6; includes its own conditional animation sets |
| MCO Universal Support | Archive label 1.0; `MCO.dll` file version 1.0.0.0; SKSE reports plugin version `01000000` and successful loading |
| Dodge MCO-DXP | Package metadata 2.1.21.0; includes a bundled Dodge Framework DLL |
| Dodge Framework | Separate enabled package; DLL file version 1.0.1.0; SKSE reports `01000010` and successful loading |
| One Click Power Attack NG | Archive label NG v1.11; DLL file version 1.0.0; SKSE reports `01000000` and successful loading |

The separate Dodge Framework entry has higher priority than the bundled copy in
the active MO2 profile. Its DLL differs from the bundled file. MCO Universal
Support supplies the active profile's only `MCO.dll`.

Attack MCO's installed animation conditions include unarmed and weapon-specific
sets. The exact winning animation files for the first test weapon still need to
be confirmed in-game; enabled package names alone do not establish the selected
moveset. Record the weapon, camera, active moveset name/version, and any relevant
animation overrides alongside each test. Do not treat the observations as general
MCO/DMCO compatibility until this is known.

## Relevant Existing Settings

The installed OCPA configuration defaults include `iKeycode=257`,
`bOnlyDuringAttack=1`, `bDisableBlockDuringAttack=1`, `bQueuePowerAttack=1`, and
`fQueueExpire=0.2`. These can affect how a block request or a queued power attack
is interpreted. No persisted `MCM/Settings/OCPA.ini` override was found in the
enabled loose-file providers or MO2 overwrite folder during inspection; MCM or
save-specific state still needs to be checked in-game.

The Dodge Framework defaults use the sprint button with a 0.25-second hold
threshold. Confirm the effective MCM bindings before testing. The observer logs
raw device/key codes and the input system's event label rather than guessing
whether a particular key means dodge or power attack.

OCPA's installed configuration references `MCO_WinOpen`, `MCO_WinClose`,
`MCO_PowerWinOpen`, and `MCO_PowerWinClose`. These are candidate window events to
look for, not assumptions built into the observer. All received player animation
tags are recorded, including unrecognized names, within the rate limit.

## Enable Recording

1. Build and deploy this branch using `scripts/build.ps1 -Configuration release -Deploy`.
2. In the installed `SKSE/Plugins/ResponsiveCombat.ini`, keep `Enabled=true` and
   `LogLevel=info`, and add or update this section:

```ini
[Diagnostics]
TraceCombat=true
```

3. Restart Skyrim through SKSE and load a disposable test save. Recording starts
   on a new game or successful save load, not while sitting at the title screen.
4. Check `ResponsiveCombat.log` for `Combat observation enabled`, a session
   `begin`, and `animation-graph available=true`. Missing graph output is a
   diagnostic failure, not evidence that an attack produced no animation event.
5. After testing, set `TraceCombat=false` and restart. Existing installed INIs are
   preserved by deployment, so an older INI will need this section added manually.

No keyboard text, mouse motion, or repeated held-button events are recorded.
Gameplay button press/release records include unmapped custom bindings. Input
recording is suppressed while the game is paused, relevant menus are open, text
entry is active, player 3D is absent, or the player is dead. A release suppressed
by a menu transition may therefore have no matching input record; menu records
mark that boundary. Do not interpret it as a stuck input.

## Trace Format

Every `[observe]` record includes `session`, `seq`, `t_ms`, and `source`.
Sequence numbers describe recorder order across callback threads, not a guarantee
of the engine's internal ordering. Elapsed timestamps use a monotonic clock and
are clamped against late callbacks to avoid going backwards in that order.

| Source | Recorded evidence |
| --- | --- |
| `input` | Press/release, raw device and button code, event label, held duration |
| `animation` | Player graph event tag and payload, copied without modifying the graph |
| `state` | Changed native attack/life/weapon states, equipped form IDs, blocking, sprinting, sneaking, camera, pause status |
| `action` | Player SKSE action type, slot, source form ID; type 0 is weapon swing |
| `hit` | Incoming/outgoing player hit notification, source/projectile, power/bash/blocked flags; not measured damage |
| `menu` | Menu opening/closing; opening the main menu ends the session |
| `lifecycle` | New/load boundaries, graph availability, player death, suppression counts |

Native melee state labels are `none`, `draw`, `swing`, `hit`, `next-attack`,
`follow-through`, and `bash`. These describe the engine's sampled enum, not a
verified phase model for the active moveset. Snapshot checks occur on input
dispatch at most every 50 ms, so brief states can be missed. Animation events and
hit notifications complement snapshots; neither alone proves every contact
window or damage application.

Graph registration retries once per second on input dispatch and is idempotent.
It covers the graphs exposed by the player's current graph manager. Graph
replacement or camera transitions can cause observation gaps until the next
check; first-person coverage must be verified rather than assumed. No raw actor
or event pointers are queued for later use.

Each source is limited to 120 records per one-second window, so animation spam
does not use the input or hit budget. Suppression counts appear at the next trace
activity/input tick or session end. Event text is capped at 96 bytes and control
characters are sanitized. Existing log rotation remains active: capture short
runs and retain relevant rotated logs locally before another launch replaces
them. With observation enabled, output is buffered and flushed about once per
second on input dispatch, at session boundaries, and on warnings/errors.

## Capture Checklist

Use one weapon/moveset first, initially in third person. Leave a few seconds of
idle time between cases and note their order. Keep the test under two minutes.

1. Draw the weapon, perform three spaced normal attacks, then one held attack.
2. Perform three spaced power attacks with the actual OCPA binding, then one combo.
3. Request block early and late during an attack, then release block.
4. Request dodge early and late during an attack. Repeat with insufficient stamina.
5. Press attack rapidly during recovery, then stop pressing and note any late swing.
6. Repeat a normal attack and block/dodge requests in first person.
7. Open and close a menu during an attack, change equipment, and reload the save.
8. On a disposable save, test player death and reload. Confirm a new session ID
   and that observations resume without duplicate listeners or stale state.
9. Repeat a short case with `Enabled=false` and with `TraceCombat=false` after
   restarting. Startup logs should remain, with no `[observe]` records.

Confirm that enabling observation does not change movement, costs, damage,
blocking, dodging, combos, or perceived responsiveness. Logging overhead also
needs an in-game check. A rate-limit summary means the corresponding part of the
trace is incomplete; repeat a shorter, quieter test before drawing conclusions.

## Interpretation And Acceptance

Version 0.0.2 passes all 23 CTest cases in Debug and Release. Tests cover the
opt-in setting, session boundaries, rate limits and independent channel budgets,
suppression reporting, monotonic timestamps, bounded text, native state labels,
and concurrent recorder access, alongside the milestone 1 tests. The Release DLL
has been deployed with the installed INI unchanged. These native checks do not
verify in-game event coverage or the selected animation set.

### Verified Capture: 2026-09-21

The enabled run starts at 11:53:38 and contains 2,443 observation records across
two loaded-save sessions: 2,262 animation, 79 state, 66 menu, 17 action, 10 hit,
and 9 lifecycle records. Graph registration succeeds in both sessions. Player
death and the subsequent pre-load/new-session boundary are present. No warning,
error, critical, or rate-limit suppression records appear in this capture.

The later run starts at 12:00:18 with `TraceCombat=false` and contains only
startup/data-loaded messages, with no observation records. The disabled-tracing
check is verified.

One third-person attack sequence in session 1 provides these reference points:

| Elapsed time | Observed event |
| --- | --- |
| 25,124 ms | Native state `draw` |
| 25,134 ms | `MCO_AttackInitiate` |
| 25,329 ms | `MCO_InputBuffer` and `preHitFrame` |
| 25,512 ms | SKSE weapon-swing action |
| 25,585 ms | `HitFrame` |
| 25,657 ms | Outgoing hit notification |
| 25,975 ms | `MCO_WinOpen` and `MCO_PowerWinOpen` |
| 26,121 ms | Another `MCO_AttackInitiate` |

The full capture also includes `MCO_Recovery`, `attackStop`, native return to
`none`, blocking/bashing, and `MCO_DodgeInitiate`. These establish useful event
coverage, not yet a complete phase or cancellation model.

Known gaps carried forward:

- No `source=input` records were captured. Investigate input filtering and event
  delivery before correlating presses/releases with queued attacks.
- Many animation tags appear in same-timestamp pairs. Their source needs to be
  identified before treating them as independent events or deduplicating them.
- No `first_person=true` state or `power=true` hit record is present. First-person
  and successful power-hit coverage are not established by this file.
- Exact winning moveset identity, low-stamina dodge rejection, and the cause of
  late queued attacks cannot be concluded from the available evidence alone.

The observation baseline is accepted for the next milestone, with these limits
explicitly retained. Copies of the enabled and disabled captures are preserved
locally under ignored `build/verification/milestone2/`; raw logs are not committed.

### Further Interpretation

Before selecting gameplay integration points, use the capture and targeted
follow-up tests to establish:

- Windup: the attack-specific graph transition after input and before a swing.
- Contact: observed animation markers correlated with outgoing hit notifications.
- Recovery: the observed post-contact interval, combo window markers, and return
  to idle. Do not equate a combo window with cancellation eligibility.
- Interruptions: what actually follows accepted/rejected block and dodge requests,
  menu transitions, death, and loading.
- Queue behavior: whether a late attack follows a recent press, an OCPA queue,
  another input handler, or an animation transition. A trace alone cannot prove
  which plugin consumed an input before this observer received it.

Resolve the relevant evidence gaps before choosing integration points or claiming
input buffering/cancellation compatibility. No gameplay implementation is part
of this verification checkpoint. Keep logs, saves, and machine-specific
configuration out of source control.
