# Integrating Myles and Krishy Games

This document outlines the plan to integrate the Waves game (Myles) and the Blast Gauge game (Krishy) into a single, unified module powered by a Finite State Machine (FSM), while also preparing for cross-board UART communication for strike counts.

## User Review Required
> [!IMPORTANT]
> **Pin Conflicts:** Currently, the TFT display used by Myles shares pins with the WS2812 LEDs and Servo used by Krishy (GP14 and GP15). We must move one set of pins. I propose re-assigning Krishy's pins:
> - **WS2812_PIN**: move from 14 to `16`
> - **SERVO_PIN**: move from 15 to `13`
> Please confirm if these pin changes are acceptable based on your hardware wiring.

> [!WARNING]
> **UART Pins:** We need two pins for UART communication to the other board to share strikes. I propose using UART1 on GP8 (TX) and GP9 (RX). Let me know if you would prefer different pins.

## Proposed Changes

We will create a new directory `krishy_myles_integrated` to host the combined code, to preserve the standalone variants if needed, or we can just merge them into a single `integrated` directory and wire up CMake. We'll use the latter approach by creating a new `CMakeLists.txt` entry and copying the sources.

### Core Structure & CMake
- Create a new folder `integrated_mk`.
- Configure `CMakeLists.txt` to compile both `myles/` and `krishy/blast_gauge/` source files along with a new `main.c`.
- Update the top-level `CMakeLists.txt` to build `integrated_mk`.

### Game Modules

#### [NEW] `integrated_mk/blast_gauge.c` & `integrated_mk/blast_gauge.h`
- Extracted from `krishy/blast_gauge/src/blast_gauge.c`.
- Remove the standalone `main()` function.
- Expose initialization and polling via:
  - `void blast_gauge_init(void);`
  - `void blast_gauge_update(void);` // The FSM update
  - `bool blast_gauge_is_complete(void);` // To check if the module was defused
  - `int blast_gauge_get_strikes(void);` 

#### [MODIFY] `integrated_mk/game.c` (Waves game logic)
- Extracted from `myles/`, exposing the core loops to be tick-based rather than infinite-loop based.
- We will tweak the wave simulation to only calculate and render when it is the active FSM state.

### FSM and Top-Level Main

#### [NEW] `integrated_mk/main.c`
This file will contain the overarching FSM that controls which game is currently playing.
**FSM States:**
- `STATE_IDLE_WAIT`: Waits for a player action.
  - If Blast Gauge Button is pressed -> Transition to `STATE_PLAY_BLAST`.
  - If Waves Potentiometers are moved significantly -> Transition to `STATE_PLAY_WAVES`.
- `STATE_PLAY_BLAST_GAUGE`: Calls `blast_gauge_update()`. If solved, transitions back to IDLE or WIN. Returns strikes to UART.
- `STATE_PLAY_WAVES`: Calls `game_update()` for Waves. If solved, transitions back to IDLE or WIN. Returns strikes to UART.
- `STATE_WIN_DEFUSED`: When both modules are solved.

#### [NEW] `integrated_mk/uart_comm.c` & `integrated_mk/uart_comm.h`
- A utility file to handle sending and receiving `"$STRIKE\n"` over UART so boards can synchronize the number of mistakes made across all games.

## Open Questions
- **Waves Game Auto-Start:** You mentioned the Waves game automatically starts when moving the potentiometers. Since they're analog inputs, they'll always have some jitter. We'll implement a threshold (e.g. +/- 10%) from the baseline to trigger the start. Does this sound good?
- **Gameplay Flow:** Once a game starts (e.g. Waves), can the player pause and switch to the other game, or must they finish/defuse the active game before moving to the other?
- **Strike System:** Should a strike reset the current module, or just record the mistake globally and let the user continue?

## Verification Plan

### Automated Tests
- Validate code compilation for the new `integrated_mk` target.
- Use `pio` or `cmake/make` to build the firmware successfully without standard library or pin conflict errors.

### Manual Verification
- Flash the `.uf2` file onto the RP2350.
- Verify that pressing the blast gauge button starts the gauge.
- Verify that substantially rotating the pot starts the Wave game.
- Verify that serial output (or UART TX) outputs `$STRIKE` correctly upon a mistake.
