# DIY-Wrist-Watch
### 3D-Printed Arduino OLED-Clock with small RTC

New small OLED-Clock with **8-pin** RTC-chip DS3231**MZ** (RTC = Real-Time Chip) and the commonly known Arduino ATmega328-chip/uController, in conjunction with an OLED-Display (SSD1306-chip) - displaying Date, Time, (Battery)-Voltage and Skin-Temperature.

Both, Micro-Processor and OLED are switched-on at Button-press, and Off/disconnected from Battery **per hardware** (Schmitt-Trigger Transistor-circuit) after about 8 seconds, so that the only remaining comsumtion comes from RTC-chip in stand-by-mode, consuming less than 1uA.  
I resigned to use the uControllers software-"power-down" function, because this does not disconnect the OLED-display(chip) and other chips on board from Battery, which are causing additional power-consumtions.  
Current-Leakages are minimized through the double-mosfet FDC6237 (small Gate-Capacitances and Gate Threshold Voltages with 1-2V) and two double-diodes (BAS40-05W) with very small leakage-currents of 20nA.  
With this configuration the CR2032-Lithium-Battery lasts more than 2 years (proved! - using the clock-display about 10 times/day).

The Clock additionally provides a calendar, displaying the actual month(days) and a relatively bright Flashlight-LED, consuming only about 10mA.

Two Buttons are handling Display-"On", FlashLight-On/Off, Calendar and Set/Reset of the Clock, meanwhile Display-"Off" is done automatically after 8 seconds.
I commented every line of the software, so it is possible to modify the whole code (at own "risk").  
The "OLED-Watch-6-nl-Manual.txt" file prives a short user-manual list.  

The Software code is Arduino-based and is flashed through an ISP-Programmer to the ATmega328-Chip - without bootloader, to save start-time.  
To flash the chip I use the known **avrdude** command-line-program, flashing the Arduino-compiled resulting hex-file with an ISP-Programmer (USBasp), this connected on one side through USB-Port to my Notebook and on the other through a 6-pole ISP-Connector-Socket to the Clock-board.

Case, Lid and battery-Holder are 3D-Printed with 0.13mm Layer-height and 100% Infill (PLA: 205°C / 60°C). I provided .stl-files of case, lid and battery-holder, also the corresponding .gcode-files for a 220x220mm 3D-Printer (Hot-)bed, as beeing my Anycubic-Pro, also usable for similar printers like Prusa-MK3, etc.

- Sliced with Prusa-Slicer - my profile-file: "prusaslicer_config.ini" - you may load it with the Hotkey [Ctr+L]  

- Layout: Eagle 7.4
- Prototype etched with 0.8mm FR4-board with Toner-Direct methode (2 layers: wax-paper fixed/congruented with 0.4mm needles/wire-clamps on the 4 outer-edges)
- CAM-files for JLCPCB FR4-board 0.8mm ("OLED-Clock-6_JLCPCB_CAM.zip").
