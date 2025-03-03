![Dynamic YAML Badge](https://img.shields.io/badge/dynamic/yaml?url=https%3A%2F%2Fraw.githubusercontent.com%2Fdtomcat%2FpicoPromSD%2Fmain%2FFirmware%2FversionInfo.yml&query=%24.version&label=Latest%20Firmware&labelColor=orange)

# picoPromSD
EEPROM reader/writer and OG Xbox Seagate Password retrieval for the Original Xbox that also decodes the EEPROM data

What will it do?

1. Backup the Xbox's EEPROM and save it to microSD card.
2. Can write an EEPROM backup saved to µSD card to the Xbox's EEPROM.  (has some checks to help prevent writing a bad backup to the EEPROM)
3. Decrypt the EEPROM and save the important information in plain text to the µSD card.
4. Can calculate the HDD password while backing up the EEPROM (for OEM Seagate and WD drives)
		- Requires preparing an additional file.  Video of process: [Video of Process](https://youtu.be/JSXaQWXcwFo?si=bOLGbLG80aQxxA59)
5. For OEM Seagate drives (ALL), It can grab the HDD password directly from the drive itself.  No need for EEPROM or XBOX.  [Video of Process](https://youtu.be/1lWm_fr0Neo?si=kcM-wwt6UB56qRjm)
6. For OEM Seagate drives (ALL), It can unlock the HDD directly from the drive itself.  No need for EEPROM or XBOX.  [Video of Slim](https://www.youtube.com/shorts/Nwhjg9Vyvl8), [Video of RJ](https://www.youtube.com/shorts/swidWJ0N5yo)
7. More features may be added later.
<br><br><br>
This project is based on the work from Ryzee119's ArduinoProm:

>ArduinoProm. An Arduino based Original Xbox EEPROM reader and writer.
>It can be used to recover a HDD key etc. <br><br>
>ArduinoProm is inspired and adapted upon the awesome work by Grimdoomer on PiPROM, now achievable on a <$3 Arduino board.
>See https://github.com/grimdoomer/PiPROM for the Original Raspberry Pi version!
>  
>**Use at your own risk**
>

>The firmware has been written around the Arduino Pro Micro Leonardo (5V/16Mhz). However I would expect it to work on any Arduino with a built in USB >bootloader/Virtual Comport support, and obviously I2C support.
>1. Open `ArduinoProm.ino` in [Arduino IDE](https://www.arduino.cc/en/main/software)
>2. Connect your Arduino to your PC and setup the IDE. An example below <br> ![Arduino IDE preview](https://i.imgur.com/V7CJpkd.png)
>3. Hit the program button then confirm it compiles and programs successfully.
>
>
>By Ryzee119


This project uses code by Ryzee119, dx4m (from this [project](https://github.com/dx4m/Xbox-EEPROM-Utility)), and may contain code from various snippets from different forums (sorry lost my notes on who to thank).

This project is a continuation of my work from [ArdPromSD project](https://github.com/dtomcat/ArdPromSD).
This project was moved to an RP2040 MCU for more memory space.  This allowed me add the features I wanted to from the previous project, but ran out of program space to do so.  New features include better logging, eeprom decryption, as well as being able to retrieve the password from OEM seagate drives without the need for the original eeprom, original xbox, or even a PC!

SPECIAL THANKS!!!!<br>
Ryzee for his project above!<br>
Skye for all the HDD commands and knowledge! Invaluable addition to PPSD!<br>
Beta Testers:  Cato, Harcroft, Siktah