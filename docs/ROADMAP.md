# Responsive Combat: Incremental Development Plan

## Goal and boundaries

Make player combat feel responsive through configurable baseline attack animation
speed, attack-to-block and attack-to-dodge cancellation, and control over unwanted
queued attacks. Start with the existing NGVO/MCO/DMCO test setup and expand
compatibility only after verifying it.

Responsive Combat owns baseline responsiveness. Level Scaled Actions owns
level-based progression, with a neutral 1.0 multiplier at level 1. Either mod must
work independently. When both are installed, the intended composition is:

`combined attack-speed factor = baseline factor * progression factor`

The implementation must preserve contributions from the game, perks, equipment,
and other mods. Do not implement this by overwriting a shared value every frame.
Select a compatible integration point after observing the installed combat stack.
Perk requirements belong in Perks Beyond 80 and are outside this mod's scope.

## Verified starting point

- The native plugin builds in Debug and Release using `scripts/build.ps1`.
- The console message has been verified in Skyrim: plugin
  discovery, loading, and the data-loaded callback work in the current setup.
- Gameplay changes and automated tests have not been implemented yet.
- Normal script builds do not deploy. Use `-Deploy` explicitly for in-game tests
  through the existing MO2 setup and a disposable test save.

## Delivery sequence

Each milestone gets a branch from the latest accepted `main`. Build it, verify its
acceptance criteria, then merge before starting the next milestone. Later branch
names are proposed; only milestone 1's branch is created now.

### 1. Configuration and persistent logging

Branch: `configuration-and-logging`

- Add `ResponsiveCombat.log` through SKSE's resolved log directory. Record plugin
  version, runtime version, initialization, configuration results, and data load.
- Keep the working console message. Handle logging or configuration failures
  without crashing Skyrim, with a useful fallback diagnostic where available.
- Introduce a versioned INI configuration and a documented example under
  `config/`. Load it from `Data/SKSE/Plugins/ResponsiveCombat.ini` via the game's
  normal virtual filesystem. Use a maintained parser compatible with the project.
- Start with an enabled flag and log verbosity. Add gameplay settings with their
  implementing milestones so settings do not promise inactive functionality.
- Validate values, use documented defaults for missing or invalid entries, and
  log invalid entries once. Do not rewrite the installed configuration on startup.
- Extract configuration validation from SKSE calls and add focused automated
  tests for defaults, malformed input, and invalid values. Extend the build
  workflow to run those tests and fail on test failures.
- Support loading configuration at startup first; live reload and an MCM can be
  considered after the basic behavior works.

Acceptance: Debug and Release builds and configuration tests pass; an in-game
launch writes a readable startup log and preserves the console message. Missing
or invalid configuration has predictable results. Combat behavior is unchanged.

### 2. Observe combat input and animation state

Proposed branch: `combat-state-observation`

- Record the actual Skyrim, SKSE, MCO, DMCO, power-attack input, and relevant
  moveset versions in the test setup. Verify installed versions before making
  compatibility claims.
- Observe player input actions and attack/animation events without changing them.
  Trace press/release, attack start, hit timing, recovery, and completion where
  the installed stack exposes them.
- Map windup, active attack, recovery, and idle states; account for interruption,
  menu transitions, death, and loading a save. Rate-limit diagnostic output.
- Use these observations to choose supported APIs/hooks. Do not assume event
  names or hardcode runtime offsets from another MCO version.

Acceptance: A reproducible trace explains normal attacks, power attacks, block,
dodge, and unwanted queued attacks in the current setup without altering combat.

### 3. Configurable baseline attack speed

Proposed branch: `baseline-attack-speed`

- Implement a bounded, configurable player attack-speed factor. Start testing at
  1.0; compare modest boosts before selecting a release default.
- Define ownership and composition with Level Scaled Actions so both mods work
  alone or together and disabling one removes only its contribution.
- Verify animation, hit timing, movement, and recovery stay coherent. Do not
  accidentally accelerate unrelated animations or NPC behavior.
- Test factor validation/composition outside Skyrim, then test in-game weapon
  types, normal attacks, power attacks, camera modes, and equipment changes.

Acceptance: 1.0 reproduces the baseline; boosts are measurable and controllable;
save loading and repeated attacks do not compound the multiplier.

### 4. Attack-to-block cancellation

Proposed branch: `block-cancel`

- Implement a player-only, configurable cancellation policy using the observed
  state transitions. Prototype immediate cancellation where the stack supports
  it, then expose restrictions only when needed for correctness or tuning.
- On a valid block request, stop the attack and enter block cleanly. Invalidate
  stale queued attacks and clean up state without swallowing unrelated input.
- Respect equipment and game-state eligibility, and test block release, rapid
  repeated input, power attacks, and the installed power-attack input mod.

Acceptance: Valid block input interrupts attacks consistently, with no stuck
blocking, unintended follow-up attacks, or attack damage after cancellation.

### 5. Attack-to-dodge cancellation

Proposed branch: `dodge-cancel`

- Integrate with the actual installed dodge system and its acceptance rules.
  Confirm a dodge is accepted before abandoning the attack where feasible.
- Preserve the dodge system's stamina costs, cooldowns, invulnerability rules,
  distance, and direction handling.
- Handle rejected dodges, low stamina, repeated inputs, menus, and interruptions.

Acceptance: Accepted dodges interrupt attacks predictably; rejected dodges do
not leave the player stuck. No extra dodge or stale attack fires afterward.

### 6. Deliberate attack input buffering

Proposed branch: `attack-input-buffer`

- Implement a configurable short buffer tied to the observed attack phases.
  Early button presses must not produce unwanted late attacks, while deliberate
  combos remain possible. Tune the acceptance window from actual input traces.
- Define normal versus power-attack handling, press versus hold semantics,
  expiry, and priority when block and dodge inputs arrive together.
- Clear queued actions on cancellation and invalid game-state transitions.
- Unit-test the state/queue rules, including rapid presses and timeout boundaries.

Acceptance: Early spam does not cause delayed extra swings; intentional combos
work; block/dodge cancellation reliably removes stale buffered attacks.

### 7. Compatibility, tuning, and first release

Proposed branch: `first-release-validation`

- Test each feature alone and in combination, including all features disabled.
- Cover weapon types, camera modes, keyboard/mouse and controller where
  supported, normal/power attacks, stamina, death, menus, and save/reload.
- Test against a minimal supported setup and the NGVO test profile; document
  exact tested versions, conflicts, unsupported cases, and reproduction steps.
- Tune defaults through playtesting, measure logging/per-frame overhead, and ensure
  release logging is bounded.
- Package the DLL, example configuration, license, installation/removal notes,
  and a short smoke-test checklist for Nexus distribution.

Acceptance: A repeatable test matrix passes and the package can be installed,
configured, disabled, and removed without leaving persistent unintended changes.

## Current branch scope

This branch currently records the roadmap only. Milestone 1 implementation is the
next development task. The prior console-load test is complete and does not need
to be repeated as a new prerequisite; regression checks still accompany new DLLs.
