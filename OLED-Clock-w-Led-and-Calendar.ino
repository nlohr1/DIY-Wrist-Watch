//Original @ http://www.instructables.com/id/DS3231-OLED-clock-with-2-button-menu-setting-and-t
//modified by nlohr on 2022, adding a Timer, an Alarm, a 30-day-Calendar showing actual Month, and a FlashLight-LED
/*
//See flashing-notes @ the end!

Principle:
The uController (chip: AtMega328P) takes Time+Date from RTC (chip: DS3231MZ) and displays computed values on the OLED (chip: SSD1306)

Processing:
-----------
Chip-Preparation, Clock-Display, Calendar-Display and two Settings-Displays.
Explanation:
  1. Beginning: After pressing D8-Button (Hardware-Power-On + uC-StartUp), the uController (+ OLED) are connected to Battery and it reads-out its FLASH-memory data:
  2. Libraries, OLED- & RTC-Chip-settings + (global-)variables,
  3. after Setup (1-time-pass), within endless repeating Main-Loop, all data + values are processed and drawn to the OLED, displaying data about 8s, after that,
  4. the Battery is disconnected and only the RTC-chip remains in "low-power" mode, consuming only ~1uA (so a normal CR2032-Battery holds about 2 years - tested!)

The Software here is structured into 9 parts:
---------------------------------------------
§0 Initial-Begin: Initializing Chips, #including libraries and #definig settings and global variables

§1 At Setup (1-time-pass) the 3 chips are initiated (uC, RTC and OLED) and stored values from RTC and EEPROM are read into variables in SRAM:
   RTC (Time+Date), EEPROM (year), (RTC-)Skin-Temperature and Voltage of Battery (CR2032)

§2 Main Loop: runs repeatedly each 0.1s, drawing the choosen display-mode on OLED (see MODES below).
~~~~~~~~~~~~~
  $2: call "clockDisplay()" function: (outsourced call from 2 places in main-loop)
  §2a: prepare loop-timing and read-out RTC: Date/Time and Temperature,
  §2b: compute Data/Time and Temperature,
  §2c: read-out & compute Battery-Voltage, (depending on LED2: adjust the LEDs forward-voltage value, mostly about 2.6 - 2.8 Volt for white or blue LEDs)
  §2d: draw Digital- & Analog-Clock, Batt.-Voltage + RTC-Temperature with previously collected values.

§3 Calendar-Display (mode-1): (pressing Button-D6 shortly toggles betw. Calendar-Display ⇔ Clock-Display)
   Pressing D6 twice (mode-2) rolls-out to default mode-0 = Clock-Display

§4 SETTINGS (modes 3-9): "setClock"-Display, increasing each item with D8 or setting it and going-ahead/-out with D6 (mode-10):
   ⇒ Time- & Date-Settings-Display (mode-3) to set Clock-date/time values. After reaching end-mode-9 (=leaving "setClock") the Display returns to default mode-0, while storing (all) values to RTC and uC-EEPROM.
   ⇒ In all SETTING-modes each D6-Button-press renews the "TOL"-time (TimeOut-Long = 20s), until rolling-out automatically to default mode-0 = "TOS" (TimeOut-short = 8s) ≈ 25s total
   ⇒ In all SETTING-modes pressing Button-D6 a bit longer (0.5s) terminates and stores all SETTINGS, rolling-out to mode-0 and "TOS" (8s more till shutdown).

§5 BUTTON-processing and SWITCH-Processing per "flags" (Buttons D6, D7 and D8: D6 = Select-Button, D8 = Change-Button, D7 = Soft-Reset-Button).
   If Input-pin D7 is connected (manually) with GND, Software resets all values to zero/initial state, including uC, RTC and uC-EEPROM.

§6 FlashLight: FlashLight-LED On/Off: if Button-D8 is pressed, the FlashLight (Pin-D3) toggles on/off (but only in "mode-0")

§7 TimeOut-processing per flag-switch: if (TOL_switch == 1), then TimeOut-long is activated (20s + 8s), else if (TOL_switch == 0), then it switches directly to TimeOut-short (8s).
~~~~~~~~~~~~~ (End of Main-Loop)

§8 FUNCTIONS (outsourced): Function-calls and Interrupt-Routines invoked from Main-Loop, including "delay()"-replacements using millis()-counter
    • Three Interrupt Service Routines (ISR) for Button-processings ⇒ D6, D7 and D8
    • More Subroutines: printMonth(), printDay(), getTimeFromRTC(), decToBcd(), bcdToDec(), getTemperature(), setTime(),
                        setSQWoff_IntOn(), clearAlarmFlags(), setAlarm1(), resetAlarm1(), getAlarm1(), valuesToZero(), clockDisplay().

==================================================================================================
Display "MODES": mode 0,1,3-9 are drawing specific Display-Screens, each with unique content
================
Rem to self: Using Button-D6 to select Menus and Button-D8 to change Values. Each D6-press increases mode +1
---------------------------------------------------------------------------
mode-0: Clock-Display (default screen)

mode-1: Calendar-Display //@1st Button-D6-press
mode-2: @ 2nd D6-press: roll-out of Calendar and return to Clock-Display, TOGGLING between Clock ⇔ Calendar with Button-D6

mode-3-9: ⇒ setClock: (set Clock date + time ...), going through modes 3-to-10 with D6, increasing each value with Button-D8 (going-round) or going-ahead with D6,
mode-10: roll-out of "setClock" with D6, saving new Time&Date values to RTC and return to mode-0 (default Clock-Display).
!!! @ each mode: pressing D6 a bit longer stores actual values on RTC and returns to normal Clock-Display (mode-0).
--------
*/

//=============================================================================================================
//--------------- §0 Initializing Chips, #include libraries, #definig settings and variables ------------------
//-------------------------------------------------------------------------------------------------------------
#include <Wire.h>               //I2C communication library, here to communicate with both chips: DS3231 and OLED-Display
//#include <DS3232RTC.h>        //Real Time Clock library + Alarm-functions:  https://github.com/JChristensen/DS3232RTC ⇒ Functions below!
#include <Adafruit_GFX.h>       //Graphics library
#include <Adafruit_SSD1306.h>   //OLED display library
#include <Fontconvert/FreeSansBold12x7.h> //Include a bigger font (12x7pt=17x12px) to display the time
#include <EnableInterrupt.h>    //library to get the Interrupt of all Pins from uC-ATmega328 
#include <EEPROM.h>             //allows reading and writing to the EEPROM of the uC

#define OLED_RESET 9            //(free) Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET); //Set OLED display to the present real size

#define DS3231_ADRESS 0x68      // I2C Adress of RTC-chip

//--------------------------------------------------------------------------------------
//Definig Global Variables:
//--------------------------------------------------------------------------------------
// Important Variables to store Time + Date:
byte sekund, minut, stund, wtag, mtag, monat; //variable-names for Date and Time with two bits (=1 byte), stored as BCD-(binary)-bytes on the RTC-chip

//State-variables used for Interrupt-calls, released by various Pin-inputs:
//volatile boolean PIN_D2 = 0;  //state-variable (flag) for Interrupt on Pin-D2 (INT0) to switch Timer-Alarm On = "1" or Off = "0" (Off with Button-D8 - or automatically after 20s)
boolean PIN_D2 = 1; //read binary
volatile boolean PIN_D7 = 1;    //state-variable (flag) for Interrupt on Pin-D7 (if pressed, PIN_D7 = 0, default = 1)
volatile boolean BUTTON_D6 = 1; //state-variable (flag) for Interrupt on Button-D6 (if pressed, BUTTON_D6 = 0, default = 1)
volatile boolean BUTTON_D8 = 1; //state-variable (flag) for Interrupt on Button-D8 (if pressed, BUTTON_D8 = 0, default = 1)

