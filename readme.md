Audio Test Signal Generator
========

### Teensy3.1/3.2 based test signal generator

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
* [Dev_LOOP](http://www.hexeguitar.com/diy/utility/devloop)   
* 4x4 Keypad
* SSD1306 I2C OLED display
------
#### Software:  
Firmware has been written using [PlatformIO](http://platformio.org/) add-on for Atom.  
The full project is available in the */firmware* directory.  

I have modified or upgraded a few libraries to get the planned features and make the best (or better vs stock Arduino libs) use of the hardware on the Teensy3:
1. Teensy Audio library by Paul Stoffregen
    - **data_waveforms.c** : added exponential look-up table used for exponential sweep
    - **synth_tonesweep** :
        * exponential frequency sweep
        * added sweep pause option
        * added sweep direction control
        * added sweep stop option
    - **synth_waveform:**   fixed a small bug, pulse waveform was generated at half of the set frequency
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
20    (A6) - free
21    (A7) - free
22    I2S TX (codec)
23    I2S LRCLK (codec)
```
------
### Keypad mapping
![alt text][pic1]
![alt text][pic2]
![alt text][pic3]
![alt text][pic4]
------
### To do:
- Check for potential bugs caused by the lack of SD card or init gone bad.
- Square and Pulse waveforms - maybe use the onboard DAC instad of audio codec. One of the free pins could be used to drive a relay switching between codec and DAC outputs.

------
(c) 07.2017 by Piotr Zapart  
www.hexeguitar.com

[pic1]: pics/WaveGen_keypad.png "WaveformGenerator"
[pic2]: pics/SinSweep_keypad.png "SinSweepGenerator"
[pic3]: pics/WavPlay_keypad.png "WavPlayer"
[pic4]: pics/NoiseGen_keypad.png "NoiseGenerator"
[pic5]: pics/block_diag.png "BlockDiagram"
