EESchema Schematic File Version 1
LIBS:power,device,conn,linear,regul,74xx,cmos4000,adc-dac,memory,xilinx,special,microcontrollers,dsp,microchip,analog_switches,motorola,texas,intel,audio,interface,digital-audio,philips,display,cypress,siliconi,contrib,valves
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
Wire Wire Line
	5550 2700 5650 2700
Connection ~ 6200 2950
Wire Wire Line
	6200 2400 6200 3450
Wire Wire Line
	6200 2400 5950 2400
Wire Wire Line
	5950 2400 5950 2500
Wire Wire Line
	6200 2950 6650 2950
Wire Wire Line
	6200 3450 6300 3450
Wire Wire Line
	6950 2750 7050 2750
Wire Wire Line
	7050 2750 7050 4000
Wire Wire Line
	7050 4000 7900 4000
Connection ~ 5600 4300
Wire Wire Line
	5600 4300 5600 3200
Wire Wire Line
	5600 3200 4200 3200
Wire Wire Line
	5350 4300 5900 4300
Wire Wire Line
	7900 4200 6750 4200
Wire Wire Line
	6750 4200 6750 4100
Wire Wire Line
	6750 4100 6400 4100
Connection ~ 6750 4300
Wire Wire Line
	6750 4450 6750 4300
Wire Wire Line
	4400 5200 4200 5200
Wire Wire Line
	4400 4800 4200 4800
Wire Wire Line
	4400 4400 4200 4400
Wire Wire Line
	4200 4000 4400 4000
Wire Wire Line
	4200 3800 4400 3800
Wire Wire Line
	4200 4200 4400 4200
Wire Wire Line
	4200 4600 4400 4600
Wire Wire Line
	4200 5000 4400 5000
Wire Wire Line
	6400 4300 7900 4300
Wire Wire Line
	6750 4950 6750 5050
Wire Wire Line
	6950 4100 7900 4100
Wire Wire Line
	4950 4300 4750 4300
Wire Wire Line
	4750 4300 4750 3100
Wire Wire Line
	4750 3100 4200 3100
Wire Wire Line
	4200 3300 5800 3300
Wire Wire Line
	5800 3300 5800 4100
Wire Wire Line
	5800 4100 5900 4100
Wire Wire Line
	7900 3900 6950 3900
Wire Wire Line
	6950 3900 6950 3150
Wire Wire Line
	6800 3450 6950 3450
Connection ~ 6950 3450
Wire Wire Line
	5950 3000 5950 2900
Wire Wire Line
	5050 2700 4950 2700
Wire Wire Line
	4950 2700 4950 3500
Wire Wire Line
	4950 3500 4200 3500
$Comp
L R R?
U 1 1 4A23E4A1
P 5300 2700
F 0 "R?" V 5380 2700 50  0000 C C
F 1 "R" V 5300 2700 50  0000 C C
	1    5300 2700
	0    1    1    0   
$EndComp
$Comp
L GND #PWR?
U 1 1 4A23E488
P 5950 3000
F 0 "#PWR?" H 5950 3000 30  0001 C C
F 1 "GND" H 5950 2930 30  0001 C C
	1    5950 3000
	1    0    0    -1  
$EndComp
$Comp
L NPN Q?
U 1 1 4A23E465
P 5850 2700
F 0 "Q?" H 6000 2700 50  0000 C C
F 1 "NPN" H 5752 2850 50  0000 C C
	1    5850 2700
	1    0    0    -1  
$EndComp
$Comp
L R R?
U 1 1 4A23E3FD
P 6550 3450
F 0 "R?" V 6630 3450 50  0000 C C
F 1 "1k" V 6550 3450 50  0000 C C
	1    6550 3450
	0    1    1    0   
$EndComp
$Comp
L MOSFET_P Q?
U 1 1 4A23DDD8
P 6850 2950
F 0 "Q?" H 6850 3140 60  0000 R C
F 1 "MOSFET_P" H 6850 2770 60  0000 R C
	1    6850 2950
	1    0    0    -1  
$EndComp
Text Label 4300 3300 0    40   ~
SCLK
Text Label 4300 3200 0    40   ~
SDATA <-
Text Label 4300 3100 0    40   ~
SDATA ->
Text Label 4300 3500 0    40   ~
POR
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
F 1 "1k" V 6750 4700 50  0000 C C
	1    6750 4700
	1    0    0    -1  
$EndComp
$Comp
L R R2
U 1 1 4A191C1A
P 6150 4300
F 0 "R2" V 6230 4300 50  0000 C C
F 1 "1k" V 6150 4300 50  0000 C C
	1    6150 4300
	0    1    1    0   
$EndComp
$Comp
L R R1
U 1 1 4A191C15
P 6150 4100
F 0 "R1" V 6230 4100 50  0000 C C
F 1 "1k" V 6150 4100 50  0000 C C
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