boolean flash = 0;      //Simple flag for display-flashing = toggle only ONCE per Display-update interval (=0,2s)
unsigned long previous; //Time-mark-variable to refresh the computed Display-values every 100ms
byte interval = 100;    //Display-refresh interval-time to actualize the display (=100ms), decreased if button hold longer, to speed-up setting-changes
byte mode = 0;          //Mode for time and date settings
int tempvar = 0;        //Temporary variable serving to carry increasing values to set time/date/timer, etc.
int loop_Count = 0;     //Counter for number of Display update periods
byte Temp_Count = 0;    //Counter to read the Temperature (1 time) each second
boolean TOL_switch = 0; //flag to decide switching between TimeOut-short (8s) or TimeOut-long (20s) depending on input
byte speed_Wait = 0;    //Counter to count-up 5 cycles after Button-D8 hold pressed-down before increasing speed of upcounting-values (modes 3-9) 

float temp3231;         //Intermediate temperature variable
int batt_value;         //Variable to store the analog-measured Battery-Voltage (on Input "A0")
//bool state = 0;       //variable to display or not the ":" colon between hours and minutes (not needed here)  //(Reminder: boolean = Arduino "bool")

boolean sett_Flag = 0;  //aux-flag for Button-D6 (set active) to count-up or bypass "sett_Count" (loop-Counter, see next line)
byte sett_Count = 0;    //Counter for Button-D6 to count-up 4 loops (~ 400ms), and if pressed longer - jump into SELECT-SETTINGS-mode
boolean sett_aux = 0;   //aux-flag to jump-into SELECT-SETTINGS-mode
boolean go_out = 0;     //aux-flag to jump-out of any SETTINGS-mode (to mode-0 = default Clock-Display)
boolean chng_Flag = 0;  //aux-flag for Button-D8
byte chng_Count = 0;    //Counter for Button-D8 "delay", counting-up +1 each loop
byte led_Count = 0;     //Counter to turn-off the FlashLight-LED after lighting > 0.3s (≈3 loops)
boolean led_On = 0;     //put the FlashLight-flag "on" or "off" (0 = off)

//The ATmega328p uController has 32KB ISP flash memory, 1024B EEPROM, and 2KB (S)RAM (=Static Random-Access Memory),
//To save RAM, put invariable-bytes (=const ...) in Flash-memory instead of (S)RAM:
//#define... puts bytes in flash, not in RAM, same as "const byte ...",
//...but "const" being preferred over #define because it allows the compiler to know the type of the variable:
byte jahr = 22; //actual "starting" year, afterwards it's possible to modify it.
const byte daysInMonth[14] = {31,31,28,31,30,31,30,31,31,30,31,30,31};   // array: not leap years
const byte daysInMonthLJ[14] = {31,31,29,31,30,31,30,31,31,30,31,30,31}; // array: leap year, f.ex. 2020 == LEAPYEAR!

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ §1 Setup ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
  //Define Output and Input-Pins and their state (HIGH or LOW)
  pinMode(5, OUTPUT);   //Pin-D5 steers the Power-Mosfets: after "time-out" VCC goes LOW, saving Battery (uC down, OLED = dark)
  PORTD |= B00100000;   //= digitalWrite(5, HIGH); //Start with Pin-D5 HIGH, so Dual P/N-Mosfets conducting and VBAT supplying VCC (to uC + Display)

  pinMode(2, INPUT_PULLUP); //Pin-D2 with activated pullup-resistor, to receive the RTC-Alarm Signal and activate the Alarm-Routines
  pinMode(6, INPUT_PULLUP); //Pin-D6 with activated pullup-resistor, for the time/date/alarm MODE SELECT-Button).
  pinMode(7, INPUT_PULLUP); //Pin-D7 with activated pullup-resistor, to Reset all Input-Values (connecting D7 to GND)
  pinMode(8, INPUT_PULLUP); //Pin-D8 with activated pullup-resistor, for the CHANGE-Button, so Signal HIGH when button released,

  pinMode(3, OUTPUT);   //Set Pin-D3 to power the FlashLight
  PORTD &= B11110111;   //= digitalWrite(3, LOW); //Initialize with FlashLight set "Off" per BitMath
  pinMode(4, OUTPUT);   //Set Pin-D4 as output for external vibrating Timer-Alarm (Mini Vibration DC-Motor 3V, activated with N-Mosfet)
  PORTD &= B11101111;   //= digitalWrite(4, LOW); //Initialize external alarm set "Off"

  //Interrupt-calls per library:
  //enableInterrupt(2, PinD2_LOW, FALLING);  //Interrupt-Call from Pin-D2 (set as input), calls function "PinD2_LOW" ⇒ PIN_D2 = 0; //Interrupt not used here!
  enableInterrupt(6, ButtonD6_LOW, FALLING); //Interrupt-Call from Button-D6 (set as input), calls function "ButtonD6_LOW" ⇒ BUTTON_D6 = 0;
  enableInterrupt(7, PinD7_LOW, FALLING);    //Interrupt-Call from Pin-D7 (set as input), calls function "PinD7_LOW" ⇒ PIN_D7 = 0;
  enableInterrupt(8, ButtonD8_LOW, FALLING); //Interrupt-Call from Button-D8 (set as input), calls function "ButtonD8_LOW" ⇒ BUTTON_D8=0;

  //Serial.begin(115200); //Initialize the serial port, if needed (not used here = uC without bootloader!)

  //Use the internal 1,1 Volt Reference of the ATmega328-chip, to measure its Batt.-Voltage (on A0)
  analogReference(INTERNAL);

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Set the I2C-Adress for the display !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // by default, the high voltage is generated from the 3.3V line internally
  display.begin (SSD1306_SWITCHCAPVCC, 0x3C); //initialize with the I2C addr 0x3C or 0x3D (Adafruit-default = 0x3D)
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  display.setTextSize(1);     //Set default font size (=small): 5x7px | x-letter-distance = 1pixel, y-line-distance = 4pixel, (x=6px,y=11px); 
  display.setTextColor(WHITE);//display white font color on black background
  display.dimm();             //begin with OLED-display set to a usble dimmed brightness (dimmed at night, from 22PM until 7AM)
  display.invertDisplay(0);   //set display to normal video (white fonts and drawings over black background, "1" is viceversa)
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ End of Setup ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


