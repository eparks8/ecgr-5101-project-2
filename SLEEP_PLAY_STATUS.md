# Sleep/Play Status and Integration Notes (Aravind)

Last updated: Oct 14, 2026

## Branches

| Branch | Contents |
|---|---|
| `aravind-sleep-play` | Working branch: Sleep/Play/LDR module source, F091RC bench test, notes |
| `aravind-integration-test` | `jon-features` + Sleep/Play registered in `menu.c` and `CMakeLists.txt`. **Builds cleanly; not yet tested on the L496.** Flash this one to test. |

Build result (`aravind-integration-test`): 42/42 steps, no errors or warnings. FLASH 35,440 B (3.4%), RAM 3,936 B (1.5%).

## Pins used

| Feature | Pin | Header | Notes |
|---|---|---|---|
| Play button | PF13 | Zio D7 (CN10 pin 2) | Input, internal pull-up, active low |
| Sleep LDR | PC3 | Zio A2 (CN9 pin 5) | ADC2 channel 4 (approved by @JonHemrick) |

Checked against: GFX01M2 display/joystick pins, Jon's pot (PA3/A0, ADC1), Karann's mic on `karann-work` (PF15 power, PF3 out). No conflicts found.

## Wiring

- **Play button:** one leg to D7, the diagonally opposite leg to GND. No resistor, no 3V3.
- **LDR divider:** 3V3 → LDR → A2 (PC3) → 10 kΩ → GND. Bright = high reading, dark = low.
- Never connect a pin to 3V3 and GND at the same time.

## How the actions work

**Play:** select PLAY, press the button. The pet bounces 3 times (+15 happiness, +5 hunger). LEFT cancels; no press within 8 s = no change. Refused if too hungry (same rule as Fetch).

**Sleep:** select SLEEP, cover the LDR. The pet sleeps with a "z Z Z" animation and slowly gains happiness while dark. Uncovering (or LEFT) wakes it and returns to the menu. Still light after 10 s = "NOT SLEEPY", no change.

Both are nonblocking (`HAL_GetTick()` timing) and use only `PetArt_Move()` to draw the pet.

## Rules for whoever merges

- `PetAnalog_Init()` must run **after** `MX_ADC1_Init()`, because ADC1 and ADC2 share a clock register. `sleep.c` already does this by initialising lazily on first use, so `main.c` needs no change.
- If Karann's mic reads PF3 on ADC3, it shares the same clock register too. Keep `MX_ADC1_Init()` first. *(Unverified - depends on how `mic.c` reads it.)*
- Final integration needs `jon-features`, `AntBranch`, `karann-work`, and Sleep/Play together.

## Status

| Item | Status |
|---|---|
| Pot pin (PA3/A0) | Confirmed by @JonHemrick |
| LDR on PC3/ADC2 | Approved by @JonHemrick |
| Sleeping face (`PET_MOOD_SLEEPY`) | Waiting on @eparks8 - using `PET_MOOD_NEUTRAL` for now |
| What sleep recovers | Waiting on @eparks8 - placeholder: +2 happiness every 2 s |
| Final integration branch | Not decided |
| Hardware test on L496 | Not done - need the board |
