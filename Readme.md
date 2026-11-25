# Ventbox – humidity-extraction module for a 1.9 L food container

**Group 2** (VGU) · rev 1.2 (English release, 08 Nov 2026) · licence CERN-OHL-S v2

Ventbox is a module that mounts on the original lid of a 1.9 L Lock & Lock HPL818 food container (the lid only needs two holes, Ø12.3 and Ø10.3). It extracts moist air from the container to limit condensation while the container is kept in the refrigerator.
The module protrudes 8.0 mm above the lid surface and measures 92.8 × 43.3 × 7.5 mm. It is controlled by a Seeed XIAO nRF52840 with an SHT40 sensor; Sunon UB5U3-700 centrifugal blower fan; LP303035 LiPo battery.

Since rev 1.1 there is also a **freshness timer**: hold the button for 2 s when storing food, and the RGB LED on the module cover flashes green for 2 days, orange for 3 days, red for 2 days, then red double flashes once 7 days have passed. The timer runs independently of the fan and shares the XIAO, battery and button ([firmware/README.md](firmware/README.md)).

![Ventbox 3D](mechanical/renders/Ventbox_3D_on_container.png)

## Repo contents per the Milestone 3 checklist

| Checklist item | Content | Folder |
|---|---|---|
| 3. Native CAD + STEP | Parametric native CAD (CadQuery/Python), STEP AP214 + AP242 assembly, STEP per part, IGES; since rev 1.1: 10 2D drawings VB-00…VB-09 (PDF/DXF) and STL files for 3D printing | [`mechanical/`](mechanical/README.md) |
| 4. Electronics (KiCad) + firmware | KiCad 9 project: schematic, PCB, netlist, gerbers, drill files, component position file, ERC/DRC reports; Arduino firmware with build instructions and a compiled .hex file | [`electronics/`](electronics/README.md), [`firmware/`](firmware/README.md) |
| 5. Bill of materials | BOM with part numbers, suppliers, unit prices, quantities and purchase links (CSV + XLSX) | [`bom/`](bom/README.md) |
| 6. Datasheets | PDF datasheets of every purchased component, with a part-number lookup table | [`datasheets/`](datasheets/README.md) |

## Folder tree

```
docs/assembly_guide/  9-step illustrated assembly guide (ASSEMBLY_GUIDE.md)
mechanical/
  cad_native/      parametric CAD source (CadQuery 2.8) + render and interference-check scripts
  step/            STEP AP214/AP242 assembly + parts/*.step, *.igs
  renders/         3D images, layout drawing, exploded view
  drawings/        2D drawings VB-00..VB-09: pdf/ dxf/ svg/ png/ + combined PDF
  stl/             STL files of the 3D-printed parts (base, module cover, 2 nuts, button, light pipe)
electronics/
  kicad/           ventbox.kicad_pro / .kicad_sch / .kicad_pcb / .kicad_sym / .net, ERC + DRC
  kicad/fab/       gerbers + drill, component positions, PCB STEP, PCB layer PDF, render images
  tools/           scripts that generate the schematic and PCB (fully reproducible)
firmware/
  ventbox/         ventbox.ino
  build/           compiled .hex and DFU .zip package
bom/               BOM.xlsx, BOM.csv, make_bom.py
datasheets/        11 PDF datasheets
```

## Verification status (08 Nov 2026)

| Item | Result |
|---|---|
| 3D model | Rev 1.1: housing split into module base + module cover, modelled threads, watertight STL; all solids valid, no interference between parts (`chk_addon.py`); the lid only loses 303 mm³, exactly the volume of the 2 holes |
| Schematic | ERC: 0 errors, 0 warnings |
| PCB | Rev 1.2 (rev 1.1 added RGB LED D2, R3–R5, TP7–TP9): DRC 0 errors, 0 unconnected pads; the 17 `lib_footprint_mismatch` warnings are caused by intentionally moving footprint silkscreen to the Fab layer (the 16 × 16 mm board is too small) |
| Firmware | v1.2 (freshness timer since v1.1) compiles successfully for `Seeeduino:nrf52:xiaonRF52840`: 52 660 bytes (6 % flash) |
| Tested on real hardware | Not yet. When a prototype is available, re-measure the XIAO height, check the fan fit, and check the LED brightness and orange colour through the light pipe |

## Third-party software

The firmware `firmware/ventbox/ventbox.ino` is original Group 2 code. It uses these standard open-source libraries (not copied into the repo; installed via the Arduino Library/Boards Manager):

- Wire (I2C), part of the [Seeed nRF52 Arduino core](https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino) – Arduino LLC / Sandeep Mistry / Adafruit, LGPL-2.1
- [Adafruit SHT4x Library](https://github.com/adafruit/Adafruit_SHT4x) – Adafruit Industries, BSD
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) – Adafruit Industries, MIT
- [Adafruit Unified Sensor](https://github.com/adafruit/Adafruit_Sensor) – Adafruit Industries, Apache-2.0
- [Seeed nRF52 Boards core 1.1.13](https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino) incl. [Adafruit TinyUSB](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) – Seeed Studio / Adafruit, LGPL-2.1 / MIT

Versions, purpose and licence of each library: [firmware/README.md](firmware/README.md#third-party-libraries-and-origin).

## Licence

Hardware, documentation and firmware are released under the CERN Open Hardware Licence Version 2 – Strongly Reciprocal (CERN-OHL-S v2), the course default (KickOff p. 49 #10). The LICENSE file is added by the person responsible for checklist item 12.
