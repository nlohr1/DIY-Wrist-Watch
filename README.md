# DIY-Wrist-Watch
### Small 3D-Printed OLED-Clock with RTC and ATmega328-chip, based on Arduino-code

Small OLED-Clock with the small **8-pin** Real-Time chip DS3231**MZ** and the commonly known (Arduino) ATmega328 uController chip,
in conjunction with a 128x64px OLED-Display (SSD1306-chip) - displaying Date, Time, (Battery)-Voltage and Skin-Temperature.

----
**OLED-Clock Case & Foto:**  
<img src="https://github.com/nlohr1/DIY-Wrist-Watch/blob/main/OLED-Clock-6-nl_Case.png" width="500"> <img src="https://github.com/nlohr1/DIY-Wrist-Watch/blob/main/OLED-Clock-v6_details.jpg" width="300">  

**Schematic:**  
<img src="https://github.com/nlohr1/DIY-Wrist-Watch/blob/main/OLED-Clock-6-nl-sch.png">  

**PCB-Layout:**  
<img src="https://github.com/nlohr1/DIY-Wrist-Watch/blob/main/OLED-Clock-6-nl_brd.png" width="800">  

----
### Partlist (BOM) for "OLED-Clock-6-nl.sch":  
<b><code>
Qty.  Components      Value          Description                                        Package</code></b><code>
1x     C1              2,2uF          Capacitor, SMD                                     C0603
3x     C2,C3,C4        1uF            Capacitor, SMD                                     C0603
5x     C5,C6,C7,C8,C9  100n           Capacitor, SMD                                     C0603
2x     D1,D2           BAS40-05(W)    Dual-Schottky-Diode, low-leakage (1uA), SMD        SOT23 (-323-R)
1x     IC1             ATMEGA328P     8-bit Microcontroller (4x8 = 32 Pin), SMD          TQFP32-08
1x     IC2             DS3231MZ       RTC Real-Time Clock (8-Pin), SMD                   SO08
1x     JP1             AVR-ISP-6      AVR ISP-6 Serial Programming Header                AVR-ISP-6
1x     LED1            cool-white     Osram-LED-Duris-E3, 2.240mcd, 3V/30mA, SMD-3014    Chipled 1206
1x     LED2            blue           High-Efficiency LED, 2.5V-forward-voltage, SMD     Chipled 0805
1x     MD1             OLED-Display   I2C OLED-Module 0.96", (SSD1306 chip), SMD         128x64px
1x     R1              10k            Resistor, SMD                                      R0603
1x     R2              33R            Resistor, SMD                                      R0603
4x     R3,R4,R5,R6     100k           Resistor, SMD                                      R0603
2x     R7,R8           1k             Resistor, SMD                                      R0603
2x     S1,S2           Push-Button    Micro Tactile Switch 12V, 50mA, SMD                3.7x6x2.5mm
1x     T1              FDC6327C       Dual N+P-Channel Mosfet, SMD                       SOT23-6
14x    Socket          1x40 Pin       Female Socket-header, short (L:7.0mm, hole:0.6mm)  RM:2,54mm
1x     Board           FR4 #0.8mm     Mainboard, 2-layer (Cu 35um), #0.8mm               27.3x26.9mm
1x     Case            PLA or PETG    3D printed, 0.13mm layer-height, 100% Infill       31x36x10mm
1x     Lid             PLA or PETG    Bottom-Lid, case-latching                          29x28x2mm
1x     Strap           Inox           20mm Quick Release Milanese Mesh Watch Band        width: 20mm
2x     Wire            Inox or Steel  Inox or Galvanized Steel-Wires ø1.2mm              L: 29mm
2x     Bushing         Inox or Steel  Inox- or Brass-Bushing, ø1.8/1.5 mm                L: 20mm
</code>   

----
Both, Micro-Processor and OLED are switched-on at Button-press, and off/disconnected from Battery **per hardware** (Schmitt-Trigger Transistor-circuit) after about 8 seconds, so that the only remaining comsumtion comes from RTC-chip (in stand-by-mode), consuming less than 1uA.  

I resigned to use the uControllers software-"power-down"-function, because this does not disconnect the OLED-display(chip) and other chips on board from Battery, which are causing additional power-consumtions.  
Current-Leakages are minimized through the double-mosfet FDC6237 (small Gate-Capacitances and Gate Threshold Voltages with 1-2V) and two double-diodes (BAS40-05W) with very small leakage-currents of 20nA.  
With this configuration the CR2032-Lithium-Battery lasts more than 2 years (proved! - using the clock-display about 10 times/day).

The Clock additionally provides a calendar, displaying the actual month(days) and a relatively bright Flashlight-LED, consuming only about 10mA.

Two Buttons are handling the Display-"On", Calendar, FlashLight-On/Off and Set/Reset of the Clock, meanwhile Display-"Off" is done automatically after 8 seconds.
The code is subdivided in several sections, with jump-markers: §0, §1, §2... explained in the code-head.  
I commented every line of the software, so it's possible to understand and modify it (at your own "risk")...  
The "OLED-Watch-6-nl-Manual.txt" file provides a short user-manual list.  

The Software code (Arduino-based C++) is flashed through an ISP-Programmer to the ATmega328-Chip - without bootloader, to save start-time.  
To flash the chip I use the known **avrdude** command-line-program, flashing the Arduino-compiled resulting hex-file with an ISP-Programmer (USBasp), this connected on one side through USB-Port to my Notebook and on the other through a 6-pole ISP-Connector-Socket to the Clock-board.  
Avrdude-Commands to flash the code into the ATmeag328-chip are explained on the end of the code-file:  

> **"OLED-Clock-w-Led-and-Calendar.ino"** *(open this file with a Text-Editor like Notepadd++)* or directly with the Arduino-GUI.

Case, Lid and battery-Holder are 3D-Printed with 0.13mm Layer-height and 100% Infill (PLA: 210°C / 60°C). I provided .stl-files of case, lid and battery-holder, also the corresponding .gcode-files for a 220x220mm 3D-Printer (Hot-)bed, as beeing my Anycubic-Pro, also usable for similar printers like Prusa-MK3, etc.

- Sliced with Prusa-Slicer - my profile-file: "prusaslicer_config.ini" - you may load it with the Hotkey [Ctr+L]  

- PCB schematic and Layout: Eagle 7.4
- Prototype etched with 0.8mm FR4-board with Toner-Direct methode (2 layers: first CNC-drilled the FR-material, then transferred the layout with toner-transfer-paper: 2 layers fixed/congruented with 0.4mm needles/wire-clamps through FR4-board on the 4 outer-edges), the FR4-board blank/fine-sanded + cleaned with acetone, toner-laminated with a 200°C (modified laminator), etched in sodium persulfate @ about 80°C ( <5min ) + finally hot-tinned (both sides) with a flat soldering iron.
- CAM-files for ![JLCPCB](https://jlcpcb.com/) &nbsp;⇒&nbsp; **"OLED-Clock-6_JLCPCB_CAM.zip"** (0.8mm FR4-board with PCB-Assenbly).

-eof-
