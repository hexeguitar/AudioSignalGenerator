
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

const float noteFreqTable[9][12] =
{     //  C          C#         D          D#         E          F          F#         G          G#         A          A#         B
    {  16.3516,   17.3239,   18.3540,   19.4454,   20.6017,   21.8268,   23.1247,   24.4997,   25.9565,   27.5000,   29.1352,   30.8677}, //0
    {  32.7032,   34.6478,   36.7081,   38.8909,   41.2034,   43.6535,   46.2493,   48.9994,   51.9131,   55.0000,   58.2705,   61.7354}, //1
    {  65.4064,   69.2957,   73.4162,   77.7817,   82.4069,   87.3071,   92.4986,   97.9989,  103.8262,  110.0000,  116.5409,  123.4708}, //2
    { 130.8128,  138.5913,  146.8324,  155.5635,  164.8138,  174.6141,  184.9972,  195.9977,  207.6523,  220.0000,  233.0819,  246.9417}, //3
    { 261.6256,  277.1826,  293.6648,  311.1270,  329.6276,  349.2282,  369.9944,  391.9954,  415.3047,  440.0000,  466.1638,  493.8833}, //4
    { 523.2511,  554.3653,  587.3295,  622.2540,  659.2551,  698.4565,  739.9888,  783.9909,  830.6094,  880.0000,  932.3275,  987.7666}, //5
    {1046.5023, 1108.7305, 1174.6591, 1244.5079, 1318.5102, 1396.9129, 1479.9777, 1567.9817, 1661.2188, 1760.0000, 1864.6550, 1975.5332}, //6
    {2093.0045, 2217.4610, 2349.3181, 2489.0159, 2637.0205, 2793.8259, 2959.9554, 3135.9635, 3322.4376, 3520.0000, 3729.3101, 3951.0664}, //7
    {4186.0090, 4434.9221, 4698.6363, 4978.0317, 5274.0409, 5587.6517, 5919.9108, 6271.9270, 6644.8752, 7040.0000, 7458.6202, 7902.1328} //8
};

const char * noteNameTable[] =
{
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};
