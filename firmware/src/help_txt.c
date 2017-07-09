
/* ### Audio Test Signal Generator #############################################
 * Copyright (c) 2017, Piotr Zapart, www.hexeguitar.com
 *
 * Caution!!!
 * Program uses modified libraries:
 * - Teensy Audio library by Paul Stoffregen
 *          - data_waveforms.c : added exponential look-up table
 *          - synth_tonesweep :
 *                              1. exponential frequency sweep
 *                              2. added sweep pause option
 *                              3. added sweep direction control
 *                              4. added sweep stop option
 *          - synth_waveform:   fixed a small bug, pulse waveform was generated
 *                              at half of the set frequency
 * - SD.h : Teensy optimization turned on
 * - Adafruit_SSD1306_t3.h - uses i2c_t3 lib in DMA mode instad of stock Wire.h
 *
 * Modified libraries are provide in the /lib directory.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdint.h>


const char * help_txt[]=
{
    "A: Waveform generator\n\n1:100Hz   2:Oct Up\n3: 1kHz   8:Oct Down\n7: 5kHz   6:Note Up\n9:10kHz   4:Note Down\n0:22kHz   5:A=440Hz\n*:Wave    #:Duty",
    "B: Sine Sweep\n\n0-9: Start sweep\nNumber sets the speed\n*:Pause\n#:Flip direction",
    "C: Wav file player\n\n44.1kHz stereo 16bit\nnaming: waveXX.wav\nXX=00-99\n0-9:Play file 00-09\n*:Enter new file No\n#:Stop/Mute",
    "D: Noise generator\n\n1:White noise\n2:Pink noise\n#:Mute/OFF"
};
