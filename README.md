# DG-30 "DecuGuard" — Project 2 Starter

SWEN 563 / CMPE 663 / EEEE 663 — Real-Time & Embedded Systems, Fall 2026
Target: **STM32WB5MM-DK** · **STM32CubeMX-generated HAL project** (CubeMX 6.17.0,
STM32Cube FW_WB V1.24.0, CMake + Ninja + VS Code — the same toolchain as P0/P1).

This is the starter for the in-mattress pressure-injury monitor
([`../RTES_Project2_Spec.md`](../RTES_Project2_Spec.md)).
It is your first HAL-era project.
The device configuration lives in **`P2_PatientVitals.ioc`**, and CubeMX **generates** the initialization code.
The sensors operate through the **ST component drivers**.
Your own IO-layer code (*you* write it) connects those drivers to the generated HAL handles.

## Build / flash / debug

The procedure is the same as P0/P1.
Open the folder in VS Code.
Accept the STM32Cube project configuration.
Select **Debug** from the CMake presets.
Build the project.
Press F5 to start the debugger.
Command line:

```
cmake --preset Debug && cmake --build --preset Debug
STM32_Programmer_CLI -c port=SWD mode=UR -w build/Debug/P2_PatientVitals.elf -v -rst
```

## What the smoke test does (flash it first)

- OLED banner + console banner (115200 8N1). Typed characters echo.

- **I2C3 bus scan**: `STTS22H @0x38 OK  ISM330DHCX @0x6B OK` on the console.
  This output shows that the sensors answer electrically before you write driver code.

- **Raw touch counts** on the OLED (~10 Hz): when you touch the TS1 pad, the number drops.
  This is your presence-gate raw signal (spec R1).

- **B1/B2 press counters** through the HAL EXTI callback — the latch-in-ISR,
  act-in-loop pattern that you must keep (R17/R23).
  They show the latch-in-ISR, act-in-loop pattern, and you must keep this pattern (R17/R23).

- One line to replace first: `vitals_bus_init: NOT IMPLEMENTED`.

## The driver architecture (read this before coding)

```
   your app (App/app.c → your DG-30 state machine)
        │ component API: STTS22H_TEMP_GetTemperature(), ISM330DHCX_ACC_GetAxes()
   ST component drivers          Drivers/Components/{stts22h,ism330dhcx}
        │ five function pointers (IO struct) — YOU write these (App/vitals_bus.c)
   HAL transactions              HAL_I2C_Mem_Read/Write(&hi2c3, …)
        │
   CubeMX-generated init         Core/ (MX_I2C3_Init owns the bus setup)
```

- The **component drivers are bus-agnostic**: no HAL includes, and no board knowledge.
  They operate through the `STTS22H_IO_t` / `ISM330DHCX_IO_t` function pointers.
  Your ~50-line `App/vitals_bus.c` connects them to `hi2c3`.

- **CubeMX owns the initialization, and your code owns the behavior.**
  The `Init` callback of the IO layer is a no-op, because `MX_I2C3_Init()` ran before it.
  No code initializes the bus two times.

- **The starter does not use the full ST board BSP** (`Drivers/BSP/STM32WB5MM-DK` in the Cube repo).
  The BSP bus layer makes its *own* `hbus_i2c3` handle and force-resets the peripheral.
  That opposes the CubeMX-generated code, which R24 tells you to keep.
  Also, the BSP `MX_I2C3_Init` symbol collides with the generated one.
  The component-plus-IO-layer pattern is how vendor drivers connect to generated code in practice.
  The point of P2 is to see that seam.

- The starter **provides** the OLED text driver (`App/oled.c`) and the console (`App/console.c`).
  They are HAL ports of the P0/P1 drivers that you know.
  Read them: the same SSD1315 command stream, and the same printf retarget, one abstraction level up.

