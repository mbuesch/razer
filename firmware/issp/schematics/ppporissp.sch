EESchema Schematic File Version 1
LIBS:power,device,conn,linear,regul,74xx,cmos4000,adc-dac,memory,xilinx,special,microcontrollers,dsp,microchip,analog_switches,motorola,texas,intel,audio,interface,digital-audio,philips,display,cypress,siliconi,contrib,valves,./parport_issp.cache
EELAYER 23  0
EELAYER END
$Descr A4 11700 8267
Sheet 1 1
Title "Parallel port based M8C PowerOnReset-ISSP adapter"
Date "24 may 2009"
Rev "1"
Comp "(c) 2009 Michael Buesch <mb@bu3sch.de>"
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
Text Label 4300 3300 0    40   ~
SCLK
Text Label 4300 3200 0    40   ~
SDATA <-
Text Label 4300 3100 0    40   ~
SDATA ->
Text Label 4300 3500 0    40   ~
POR
Wire Wire Line
	5950 3000 5900 3000
Wire Wire Line
	5900 3000 5900 3500
Wire Wire Line
	5900 3500 4200 3500
Wire Wire Line
	6050 3350 6150 3350
Wire Wire Line
	7900 4000 6850 4000
Wire Wire Line
	6850 4000 6850 3200
Wire Wire Line
	5900 4100 5800 4100
Wire Wire Line
	5800 4100 5800 3300
Wire Wire Line
	5800 3300 4200 3300
Wire Wire Line
	4200 3100 4750 3100
Wire Wire Line
	4750 3100 4750 4300
Wire Wire Line
	4750 4300 4950 4300
Wire Wire Line
	6950 4100 7900 4100
Wire Wire Line
	6750 4950 6750 5050
Wire Wire Line
	6400 4300 7900 4300
Wire Wire Line
	4200 5000 4400 5000
Wire Wire Line
	4200 4600 4400 4600
Wire Wire Line
	4200 4200 4400 4200
Wire Wire Line
	4200 3800 4400 3800
Wire Wire Line
	7900 3900 7100 3900
Wire Wire Line
	4200 4000 4400 4000
Wire Wire Line
	4400 4400 4200 4400
Wire Wire Line
	4400 4800 4200 4800
Wire Wire Line
	4400 5200 4200 5200
Wire Wire Line
	6750 4450 6750 4300
Connection ~ 6750 4300
Wire Wire Line
	6400 4100 6750 4100
Wire Wire Line
	6750 4100 6750 4200
Wire Wire Line
	6750 4200 7900 4200
Wire Wire Line
	5350 4300 5900 4300
Wire Wire Line
	4200 3200 5600 3200
Wire Wire Line
	5600 3200 5600 4300
Connection ~ 5600 4300
Wire Wire Line
	6850 2800 6850 2700
Wire Wire Line
	6850 2700 7100 2700
Wire Wire Line
	7100 2700 7100 3900
Wire Wire Line
	6650 3350 6850 3350
Connection ~ 6850 3350
Wire Wire Line
	6450 3000 6550 3000
$Comp
L GND #PWR9
U 1 1 4A19241D
P 6050 3350
F 0 "#PWR9" H 6050 3350 30  0001 C C
F 1 "GND" H 6050 3280 30  0001 C C
	1    6050 3350
	0    1    1    0   
$EndComp
$Comp
L R R4
U 1 1 4A1923C7
P 6400 3350
F 0 "R4" V 6480 3350 50  0000 C C
F 1 "4.7k" V 6400 3350 50  0000 C C
	1    6400 3350
	0    1    1    0   
$EndComp
$Comp
L R R3
U 1 1 4A192356
P 6200 3000
F 0 "R3" V 6280 3000 50  0000 C C
F 1 "1k" V 6200 3000 50  0000 C C
	1    6200 3000
	0    1    1    0   
$EndComp
$Comp
L NPN Q1
U 1 1 4A1922BB
P 6750 3000
F 0 "Q1" H 6900 3000 50  0000 C C
F 1 "BC547" H 6652 3150 50  0000 C C
	1    6750 3000
	1    0    0    -1  
