# Runtime Profile

The runtime profile service lets users remap buttons and assign auto-fire frequencies at runtime. Remappings are built on the fly using button combos on the controller itself and overlay on top of any active profile.

Source: `src/core/services/profiles/runtime_profile.c` and `runtime_profile.h`

## How It Works

The service watches all button events via `runtime_profile_check_combo()`, called from the app's input processing loop (the router wires a global instance for every app). It maintains an internal state machine:

| State | Description |
|-------|-------------|
| `RUNTIME_IDLE` | Normal operation; listening for entry combos |
| `RUNTIME_MAPPING` | Live remap mode (one input button per output, in order) |
| `RUNTIME_AUTOFIRE` | Rapid-fire mode (each press cycles a button's rate) |

Both modes are entered from idle by a deliberate **hold** (`hold_ms`, ~2 s). Neither
entry combo uses START, so nothing collides with console reset combos (PCE
START+SELECT, SNES START+SELECT+L+R). While the NeoPixel or controller LED is
indicating (blinking), button input is ignored to avoid accidental triggers.

| Entry combo | Mode |
|-------------|------|
| **SELECT + B3** (hold) | Rapid-fire set |
| **SELECT + B4** (hold) | Live remap |

## Rapid-Fire (RUNTIME_AUTOFIRE)

Cycles a repeating auto-fire rate on any button, live.

**To enter:** hold **SELECT + B3** for `hold_ms`. The LED blinks once; release the buttons.

**While in rapid-fire set:**
- **Tap a button** → advances *that* button's rate one step each press, cycling:

  `off → 30 → 20 → 15 → 12 → 10 → 7.5 Hz → off …`

- Each button tracks its own rate independently; the change takes effect immediately so you can feel it.
- Press **SELECT** → exit.

Rapid-fire overlays the current mapping without replacing it. If no runtime mapping exists yet, it seeds once from the active profile so rates apply on top of the existing remapping.

## Live Remap (RUNTIME_MAPPING)

Maps each input button to a fixed output, one at a time, in order.

**To enter:** hold **SELECT + B4** for `hold_ms`. The LED blinks once; release the buttons. The previous runtime map is cleared.

**While in remap mode:**
- Press an input button → assigns it to the next output slot (slot 1, 2, … in `output_buttons` order).
- Already-mapped buttons are silently rejected (duplicate input protection).
- Press **SELECT** → save and exit. Unfilled output slots and any input button that was never assigned are left disabled (produce no output).
- Press **START** → cancel and clear the mapping.

**Feedback:** 1 NeoPixel blink per entry confirmed; 2 blinks on save/exit.

## Duplicate Input Protection

In `RUNTIME_MAPPING`, a `uint32_t runtime_mapped_mask` bitmask accumulates every input button that has already been assigned. When a button press is detected, a single bitwise AND rejects it instantly if already mapped:

```c
if (input_btn & runtime_mapped_mask) {
    // skip — already assigned to another output slot
}
```

The mask is set in `map_entry()` and cleared in `runtime_profile_init()`, `runtime_profile_clear()`, and at the start of each new mapping session.

## Clearing the Runtime Mapping

Press **START** while in **Live Remap** mode to cancel and erase the mapping, returning to the active profile. Entering remap mode (SELECT + B4) also clears the previous runtime map before starting a new one.

Programmatically: `runtime_profile_clear()` resets all state. `runtime_autofire_clear()` removes only auto-fire assignments without touching the button remapping.


## Integrating in an App

Apps configure the service by providing a `runtime_profile_config_t` at init:

```c
static profile_t runtime_prof = { .name = "runtime" };

static const runtime_profile_output_config_t rt_out = {
    .profile             = &runtime_prof,
    .input_mask          = JP_BUTTON_B1 | JP_BUTTON_B2 | JP_BUTTON_B3 | JP_BUTTON_B4,
    .output_buttons      = { JP_BUTTON_B1, JP_BUTTON_B2, JP_BUTTON_B3, JP_BUTTON_B4 },
    .output_button_names = NULL,
    .output_button_count = 4,
    .hold_ms             = 3000,
};

static const runtime_profile_config_t rt_cfg = {
    .output_configs = {
        [OUTPUT_TARGET_...] = & rt_cfg,
    },
};

// In app_init():
runtime_profile_init(&rt_cfg);

// In ..._device_init();
runtime_profile_set_player_count_callback(get_player_count);

// In the input processing loop:
runtime_profile_check_combo(event->buttons,
                            event->analog[ANALOG_L2],
                            event->analog[ANALOG_R2]);

// In output aplly
const profile_t* profile = runtime_profile_get_active(OUTPUT_TARGET_...);
if (!profile) profile = profile_get_active(OUTPUT_TARGET_...);

```

`runtime_profile_is_active()` returns true while the device is in any mapping or auto-fire mode, which apps can use to suppress unrelated combos.

## See Also

- [Profiles](profiles.md) -- Compiled profile system that runtime mapping overlays
- [Buttons](buttons.md) -- JP_BUTTON_* constants used as input and output masks