- Three more **provided modules** ship with this starter.
  Spec §6 tells you about each one.
  All three build as shipped, and no starter code calls them until your code calls them:

  - `App/qualify.c/.h` — a generic threshold + sustain + hysteresis qualifier.
    The spec lets you use it for HOB-HIGH (R11) and TEMP-RISE (R14) *after* you build the pattern
    by hand for the repositioning clock.
    The spec explicitly **bars it from the posture-change detector (R9)**.
    The candidate-based reset of R9 is the D6 graded subtlety.

  - `App/ui_pages.c/.h` — the LIVE/CLOCKS/SESSION page registry, labeled-row helpers, and
    change-only redraw bookkeeping (the R18 discipline, packaged).
    The page contents and the banner that shows the alerts are yours.

  - `App/config_table.c/.h` — all the §7.3 settings, one table row for each, with generic
    `SET`/`SHOW` handlers.
    The non-blocking line assembly (R20) and the logged `EVENT` echo (R19) are yours.

## Where your code goes

| File | Yours? | Purpose |
|---|---|---|
| `App/vitals_bus.c` | **YOU** | IO layer + WHO_AM_I POST + sensor enable (R3) |
| `App/app.c` | **YOU** (replace the smoke test) | the DG-30 state machine (spec §3/§4) |
| `App/oled.c` / `App/console.c` | provided | display + console (do not modify) |
| `App/qualify.c/.h` | provided | threshold+sustain+hysteresis qualifier — connect R11/R14 to it, **never R9** |
| `App/ui_pages.c/.h` | provided template | page registry + change-only redraw. You write the page contents + alert banner |
| `App/config_table.c/.h` | provided | settings table + `SET`/`SHOW`. You write the non-blocking integration + R19 echo |
| `Core/`, `cmake/stm32cubemx/` | generated | CubeMX territory — user code only in `USER CODE` sections (R24) |
| `CMakeLists.txt` (top level) | yours | add all new .c files here (CubeMX never rewrites it) |
| `P2_PatientVitals.ioc` | instructor-configured | regenerate from it freely. Demo D1 proves you can |

**Regeneration rule (R24, demo D1):**
The next CubeMX generation destroys anything you write in `Core/` that is not in a `USER CODE BEGIN/END` fence.
Keep your application code in `App/`.
Change `Core/Src/main.c` only in its fences.
The starter connects `app_init()` / `app_service()` there.
Read that code.

## The .ioc, as configured (spec §6 table)

| Peripheral | Config | Notes |
|---|---|---|
| I2C3 | PB13 SCL / PB11 SDA, 100 kHz | STTS22H (0x38) + ISM330DHCX (0x6B) |
| TSC | Group 6 key (PD10 cap / PD11 electrode), Group 4 shield (PC6/PC7) | TS1 presence pad |
| SPI1 | 8-bit, 8 MHz, CPOL=High, software NSS | OLED (PH0 CS, PC9 D/C, PC8 RST) |
| USART1 | 115200 8N1 async | ST-LINK VCP console |
| EXTI | PD2 = IMU INT1 · PD9 = INT2 · PC12/PC13 = B1/B2 (falling, pull-up) | NVIC enabled |
| SysTick | 1 ms HAL tick | the one timebase (R19 timestamps) |

Watchdogs, radio, USB, QSPI, timers, ADC: the `.ioc` deliberately does not have them.
This project teaches sensors, not all the peripherals.
(To see what "initialize all peripherals" adds, compare this .ioc against a clean board template
with `git diff`.)

## Rehearsing the demo

All the waits in the demo sheet are CONFIG settings.
The **rehearsal note** in spec §7.3 sanctions the full demo sheet at SET-compressed values
(turn interval 20 s, re-arm 10 s, …).
A full rehearsal thus takes minutes.
Demo day uses the printed defaults at full duration.

## Datasheets & references

`Collateral/05`: STTS22H (DS12606), ISM330DHCX (DS13012) — the register maps for your IO layer
and your POST.
`Collateral/03`: UM1718 (CubeMX), UM2442 (WBxx HAL API).
The component headers (`Drivers/Components/*/**.h`) are the API documentation.

## Licenses

`Drivers/Components/*` are STMicroelectronics BSD-3-Clause components (LICENSE.txt stays in each
folder).
The standard ST terms apply to the generated `Core/` and `Drivers/` code and to the ST HAL.
Course-authored files (`App/oled.c`, `App/console.c`, skeletons) are for use in this course.