$EndComp
NoConn ~ 4200 5300
NoConn ~ 4200 5100
NoConn ~ 4200 4900
NoConn ~ 4200 4700
NoConn ~ 4200 4500
NoConn ~ 4200 4300
NoConn ~ 4200 4100
NoConn ~ 4200 3900
NoConn ~ 4200 3700
NoConn ~ 4200 3600
NoConn ~ 4200 3400
NoConn ~ 4200 3000
NoConn ~ 4200 2900
$Comp
L GND #PWR11
U 1 1 4A1921BA
P 6950 4100
F 0 "#PWR11" H 6950 4100 30  0001 C C
F 1 "GND" H 6950 4030 30  0001 C C
	1    6950 4100
	0    1    1    0   
$EndComp
$Comp
L GND #PWR10
U 1 1 4A19218B
P 6750 5050
F 0 "#PWR10" H 6750 5050 30  0001 C C
F 1 "GND" H 6750 4980 30  0001 C C
	1    6750 5050
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR8
U 1 1 4A191DAF
P 4400 5200
F 0 "#PWR8" H 4400 5200 30  0001 C C
F 1 "GND" H 4400 5130 30  0001 C C
	1    4400 5200
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR7
U 1 1 4A191DA8
P 4400 5000
F 0 "#PWR7" H 4400 5000 30  0001 C C
F 1 "GND" H 4400 4930 30  0001 C C
	1    4400 5000
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR6
U 1 1 4A191DA5
P 4400 4800
F 0 "#PWR6" H 4400 4800 30  0001 C C
F 1 "GND" H 4400 4730 30  0001 C C
	1    4400 4800
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR5
U 1 1 4A191DA1
P 4400 4600
F 0 "#PWR5" H 4400 4600 30  0001 C C
F 1 "GND" H 4400 4530 30  0001 C C
	1    4400 4600
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR4
U 1 1 4A191D9B
P 4400 4400
F 0 "#PWR4" H 4400 4400 30  0001 C C
F 1 "GND" H 4400 4330 30  0001 C C
	1    4400 4400
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR3
U 1 1 4A191D96
P 4400 4200
F 0 "#PWR3" H 4400 4200 30  0001 C C
F 1 "GND" H 4400 4130 30  0001 C C
	1    4400 4200
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR2
U 1 1 4A191D92
P 4400 4000
F 0 "#PWR2" H 4400 4000 30  0001 C C
F 1 "GND" H 4400 3930 30  0001 C C
	1    4400 4000
	0    -1   -1   0   
$EndComp
$Comp
L GND #PWR1
U 1 1 4A191D8C
P 4400 3800
F 0 "#PWR1" H 4400 3800 30  0001 C C
F 1 "GND" H 4400 3730 30  0001 C C
	1    4400 3800
	0    -1   -1   0   
$EndComp
Text Label 7150 4000 0    40   ~
Vdd to device
Text Label 7150 3900 0    40   ~
Vdd from supply
Text Label 7150 4100 0    40   ~
GND
Text Label 7150 4200 0    40   ~
SCLK
Text Label 7150 4300 0    40   ~
SDATA
$Comp
L CONN_5 P1
U 1 1 4A191C5C
P 8300 4100
F 0 "P1" V 8250 4100 50  0000 C C
F 1 "ISSP" V 8350 4100 50  0000 C C
	1    8300 4100
	1    0    0    -1  
$EndComp
$Comp
L DIODE D1
U 1 1 4A191C2F
P 5150 4300
F 0 "D1" H 5150 4400 40  0000 C C
F 1 "1N4148" H 5150 4200 40  0000 C C
	1    5150 4300
	1    0    0    -1  
$EndComp
$Comp
L R R5
U 1 1 4A191C1E
P 6750 4700
F 0 "R5" V 6830 4700 50  0000 C C
F 1 "1.5k" V 6750 4700 50  0000 C C
	1    6750 4700
	1    0    0    -1  
$EndComp
$Comp
L R R2
U 1 1 4A191C1A
P 6150 4300
F 0 "R2" V 6230 4300 50  0000 C C
F 1 "1.5k" V 6150 4300 50  0000 C C
	1    6150 4300
	0    1    1    0   
$EndComp
$Comp
L R R1
U 1 1 4A191C15
P 6150 4100
F 0 "R1" V 6230 4100 50  0000 C C
F 1 "1.5k" V 6150 4100 50  0000 C C
	1    6150 4100
	0    1    1    0   
$EndComp
$Comp
L DB25 J1
U 1 1 4A191BE0
P 3750 4100
F 0 "J1" H 3800 5450 70  0000 C C
F 1 "PC parallel port" H 3700 2750 70  0000 C C
	1    3750 4100
	-1   0    0    1   
$EndComp
$EndSCHEMATC
