Audio Test Signal Generator
========
##### (work in progress...)
Teensy3.1/3.2 based audio range test signal generator designed to aid building musical instruments and effects.    

#### Changelog

* V1.1
    - added reconfigurable I2S Fs, uses stock value (44.117kHz) for WAV player and doubles it in waveform generator and sine sweep modes (~88.2kHz)
    - reduced max frequency for the Ramp Up/Down waveforms to 8kHz. Above that value the aliasing was too strong
    - added a secondary output - the 12bit onboard DAC. It will be used to output the square and pulse waveforms in the waveform generator mode. The codec and it's built in anti aliasing filters does not reproduce sharp edges very well (ringing). For square/pulse wave the bit resolution does not really matter, the bandwidth does. D20 ouput pin will be used to switch the output signal between the codec DACs and the onboad 12bit DAC.
* V1.0 Initial release


[![AudioTestSignalGenerator](http://img.youtube.com/vi/fiGgEgc5klA/0.jpg)](http://www.youtube.com/watch?v=fiGgEgc5klA)

#### Features:  
1. Waveform generator:
    * 6 available waveforms:
        - Sine
        - Triangle
        - Square
        - Pulse
        - Ramp down
        - Ramp up
    * Frequency set in musical scale from C0 to B8 (16.35Hz-7.9kHz)
    * Few non musical/technical presets, like 100Hz, 1kHz...
2. Sine sweep generator
    * Frequency range: 16Hz - 22kHz
    * Sweep rate set in 10 available presets
    * Pause option
    * Sweep direction flip option
3. WAV file player:
    * up to 100 16bit 44.1kHz stereo wav files stored on an SD card
    * files are played in an endless loop
    * 10 quick access presets (wave 00 to 09)
4. Noise generator
    * White noise
    * Pink noise  
------
#### Hardware:  
* [Teensy3.1 or 3.2](https://www.pjrc.com/store/teensy32.html)  
* [Teensy Audio Shield](https://www.pjrc.com/store/teensy3_audio.html)  
* [Dev_LOOP](http://www.hexeguitar.com/diy/utility/devloop) - optional, use do control the output level and buffer the codec's line outputs.     
* 4x4 Keypad
* SSD1306 I2C OLED display
------
#### Software:  
Firmware has been written using [PlatformIO](http://platformio.org/) and Atom text editor.  
Installation procedure is avalilable [here](http://docs.platformio.org/en/latest/ide/atom.html#installation).  
The full project is available in the */firmware* directory.  

I have modified or upgraded a few libraries to get the planned features and make the best (or better vs stock Arduino libs) use of the hardware on the Teensy3:
1. Teensy Audio library by Paul Stoffregen
    - **data_waveforms.c** : added exponential look-up table used for exponential sweep
    - **synth_tonesweep** :
        * exponential frequency sweep
        * added sweep pause option
        * added sweep direction control
        * added sweep stop option
        * fixed a few bugs reported [here](https://forum.pjrc.com/threads/45246)
    - **synth_waveform:**   fixed a small bug, pulse waveform was generated at half of the set frequency
    - moved the **AudioStream.cpp and AudioStream.h** files to a local lib folder, so the changes will not interfere with the installed original library.
2. **SD.h** : Teensy optimization turned on
3. **Adafruit_SSD1306_t3.h** - uses i2c_t3 lib in DMA mode instad of stock Wire.h

Modified libraries are supplied with the project (*/lib*). There is no extra step needed to install them. PlatformIO will look for local libraries first when compiling the code.

![alt text][pic5]

### Teensy 3.1 pin mapping
```
0     Keypad col 3
1     Keypad col 2
2     Keypad col 1
3     Keypad col 0
4     Keypad row 3
5     Keypad row 2
6     SPI CD (SPI FLASH CS)
7     SPI MOSI (SD card)
8     ---
9     I2S BCLK (codec)
10    SPI CS (SD card)
11    I2S MCLK (codec)
12    SPI MISO (SD card)
13    I2S RX(codec)
14    SPI SCLK (SD card)
15    (A1) - free
16    Keypad row 1
17    Keypad row 0
18    I2C SDA
19    I2C SCL
20    output DAC relay/switch control
21    (A7) - free
22    I2S TX (codec)
23    I2S LRCLK (codec)
```
#### Prototype wiring:
![alt text][pic8]

**LINE OUT** is used as the main codec output.

------
### Keypad mapping
![alt text][pic1]
![alt text][pic2]
![alt text][pic3]
![alt text][pic4]
------
### WAV files
Device is capable pf playing up to 100 16bit/44.1kHz stereo WAV files. This number can be easily expanded in software in necessary. WAV files are stored on an micro SD card (up to 32GB - see [here](https://www.pjrc.com/store/teensy3_audio.html) for recommendations) and use the following naming scheme:

```
waveXX.wav

wave00.wav
wave01.wav
...
wave99.wav
```
where **XX** is the number in range 00 to 99.  

------
### Alternative ways to flash the Teensy   

In case you don't wan't to install PlatformIO a command line tool can be used to program Teensy3.1/3.2 with the provided firmware *hex* file.  
The teensy command line loader can be dowloaded directly from PRJC site:  
https://www.pjrc.com/teensy/loader_cli.html  
After dowmloading the software, unpack it to the folder where the hex file is stored and use the following command to upload the firmware into the MCU:
```
teensy_loader_cli -mmcu=mk20dx256 -w -v firmware.hex
```
![alt text][pic7]

Alternatively you can use the built in Arduino Teensy Loader to flash a custom hex file. Assuming you have installed the Arduino "IDE" and the Teensyduino follow these steps:
1. Open a new empty sketch in Arduino and set the board to Teensy 3.1/3.2.
2. Press upload, the sketch will be compiled and the Teensy Loader window will show up.
3. Click "File" and open the firmware.hex file included in the *hex* folder.
4. Press the Program button on the Teensy to start the upload.
5. Done!  

![alt text][pic6]

------

### To do:
- Check for potential bugs caused by the lack of SD card or init gone bad.
- Dedicated analog IO board for the Teensy Audio Shield.

------
(c) 07.2017 by Piotr Zapart  
www.hexeguitar.com

[pic1]: pics/WaveGen_keypad.png "WaveformGenerator"

[pic2]: pics/SinSweep_keypad.png "SinSweepGenerator"
[pic3]: pics/WavPlay_keypad.png "WavPlayer"
[pic4]: pics/NoiseGen_keypad.png "NoiseGenerator"
[pic5]: pics/block_diag.png "BlockDiagram"
[pic6]: pics/TennsyLoader.png "LoadCustomHex"
[pic7]: pics/TeensyCLI.png "Teensy CLI programming"
[pic8]: hardware/SchmV1.1.png "Prototype wiring"