//******************************************************************************************************
///////////////////////~~~~~~~~~~~~~~~~~~~~~ §2 Main Loop ~~~~~~~~~~~~~~~~~~~~~/////////////////////////
void loop() {
 //Draw and update actual Display(-mode) every refresh period (=100ms):
 if (millis() - previous >= interval) { //uC millis()-counter with 100ms interval duration,
  previous = millis();  //actualizing the time-variable "previous" with Arduinos (after HW-Start up-counting) "millis()"
  loop_Count++;        // = loopCount + 1; increase the Loop-Counter every 100ms, serving as time-base for Settins (Timer, Clock, etc...)
  //Toggle flash flag for cursor blinking (100ms-"LOW" + 100ms-"HIGH" = 0.2s = 5Hz
  //if (flash == 0) {flash = 1;} else {flash = 0;} //alternative: flash = (flash == LOW) ? HIGH : LOW;
  flash = !flash; //simpler, since only alternating on same 100ms-interval ⇒ used to show a blinking-rectangle surrounding the value-to-set

 //After max. 2 Minutes, shutdown HW (to avoid to depleting Battery, f.ex. if FlashLight rests permanently "On")
 if (millis() == 120000) { PORTD &= B11011111; } //same as digitalWrite(5, LOW); per BitMath

  //////////////////////////////////////////////////////////////////////////////////////////
  //--------------- §2a Read and prepare values from uC, EEPROM and RTC ---------------
  //Reading Time & Date from RTC, stored in 7 variables: sekund, minut, stund, wtag, mtag, monat, jahr
  getTimeFromRTC(); //Every 100ms: read the current Date + Time,
  //then force a temperature conversion if it is not in progress, for fast update of temperature (default temp.-reading: once/10s)
  Temp_Count++; //counter to check the temperature of RTC on each second (10x 100ms)
  if (Temp_Count >= 10) {
    getTemperature(); //check the temperature from DS3231-chip and store value in "tempF" (function below)
    Temp_Count = 0;   //reset it, to read the Temp after next second
  }
  //Dimming the display (at night):
  if ((stund >= 22) || (stund < 7)) { display.dimm(); } //Dim the display between 10 PM and 6 AM (dim"m"=minus)
  else { display.dimp(); } //otherwise set the display to full brightness (dim"p"=plus)

  //Clear Display every cycle:
  display.invertDisplay(0); //(default: 0 = WHITE over BLACK-background) if after Alarm-flashing stopped w. display-inverted, return normal (OLED)-Display
  display.clearDisplay();   //Clear display buffer from last cycle and draw new display-values:
  //From here choose modes per "if-switch" - whether to display Clock-Display, Calendar, FlashLight or Settings...

  ////////////////////////////////////////////////////////////////////////////////////////
  //--------------- §2b-2d: Clock-Display: compute date & time etc. values ---------------
  if (mode == 0) { clockDisplay(); } //call $2 Clock-Display function (⇒ outsourced functions @ end)
  //--------------- End of Clock-Display -------------------------------------------------


  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //--------------- §3 CALENDAR-Display (mode-1), toggling CLOCK-Display per Button-D6 ---------------
 if (mode == 1) { //Calendar:
  //Print a static row with DOW:
  display.setCursor(6,6);
  display.print("Mo Di Mi Do Fr Sa So");  //1st Line
  display.drawLine(6,14,122,14, WHITE);   //stripline

  //Important!: Calculate the "month"begin (mb) with the first (week)day of preceding AND actual month which is on column "Mo" (=Monday)
  byte mb = mtag %7; //modulo 7 = remainder (rest) from div/7
  byte day = mb + (8 - wtag);  if (day > 7) { day = day %7; } //day = FIRST MONDAY of actual month - but we need also the PREVIOUS WEEK:

  byte dim_p; //local auxiliary-variable for previous month
  byte dim;   //local auxiliary-variable for actual month
  //Calculate if actual year is a LeapYear?
  if (jahr%4==0) { //after year 2000(=LJ) we don't need to dig deeper! (avoiding here the need to calculate with modulo 400 and 100 before year 2000...)
    dim_p = daysInMonthLJ[monat-1]; //extract amount of days in *previous* month if actual year is a LeapYear (array) - (LJ = 366 days)
    dim = daysInMonthLJ[monat]; //extract amount of days in *actual* month if actual year is a LeapYear.
  } else {
    dim_p = daysInMonth[monat-1]; //total days in *previous* month (position "0" of month-array is last month of previous year = 31.dec., so array with 14(!) months)
    dim = daysInMonth[monat]; //extract amount of days in *actual* month (in "normal"-year )
  }
  //then extract the first DATE(on Monday) from PREVIOUS REST OF MONTH. Calendar begins with this date!
  byte day_p = dim_p - (7 - day); //"day": the date of first Monday of actual month, ⇒ calculation above!
  byte resting_days = 7 - day; //the AMOUNT of days to display of previous month

  //Now draw all calendar-days from actual month (+ max.6 days "pre" and "post" = trailing days of next month)
  display.setCursor(6,16);

  //byte i, j; //locally used auxiliary-variables
  if (resting_days < 7) { 
    for (byte i=0; i<=resting_days; i++) {      //1. display the REST of preceding month from Monday on
    display.print(day_p++); display.print(" "); //" " " "
    }
  }
  for (byte j=1; j<=dim; j++) {                 //2. display days IN month
    if (j == mtag) { display.setTextColor(BLACK, WHITE); } if (j<10) { display.print("0"); } //with leading-zero | reverting colors black⇔white
    if (j == mtag) { display.setTextColor(BLACK, WHITE); } display.print(j);   //print actual day, printing black letters over a white background
    if (j == mtag) { display.setTextColor(WHITE, BLACK); } display.print(" "); //print a "white"(=transparent) trailing space for next day (over black background)
    //display.setTextColor(WHITE, BLACK); //only if textCOLOR changed before (BLACK on WHITE background), Clock-default is WHITE on BLACK BG.
  }
  for (byte k=1; k<=10; k++) {                  //3. display the first 7 days of next month
    display.print("0"); display.print(k);
    display.print(" "); //+ trailing space
  } //8 rows x 7days/week = 56 days max (if out of screen, "overflow hidden" possible but not visible = no problem here!)

  //If either Button-D6 or Button-D8 were pressed, roll-out/toggle from Calendar to default mode-0 (Clock-Display)...
  if ((BUTTON_D8 == 0) || (BUTTON_D6 == 0) || (mode == 2)) { mode = 0; BUTTON_D8 = 1; BUTTON_D6 = 1; led_On = 0; //...clear Button-flags and FlashLight-flag.
  }
 } //End of "if"-Calendar...
  if (mode == 2) { mode = 0; TOL_switch = 0; } //definitively go-out to mode-0, resetting also "TOL"-Switch to choose TimeOut_short directly (8s "countdown").
  //--------------- End of Calendar-Display --------------------------


  //////////////////////////////////////////////////////////////////////
  //--------------- §4: "setClock"-DISPLAY: modes 3-9 ---------------
  if ((mode >= 3) && (mode <= 9)) {
   clockDisplay(); //call the whole Clock-DISPLAY function  (⇒ outsourced functions below)
   
  //Day-Of-Week setting:
   if (mode == 3) { //here = "wtag", values from 1 - 7 to store on RTC as a BCD-number
    interval = 100; //(reset to normal speed)
    if (flash) { display.drawRect(8,0,21,9,WHITE); } //Display rectangle cursor every other display update (5Hz blinking)
    if ((BUTTON_D8 == 0) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = wtag; //Get the current weekday (from variable "weekday") and save in temporary variable "tempvar"
     tempvar = tempvar + 1; //Increment the day at 5Hz rate if Button-down
     if (tempvar > 7) { tempvar = 1; } //Roll over after 7 days
     wtag = tempvar; //Get the current weekday and save its value in temporary "tempvar"-variable, then write its value (now in "wtag") back to the RTC module;
     //"tempvar": We need this auxiliary variable, because RTC is overriding "wtag"-values on SRAM every loop (=0.1s) - but it's new value here is written directly to RTC *on next line*, before updating
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x03); //Day (=weekday, 1-7)
     Wire.write(decToBcd(wtag)); //writes BCD-converted value down
     Wire.endTransmission();
    }
   } //Day Of Month setting:
   if (mode == 4) { //"mtag" (Date = number)
    if (flash) { display.drawRect(38,0,15,9,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((!BUTTON_D8) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = mtag; //Get the current date and save in temporary variable //(mtag = 28,29,30,or 31 days)
     tempvar++; //Increment the date at 5Hz rate
     speed_Wait++; //Increment the Button-pressed counter after 5 Button-presses :
     if (speed_Wait == 5) { interval = 50; speed_Wait = 0; } //shorten count-up time when holding Button-down
     //(RTC allows incorrect date setting for months < 31 days, but will use correct date rollover for subsequent months.
     if (tempvar > 31){ tempvar = 1; } //Roll over after 31 days
     mtag = tempvar; //After each update, write the date back to the temp-variable,
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x04); //Date (=day, 1-31)
     Wire.write(decToBcd(mtag));
     Wire.endTransmission();
    }
   } //Month setting:
   if (mode == 5) { //(=numerical values of 12 months, from 1-12)
    interval = 100; //(reset to normal speed)
    if (flash) { display.drawRect(62,0,21,9,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((!BUTTON_D8) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = monat; //Get the current month and save in temporary variable
     tempvar++; //Increment the month at 5Hz rate
     if (tempvar > 12){tempvar = 1;} //Roll over after 12 months
     monat = tempvar; //After each update, write the month back to the temp-variable,
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x05); //Month (=month, 1-12)
     Wire.write(decToBcd(monat));
     Wire.endTransmission();
    }
   } //Year setting:
   if (mode == 6) { // (incrementing from year 00 to 99, jumping at 1st Button-D8-press to "19")
    if (flash) { display.drawRect(92,0,27,9,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((BUTTON_D8 == 0) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = jahr; //Get the current year and save in temporary variable ("yy" only with 2 bits!)
     tempvar++; //Increment the month at 5Hz rate
     speed_Wait++; //Increment the Button-pressed counter
     if (speed_Wait == 5) { interval = 50; speed_Wait = 0; } //After 5 Button-presses shorten count-up time when holding Button-down
     //RTC allows setting from 1900, but range limited here from 00 to 99
     if ((tempvar < 20) || (tempvar > 99)) { tempvar = 20; } //Roll over after 99 directly to (20)20 (alt=2000)
     jahr = tempvar; //After each update, write the year back to the temp-variable,
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x06); //Year (=year_s, 1900-2099)
     Wire.write(decToBcd(jahr));
     Wire.endTransmission();
    }
   } //Hour setting:
   if (mode == 7) { //...with a geater font (new line!) // (incrementing hour from 0-23h)
    if (flash) { display.drawRect(5,15,28,19,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((BUTTON_D8 == 0) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = stund;  //Get the current hour and save in temporary variable
     tempvar++; //Increment the month at 5Hz rate
     speed_Wait++; //Increment the Button-pressed counter
     if (speed_Wait == 5) { interval = 50; speed_Wait = 0; } //After 5 Button-presses shorten count-up time when holding Button-down
     if (tempvar > 23) {tempvar = 0;} //Roll over hour after 23rd hour (setting done in 24-hour mode)
     stund = tempvar; //After each update, write the hour back to the temp-variable,
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x02); //Hours (=hour, 00-23)
     Wire.write(decToBcd(stund));
     Wire.endTransmission();
    }
   } //Minute setting:
   if (mode == 8) { //(incrementing from 0-60m)
   if (flash) { display.drawRect(38,15,28,19,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((BUTTON_D8 == 0) && (!flash)) { //Update setting at 5Hz rate if button held down
     tempvar = minut; //Get the current minute and save in temporary variable
     tempvar++; //Increment the month at 5Hz rate
     speed_Wait++; //Increment the Button-pressed counter
     if (speed_Wait == 5) { interval = 50; speed_Wait = 0; } //After 5 Button-presses shorten count-up time when holding Button-down
     if (tempvar > 59) {tempvar = 0;} //Roll over minute to zero after 59th minute
     minut = tempvar; //After each update, write the minute back to the temp-variable,
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x01); //Minutes (=minute, 0-59)
     Wire.write(decToBcd(minut));
     Wire.endTransmission();
    }
   } //Second synchronization:
   if (mode == 9) { //...to set at "00" seconds:
    interval = 100; //(reset to normal speed)
    // Set clock + 1 minute, then press and hold to freeze second setting. Release Button-D8 at 00 seconds to synchronize clock to external time source.
    if (flash) { display.drawRect(33,19,6,14,WHITE); } //Display rectangle cursor every other display update (5Hz blink)
    if ((BUTTON_D8 == 0) && (!flash)) { //Update setting at 5Hz rate if button held down
     sekund = 0; //After each update, write the zeroed second back to it's variable
     //then write the variable back to the RTC module after each update
     Wire.beginTransmission(DS3231_ADRESS);
     Wire.write(0x00); //Seconds (=second, 0-59)
     Wire.write(decToBcd(sekund));
     Wire.endTransmission();
    }
   //if Button-D6 is pressed, roll-out from "setClock" (from mode-9 into mode-10 ⇒ if (mode-10), roll-out to mode-0, below)
   if (!BUTTON_D6) { mode = 10; BUTTON_D6 = 1; BUTTON_D8 = 1; } //leaving mode-16:
   }
   //Prepare for new D8-pressing: if D8 is released (D8=HIGH) after pressed in previous loop (waiting 100ms for next loop):
   if ((PINB & (1 << PB0) != 0)) { BUTTON_D8 = 1; led_On = 0; } //(digitalRead(8)) //Reset "BUTTON_D8" flag, staying HIGH for all following D8-pressings (INSIDE "setTimer" = modes 6-8)
  }
  //--------------- End of "setClock-SETTINGS ---------------
  //Going-out of setClock-SETTINGS (modes-3-9), rolling-back from mode-10 to mode-0,
  if (mode == 11) { mode = 0; TOL_switch = 0; loop_Count= 0; led_On = 0; interval = 100; } //and going into TO-short, prevent the FlashLight(-flag) to turn FL-on and reset "interval"-time to default


  ////////////////////////////////////////////////////////////////////////////////////////
  //--------------- §5 Button-processing (D6, D7 and D8) + "flag"-Switches ---------------
  //Button D6: Clock SELECT-Mode (= going through Modes 0-10 and toggling between Calendar- and Clock-Display)
  sett_Count++; //increase the counter each loop +1 (to compare later if Button-D6 is longer "down"...
  if (BUTTON_D6 == 0) { //if Button-D6 (=PD6) was pressed, the Falling-Interrupt function (ISR) sets the Button-STATE "BUTTON_D6" to LOW //(!digitalRead(6) replaced per Pin-Change-Interrupt !
    //delay(30);        //here debounce is not necessary because ISR can only set "BUTTON_D6" to "0"(=FALLING), not to "1" - this happens later,
    //so starting a 2-function "non-blocking delay" with state-flags (important: not obstructing the uC going ahead-round in the loop!)
    if (sett_Flag == 0) { sett_Count = 0; sett_Flag = 1; } //if D6is pressed, then set its flag "active" and RESET its Counter
    //Control every loop: Button still down after 4 loops? (~400ms)...
    //...then a) jump-into SELECT-SETTINGS-Display (mode 3)...
    if ((mode == 0) && (sett_Flag == 1) && (sett_Count >= 4)) { //only in mode-0: if Button-D6 still is hold-down longer than 4 loops (~400ms) and if its flag still is active,
      sett_aux = 1;    //set an aux-flag to indicate to goto mode-3 (=SELECT-SETTINGS-Display); mode-3 is choosen @next D6-RELEASE,
    } //...or b) jump-out of any SETTINGS-mode (to mode-0):
    if ((mode >= 3) && (mode <= 9) && (sett_Flag == 1) && (sett_Count >= 4)) { //if into modes 3-16 and Button-D6 still down after 4 loops (~400ms),
      go_out = 1;     //set an aux-flag to indicate to jump-out directly to mode-0 @next D6-RELEASE (= to Clock-Display) but only if in any "setClock"-mode,
    }
  } //After Button-D6 RELEASE, set the way to go:
  //Compare using this 2 aux-flags (if "sett_aux" OR "go_out") on Button-D6-RELEASE! (= jump-to mode-3 OR jump-out to mode-0, depending on previous modes)
  if (digitalRead(6) && (BUTTON_D6 == 0)) { //(PIND & (1 << PD6) != 0) //after Button-D6 released (=now HIGH, but Button-STATE still LOW), and BUTTON_D6(-state) beeing down before,
    mode++; BUTTON_D6 = 1; //increment the select-mode (mode++ = +1 each button-press) to cycle through "modes"(=Screens), and reset Button-D6-STATE to HIGH,
    if (sett_aux == 1) { mode = 3; sett_aux = 0; TOL_switch = 1; loop_Count = 0; } //if in mode-0 and D6 is long-pressed, jump directly to mode-3 (sett_aux = 1 was set before (above line)
    if (go_out == 1) { setTime(); mode = 0; interval = 100; TOL_switch = 0; loop_Count = 0; go_out = 0; //if in any other SETTINGS-mode and D6 is long-pressed, jump-out directly to mode-0,
    } //Reminder for setTime(): stores all Date+Time-values to RTC. Then resets flags, counter, etc. and goes to mode-0.
    sett_Flag = 0; //finally reset the time-marker flag (after D6-release), so next time-mark for 400ms-measure begins after next BUTTON_D6=LOW (pressed).
  }


  //Grounding-Pin D7 = Soft-Reset: Time, Date, Alarm and stored data, ⇒ functions @ end.  "Soft-Reset" is also released, if D6 + D8 are pressed together (in mode-0)!
  if (PIN_D7 == 0) { valuesToZero(); PIN_D7 = 1; } //Reset ALL STORED VALUES to zero immediately: RTC, SRAM(-Variables) and uC-EEPROM
  if ((!BUTTON_D6) && (!BUTTON_D8) && (mode == 0)) { sett_Flag = 0; valuesToZero(); mode = 0; led_On = 0; BUTTON_D6 = 1; BUTTON_D8 = 1; } //same result as above, through software


  //Clock CHANGE Button D8 (= Date, Time and Alarm values and toggles FlashLed on/off)
  chng_Count++; //increase the counter each loop +1 (to compare later if Button-D8 is a bit longer "down"...
  if (BUTTON_D8 == 0) {  //if Button-D8 (=PB0) was pressed, the Falling-Interrupt function (ISR) sets the Button-STATE "BUTTON_D8" to LOW // (!digitalRead(8) replaced per Pin-Change-Interrupt !
    if (chng_Flag == 0) { chng_Count = 0; chng_Flag = 1; } //reset the counter and set its flag "active",
    //then activate VCC:
    PORTD |= B00100000; //digitalWrite(5, HIGH); //stabilize the Dual-Mosfet-Flip-Flop with Pin-D5 HIGH (yet "On" while Button-D8 pressed),
                        //so Mosfet-Pair is conducting VBAT to VCC and uC + OLED-Display are getting current from Battery.
    if ((chng_Flag == 1) && (chng_Count >= 1)) { //wait 1 loop ≈100ms (to stabilize Button-D8 bouncing)
      led_On = !led_On; chng_Flag = 0; //toggle FlashLight on or off and reset D8-flag
    }
    if (mode >= 1) { led_On = 0; } //If in Calendar-mode and Button-D8-pressed, turn-off FlashLight and terminate Calendar instantly, toggling with (default) mode-0 (=Clock-Display)
  }
  //--------------- End of Button-processing ---------------


  ///////////////////////////////////////////////////////////////////////
  //--------------- §6 FlashLight-LED -----------------------------------
  //FlashLight: LED3 turned On or Off ("OSRAM LED DURIS E3 white", Package: Chipled 1206) - pressing Button-D8:
  if ((led_On == 1) && (mode == 0) && (digitalRead(8))) { //but only in mode-0 (=normal Clock-Display-mode) and if variable "led_On" is "1" and if Button-D8 is released,
    //then set Pin-D3 HIGH (30mA from Pin-D3 for the Flash-LED)
    PORTD |= B00001000;     //= digitalWrite(3, HIGH); //same as line above, per BitMath (⇒ OR-ing with "1" modifies only this bit, the other bits keeps untouched)
    led_Count++; //if lighting, increment led-counter +1
    loop_Count= 0; //resetting the Frame-Counter here EVERY CYCLE to "0" ⇒ resets also the TO-timer, so TimeOut can't count-up - no ShutDown till Battery empty!
    BUTTON_D8 = 1; //reset the BUTTON-flag here (to default="1")
  }
  
  if (mode > 0) { PORTD &= B11110111; led_On = 0; } //= digitalWrite(3,LOW); //if in any other mode than "0", switch-off the FlashLight and it's flag to prevent LED-lighting "out of the box"
  
  if ((led_Count >= 2) && (BUTTON_D8 == 0) && (digitalRead(8))) {//after lighting ~200ms (2 loops) and if Button-D8-flag is still LOW, but Button released, turn-off FlashLight:
    PORTD &= B11110111;   //same as "digitalWrite(3, LOW);" per Bith Math (⇒ AND-ing with "0" modifies only this bit, the other bits keeps untouched)
    //(D5: only for test-purpose: (PORTD &= B11011111;) = digitalWrite(5, LOW); //set Pin-D5 also LOW ⇒ turn off Power, ShutDown here)
    led_Count = 0; led_On = 0; //reset led-counter and the "led-on"-flag,
    BUTTON_D8 = 1; //and reset also the BUTTON-flag for next D8-pressing
  }


  /////////////////////////////////////////////////////////////////////////////////
  //--------------- §7 TimeOut-processing (TOL_switch = "1" or "0") ---------------
  //Go either TO-long or TO-short, depending on which Button/decision was pressed/made before: (using the "loop_Count"-counter = 100ms/loop, on begin)
  ////TO-long:
  if ((TOL_switch == 1) && (loop_Count >= 130)) { //flag set by Button-D6 and Timer-Alarm ⇒ 20s lighting ("TOL"), then goto "TOS" and add 8s more; Reminder: 20s ≈ 130x (100+60)ms /loop
    TOL_switch = 0; loop_Count = 0; mode = 0; //reset the TOL-switch flag and also the loop-Counter to continue with 8s-TO-short (@next step) ⇒ in mode-0 (Clock-Display),
  } //or TO-short:
  if ((TOL_switch == 0) && (loop_Count >= 50)) { //default: short = 8s (+8s more after 20s "long" TimeOut)... Reminder: 8s = 50x (100+60)ms /loop
    PORTD &= B11011111; //= digitalWrite(5, LOW); //"ShutDown" Power-consuming ICs (=disconnecting them from VBAT) ⇒ if Pin-D5 is LOW, the Mosfet-
                        //Transistor-Pair is blocking +VBAT-connection, so +VCC goes to 0V, thus uC + OLED are "down" (OLED dark, saving energy), only RTC-chip remains "On".
  }


  display.display(); //Finally DISPLAY the constructed frame buffer for one loop
  ////////////////////// End of DISPLAY-RENEWING Interval //////////////////////
  }
}
//////////////////////~~~~~~~~~~~~~~~~~~~~~ End of Main Loop ~~~~~~~~~~~~~~~~~~~~~//////////////////////
//******************************************************************************************************



///////////////////////////////////////// §8 FUNCTIONS (outsourced) /////////////////////////////////////////
//------------------------------------------------ nl -------------------------------------------------------

//2 Subroutines to set more REAL dimming (at night), added in the "Adafruit_SSD1306.cpp" library: see "nl_addition" below (⇒ copy this paragraph to the library)
// void dimm() {...
// void dimp() {...


//3 Interrupt Service Routines (ISR's) for Button processing (from Input-Pins D6-D8)
//----------------------------------------------------------

//RTC-Pin-3-Signal triggeres uC-D2-Input, if RTC-Time and RTC-Alarm1-Time matches - but after RTC-Reset from "INT/SQW" on StartUp, D2-Input="0", so falling-INT can't be used:
//⇒ D2 LOW Input-Signal activates also HW-transistors ⇒ VBAT connected to uC ⇒ INT @StartUp = "0", but after RTC-reset (@ Setup) a RISING Signal on D2 triggeres "PIN_D2"-flag = "1" !
//void PinD2_LOW() { PIN_D2 = 0; } //activate Timer-Alarm on FALLING-Signal //not used, bec.of reset on StartUp ⇒ digitalRead(2) to read-out D2 status if RTC-Alarm has triggered

//Signal LOW if Pin-D7 grounded, flag = 0 (LOW)
void PinD7_LOW() { PIN_D7 = 0; }

//Signal LOW if Button-D6 pressed, flag = 0 (LOW)
void ButtonD6_LOW() { BUTTON_D6 = 0; loop_Count = 0; } //each time Button-D8 is pressed, reset also the loop-cycle counter, to reset the base-counter for TimeOut.

//Signal LOW if Button-D8 pressed, flag = 0 (LOW)
void ButtonD8_LOW() { BUTTON_D8 = 0; loop_Count = 0; } //each time Button-D8 is pressed, wait for small-debounce and reset the loop-cycle counter, " " " " .


//Subroutine to display month string from numerical month argument
void printMonth(int month) {
  switch(month)
  {
    case 1: display.print("Jan. ");break;
    case 2: display.print("Feb. ");break;
    case 3: display.print("Mar. ");break;
    case 4: display.print("Apr. ");break;
    case 5: display.print("Mai  ");break; //alt: "May "
    case 6: display.print("Jun. ");break;
    case 7: display.print("Jul. ");break;
    case 8: display.print("Aug. ");break;
    case 9: display.print("Sep. ");break;
    case 10: display.print("Okt. ");break; //alt: "Oct "
    case 11: display.print("Nov. ");break;
    case 12: display.print("Dez. ");break; //alt: "Dec "
  }
}

//Subroutine to display day-of-week string from numerical day-of-week argument
void printDay(int day) {
  switch(day)
    {
    case 1: display.print("Mon, ");break;
    case 2: display.print("Die, ");break; //alt: "Tue "
    case 3: display.print("Mit, ");break; //alt: "Wed "
    case 4: display.print("Don, ");break; //alt: "Thu "
    case 5: display.print("Fre, ");break; //alt: "Fri "
    case 6: display.print("Sam, ");break; //alt: "Sat "
    case 7: display.print("Son, ");break; //alt: "Sun "
  }
}

//2 Auxiliary functions:
byte decToBcd(byte val) {return ((val/10*16) + (val%10));}
byte bcdToDec(byte val) {return ((val/16*10) + (val%16));}

//*Read-out* time+date from RTC via I2C interface:
void getTimeFromRTC() {
  // initialise:
  Wire.beginTransmission(DS3231_ADRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADRESS, 7); // 7 bytes are requested
  sekund = bcdToDec(Wire.read() & 0b01111111); //0x7f - removes the high-order bit because it isn't part of the seconds,
  minut = bcdToDec(Wire.read());
  stund = bcdToDec(Wire.read()  & 0b00111111); //0x3f - removes the two high-order bits because they aren't part of the hours, but
  wtag = bcdToDec(Wire.read());                //in both cases the unused bits are defined to be zero so the 0x7f and 0x3f aren't strictly necessary.
  mtag = bcdToDec(Wire.read());
  monat = bcdToDec(Wire.read());
  jahr = bcdToDec(Wire.read());
  Wire.endTransmission();
  //jahr = EEPROM.read(0);  //read-out from EEPROM (?), bec. RTC looses it on each Battery-change
}

void setTime() { //write time-values all together (to RTC) with one stroke...
  Wire.beginTransmission(DS3231_ADRESS);
  Wire.write(0x00); //Begin with seconds on adress 0x00
  Wire.write(decToBcd(sekund));
  Wire.write(decToBcd(minut));
  Wire.write(decToBcd(stund));
  Wire.write(decToBcd(wtag));
  Wire.write(decToBcd(mtag));
  Wire.write(decToBcd(monat));
  Wire.write(decToBcd(jahr));
  Wire.endTransmission();
  //EEPROM.write(0, jahr); //write actual year also to EEPROM, (set @ Setup *in SRAM*), bec. the RTC initially sets each value to "0"...
}

void getTemperature() { //get the Temperature from RTC-Chip
  byte tMSB, tLSB;
  //(temp registers (11h-12h) get updated automatically every 64s)
  Wire.beginTransmission(DS3231_ADRESS);
  Wire.write(0xE); //Check first the CONV flag on Control register (see DS3231 datasheet, "Timekeeping register" on pages 11 and 13)
  Wire.endTransmission();
  Wire.requestFrom(DS3231_ADRESS, 1);
  if ((Wire.read() & 0b00100000) == 0) {  //Check first for CONV flag = "0" to see if (internal) conversion has completed!
    Wire.beginTransmission(DS3231_ADRESS);
    Wire.write(0x11);
    Wire.endTransmission();
    Wire.requestFrom(DS3231_ADRESS, 2);
    if (Wire.available()) {
      tMSB = Wire.read(); //2's complement int portion
      tLSB = Wire.read(); //fraction portion
      temp3231 = ((((short)tMSB << 8) | (short)tLSB) >> 6) / 4.0;
    //temp3231 = (temp3231 * 1.8) + 32.0; //Convert Celsius to Fahrenheit
    }
    Wire.endTransmission();
  }
}

//SOFT-RESET:
//If Pin-D7 is grounded (with GND-Pad nearby), this "Soft-Reset" function "soft-resets" all values (without rebooting or going out of loop!)
void valuesToZero() { 
  sekund=0; minut=0; stund=0; wtag=1; mtag=1; monat=1; jahr=22;
  setTime(); //set above values
  mode = 0; led_On = 0; PORTD &= B11101111; //set mode-0 and turn-off FlashLight
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//--------------- $2: CLOCK-Display function: "mode-0" and (Clock-)Setting "modes 3-9"
///////////////****** Display computed date/time/batt/temp-values on Clock-Screen ******///////////////
//--------------- §2b Display computed date/time
 void clockDisplay() { //(mode-0)
  display.setTextSize(1); //default small font-size (5x7px) //display.setFont(0); (alt.)
  display.setCursor(10,1);
  printDay(wtag); //Lookup day of week string from retrieved RTC data and write to display buffer (Mon-Son, see function)

  if (mtag < 10) {display.print("0");} //Add leading zero to date display if date is single-digit
  display.print(mtag); //Write date (= numerical Day Of Month) to display buffer

  display.print(". "); //Write a dot and a space between date and month
  printMonth(monat); //lookup month from retrieved RTC data and write abbreviated string to display buffer (see function)

  if (jahr < 10) { display.print("200"); } //Add 2 leading zero's to year if date is a single-digit
  else { display.print("20"); }  //Add 1 leading zero to year if date is a double-digit
  display.print(jahr);    //the full year to display. "2000" - shows that correction is needed...
  display.print(" "); //+1 space

  //-------- new line (showing time: Hours and Minutes) ---------
  display.setFont(&FreeSansBold12x7); //display the time with a bigger font
  display.setCursor(6,32); //Position text cursor for time display with the big font

  if (stund < 10) {display.print("0"); display.print(stund);} //If single digit hour, add a leading zero
  else {display.print(stund);} //Display 24h hour

  //Display the Colon(:) between hours and minutes
  display.setCursor(33,31); //Position the cursor for the ":" separator between hours and minutes
  display.print(":"); //and display the hour-minute ":"-colon-separator

  display.setCursor(39,32); //Position the cursor for minutes
  if (minut<10) {display.print("0");} //Add a leading zero if single-digit minute
  display.print(minut); //Display retrieved minutes

  //////////////////////////////////////////////////////////////////////
  //--------------- §2c Calculate Voltage of the Battery ---------------
  batt_value = analogRead(A0); //measures the voltage on the A0-Pin (= Pin-14)
  //float volts = 0.0010753 * val;   //= (1.1/1023) * val; //max. Volts on Pin A0 ! (blue-LED: 2,5V forward voltage / 100kΩ / 8µA current)
  //Batt-full   = 3,0V -2,5Vled = 0,5Vmax; (alt. full: 3,25V)
  //Batt.-empty = 2,6V -2,5Vled = 0,1Vmin
  //Calculation:
  //Batt-full ⇒  0,5V/1,1Vref = 0,4545 * 1023 = 465 bits (Max.) ⇒ rounded to 470 bits
  //Batt-empty ⇒ 0,1V/1,1Vref = 0,091 * 1023 =  93 bits (Min.) ⇒ rounded to 100 bits
  //Reminder to self: map(batt_value, fromLow, fromHigh, toLow, toHigh)
  int vol = map(batt_value, 100, 470, 0, 14);  //16(px) = total bar-length; 14px = inner-length (1px-border)
  if (vol >= 14) {vol = 14;} //limitation to max. 14px length

  //Now draw Battery + display Voltage
  display.setFont(0); //reset the Font from preceding "FreeSansBold12x7"
  display.setCursor(11,42); //(3 Rows á 9px +1) = 10+9+9 = 28px
  display.print("Batt");
  //Reminder: display.drawRoundRect(x, y, w, h, radius, color);
  display.drawRoundRect(39,41,18,8,1,WHITE); //Batt-rectange outline, Rect: 20x8 px
  //Reminder: display.fillRect(x, y, w, h, color);
  display.fillRect(57,43,2,4,WHITE);   //small rectange-knob, 2x3 px
  display.fillRect(41,43,vol,4,WHITE); //bargraph with "vol" as Volt-indicator (x-line)
/*display.setCursor(10,50);            //next line for Volts in text
  display.print(volts);                //display the measured Batt.-Voltage
  display.print(" V");
*/
  //Display the (internal) Temperature of RTC chip (DS3231)
  display.setCursor(13,53); //Set the cursor for temperature display
  display.print(temp3231);  //Send the gathered temperature to the display buffer
  display.drawCircle(46,53,1,WHITE); //Draw degree symbol after temperature (x,y=center + radius)px
  display.print(" C");

  display.setCursor(110,53);  //overwrite needless blinking-pixels at right-bottom of OLED-Display
  display.drawRect(110,53,18,11,BLACK); //draw an outer line-rectangle first (draw from upper-left: x,y, - to bottom-left: x-width,y-height, color)
  display.fillRect(110,53,18,11,BLACK); //fill it with inner-line-rectangles

  ////////////////////////////////////////////////////////////////
  //--------------- 2d: Draw the analog clock face ---------------
  display.drawCircle(display.width()/2 + 31, display.height()/2 + 5, 23, WHITE); //Position and draw clock outer circle (display=128x64px; 128/2+20 = 84, y=64/2+4 = 36, r=22/d=44)
  //display.fillCircle(display.width()/2+25, display.height()/2 + 6, 22, WHITE); //Fill circle only if displaying inverted colors
  //if (flash) {    //don't flash the tiny circle, so here no "if",
  display.drawCircle(display.width()/2 + 31, display.height()/2 + 5, 2, WHITE); // } Draw, position (and blink) tiny inner circle
  display.drawRoundRect(71,13,49,49,4,WHITE); //Draw a rounded box around outer clock-circle - "rounded-box" draws from location (x1,y1) to down-right (x2,y2) + round-radius. (84-22 = 62, 36-22 = 14, ...)

  //Position and draw hour tick marks (=5min)
  for( int z=0; z < 360; z = z + 30 ) { //Begin at 0°, then mark an "ange" every 30° and stop at 360° (nl: begin at 0° = vertical tick-bar)
    float angle = z ; //(z = 30°) //Next: Important!: the uC needs radians to calculate trigonometric-values, so:
    angle=(angle/57.29577951) ;   //convert degrees to radians (1 rad = 180/pi °deg = 57.295779513°) ; (calculation below with normal degrees for control-purpose)
    int x1=(95+(sin(angle)*22));  //draws a line from x1,y1 to x2,y2 ; x1 = 90+(sin(0°)*r) px = 90+0px = 90px ; x2 = 90px+(sin(0°)*(22-5))px = 90 + 0 = 90px
    int y1=(37-(cos(angle)*22));  //                                   y1 = 37-(cos(0°)*r) px = 15px ;          y2 = 37px-(cos(0°)*(22-5))px = 37 - 0,866*17 = ~22px ; diff: 7px (⇐ each "5 min")
    int x2=(95+(sin(angle)*(22-5))); //2.nd x/y-coordinate
    int y2=(37-(cos(angle)*(22-5))); //" " "
    display.drawLine(x1,y1,x2,y2,WHITE); //draw it 6x (see above "for"-loop)
  }

  //Position and display second hand (nl: from clock-"center"(=fix) to each x/y-position)
  float angle = sekund * 6 ;     //Retrieve stored seconds and apply (each sec-angle = 30°)
  angle=(angle/57.29577951) ;    //Convert degrees to radians (1 deg = 180/π rad = 57.295779513 rad)
  int x3=(95+(sin(angle)*(22))); //radius = 22px length
  int y3=(37-(cos(angle)*(22)));
  display.drawLine(95,37,x3,y3,WHITE);

  //Position and display minute hand
  angle = minut * 6;          //Retrieve stored minutes and apply
  angle=(angle/57.29577951) ; //Convert degrees to radians
  x3=(95+(sin(angle)*(22-3)));
  y3=(37-(cos(angle)*(22-3)));
  display.drawLine(95,37,x3,y3,WHITE);

  //Position and display hour hand
  angle = stund * 30 + int((minut / 12) * 6); //Retrieve stored hour and apply
  angle=(angle/57.29577951) ; //Convert degrees to radians
  x3=(95+(sin(angle)*(22-11)));
  y3=(37-(cos(angle)*(22-11)));
  display.drawLine(95,37,x3,y3,WHITE);

} //--------------- End of CLOCK-Display (mode-0 and nodes 10-16) ---------------
/////////////////////////////////////////////////////////////////////////////////


/*
//Beeping-PWM:
//Timer-2: PWM-Output on Pin-D3, to activate the external Piezo-Beeper if Alarm target-time matches clock-time (else prescaler n=0)
void tone(int n) { //Timer 2 "B", output: OC2B (=D3), fast PWM:
// see https://www.gammon.com.au/forum/?id=11504 ⇒ goto ~65% of page-depth (scroll down) ⇒ "Example code which uses a prescaler of 64"
  TCCR2A = bit (WGM20) | bit (WGM21) | bit (COM2B1); // Fast PWM, Clear OC2B on Compare Match, set OC2B at BOTTOM, (CTC)
  TCCR2B = bit (WGM22) | bit (CS22);  // Toggle OC2A on Compare Match; CS2: prescaler on clk/64
  OCR2A =  n;                // n=70 ⇒ ~3500 Hz // get "n" from table of nick-gammon's page (URL above)
  OCR2B = n/2; // ((n + 1) / 2) - 1; ⇒ ~50% duty cycle
}
*/

  /* print Date+Time on Serial-Output:  (replacing the weekday-NUMBER with a wd-NAME)
  Serial.print(", ");  //Spacers in between (for clarity in the Serial Output)
  Serial.print((day < 10) ? "0" : ""); Serial.print(day);     // Calendar-day (0-31) + leading zero before first row, if the number is less than 10...
  Serial.print(".");
  Serial.print((month < 10) ? "0" : ""); Serial.print(month); // month (1-12) + leading zero before,
  Serial.print(".");
  Serial.print((year < 10) ? "0" : ""); Serial.print(year);  // year (..) (two bits only, without preceding century-bits)
  Serial.print(" - ");
  Serial.print((hour < 10) ? "0" : ""); Serial.print(hour); // hour (1-24)
  Serial.print(":");
  Serial.print((minute < 10) ? "0" : ""); Serial.print(minute); // Minute (1-59)
  Serial.print(":");
  Serial.print((second < 10) ? "0" : ""); Serial.println(second); // second (1-59)
  */


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
/* Add this section to (the end) of "Adafruit_SSD1306.cpp" (search in downloaded Arduino Libraries)
// -------------- nl_addition --------------
void Adafruit_SSD1306::dimm() { //dim"m" = minus
// for (int dim=150; dim>=0; dim-=10) { //fadeout
ssd1306_command(0x81);
ssd1306_command(0); //max 160 - 0
delay(20);

// for (int dim2=34; dim2>=0; dim2-=17) {
ssd1306_command(0xD9);
// ssd1306_command(dim2); //max 34 - 0
ssd1306_command(0);
delay(40);
}

void Adafruit_SSD1306::dimp() { //dim"p" = plus
// for (int dim=0; dim<=160; dim+=10) { //fadein
ssd1306_command(0x81);
ssd1306_command(160); //max 160
delay(20);

// for (int dim2=0; dim2<=34; dim2+=17) {
ssd1306_command(0xD9);
ssd1306_command(34);//max 34
delay(40);
}
//------------ nl_addition end ------------
*/
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


/* /////////////////////////////////////////////////////////////////////////////////////////////
FLASHING compiled Code directly with avrdude, to minimize uC-StartUp-time (skipping Bootloader):
//==============================================================================================

1.) Start the Arduino GUI (arduino.exe), opening this Sketch-file within the Arduino GUI
2.) In menu Tools/Board/Arduino AVR Boards ⇒ choose "Lilipad Arduino" (= chip ATmega328 running with internal-8MHz)
3.) Compile this Sketch with Hotkey [Ctrl+R]
4.) Find the output-directory within the black control-box (at bottom): anything like "arduino_build_[number]". (the number is generated @ each compiling-process)
5.) Therein is a file named "OLED-Clock-w-Led-and-Calendar.ino.hex " (this is the resutling compilation-file to upload later to the ATmega-chip through "ISP"-connection!)

6.) Download and install avrdude on https://download.savannah.gnu.org/releases/avrdude ⇐ there, search for a file named "avrdude-6.4-mingw32.zip"
    (the archive contains 3 files: avrdude.exe, avrdude.conf, libusb0.dll),
    Copy this 3 files in a program-directory of yuor choice, f.ex. C:\Program files\Avrdude\  (and add this path to Windows (search for how to do this))
7.) Connect a programmer-stick like "USBAsp" to any USB-Socket of your PC and the other side to the corresponding 6 ISP-sockets of the Clock pcb-Board,
    ⇒ important: the 6 ISP-Pins of the Programmer-Socket must match the same ISP-Pins on the clock-board! ⇒ see "ISP-Connector" on "OLED-Clock-6-nl-sch.png" schematic!)

8.) Fashing: Start a command promt ("cmd") in the previously found (temporary) Compiling-directory, where the resulting .hex-file resides,
    --------

      cmd [Enter]

There, on the resulting Command line, enter:

    //////////////////////////////////////////////////////////////////////////////////////////////////
    avrdude.exe -v -V -p m328p -c usbasp -B10 -e -D -U flash:w:OLED-Clock-w-Led-and-Calendar.ino.hex:i
    //////////////////////////////////////////////////////////////////////////////////////////////////

      !!! Att.: If on the command-prompt avrdude.exe can't be found, include the path to avrdude.exe, like: C:\Program files\Avrdude\avrdude.exe (...etc.)

You should then see an advancing asterix-line**** and finally the compiled result:
  ...
  ...
  avrdude: 22504 bytes of flash written
  avrdude: safemode: Fuses OK (E:FA, H:DA, L:FF)  //or similar fuse-values, depending on ATmega328-chip origin (using an original-chip or desoldering f.ex. a new Arduino-Pro-Mini...)
  avrdude done.  Thank you.


9) Fuses: Finally, the fuses of the uController must be set ⇒ 2 Alternatives: "Low-Batt." at 1.8V or at 2.7V, take the 1st. line:
   ------

    ///////////////////////////////////////////////////////////////////////////////////////////
    avrdude.exe -v -V -p m328p -c usbasp -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xfe:m      = "Brown-out" @ 1.8V) = OLED-"off" @ 1.8V = min.-voltage within OLED "on". Voltage = 1.8V-3.3V,
    avrdude.exe -v -V -p m328p -c usbasp -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xfd:m)     = "Brown-out" @ 2.7V) = OLED-"off" @ 2.7V = min.-voltage within OLED "on". Voltage = 2.7V-3.3V
    ///////////////////////////////////////////////////////////////////////////////////////////

Result:
  ...
  avrdude.exe: 1 bytes of efuse written

  avrdude.exe: safemode: hfuse reads as D9
  avrdude.exe: safemode: efuse reads as FD
  avrdude.exe: safemode: Fuses OK (E:FD, H:D9, L:E2)

  avrdude.exe done.  Thank you.

//-----------------------------------------------------------------------------------------------
// Flashing Done!
*/ //////////////////////////////////////////////////////////////////////////////////////////////
