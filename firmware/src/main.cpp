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
                                5. fixed a few bugs https://forum.pjrc.com/threads/45246
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

#include <AudioStream.h>
#include <Audio.h>
#include <i2c_t3.h>
#include <SPI.h>
#include <SD.h>                         //Teensy optimization code enabled
#include <SerialFlash.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306_t3.h>        //using i2c_t3 lib instead of wire.h
#include <TimerOne.h>
#include <EEPROM.h>


// help_txt.c & freq_table.c & symbols_bmp.h
extern "C"
{
    extern const char * help_txt[];
    extern const float noteFreqTable[10][12];
    extern const char * noteNameTable[];
    extern const uint8_t waveSymbols[];
    extern const uint8_t welcomeScrn[];
}
/*##############################################################################
  ### Teensy 3.1 pin mapping ###
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
 10     SPI CS (SD card)
 11     I2S MCLK (codec)
 12     SPI MISO (SD card)
 13     I2S RX(codec)
 14     SPI SCLK (SD card)
 15     (A1) - free
 16     Keypad row 1
 17     Keypad row 0
 18     I2C SDA
 19     I2C SCL
 20     (A6) - free
 21     (A7) - free
 22     I2S TX (codec)
 23     I2S LRCLK (codec)
*/
//##############################################################################
// ### state machine ###
typedef enum
{
    SD_WAV_PLAY,
    SIG_GEN,
    NOISE_GEN,
    SIN_SWEEP
}engineState_t;

engineState_t engineState = SIG_GEN;
bool SDinitComplete = false;
bool helpMode = false;
//##############################################################################
// ### Mixer output channels ###
typedef enum
{
    MUTE_ALL,
    WAV_PLAYER,
    SIGNAL_GEN,
    WHITE_NOISE,
    PINK_NOISE,
    SINUS_SWEEP
}outputChannel_t;
// available output channels:
#define WAV_PLAY_CH     0
#define WAV_PLAY_CHSDA  0       //wav player A (SD mixer)
#define WAV_PLAY_CHSDB  1       //wav player B (SD mixer)

#define SIG_GEN_CH      1
#define WHITE_NOISE_CH  2
#define SIN_SWEEP_CH    3
#define PINK_NOISE_CH   0       //output mixer: pink noise ch = wav player ch
#define PINK_NOISE_CHSD 2       //SD out mixer: pin noise ch = 2

#define RELAY_CTRL      20      //relay switching the output
                                //between the codec and 12bit DAC
//##############################################################################
// ### Display settings ###
#define DISP_TXT_ROW0   20
#define DISP_TXT_ROW1   35
#define DISP_TXT_ROW2   50
#define DISP_TXT_COL0   0
#define DISP_TXT_COL1   63

const char * engineStateTxt[] =
{
    "WAV PLAYER","SIG GEN","NOISE","SIN SWEEP"
};
//##############################################################################
// ### WAV player ###

#define WAVE_NO_01  5
#define WAVE_NO_10  4
#define XFADE_SIZE  40
#define MAX_WAV_FILES   99

// Use these with the audio adaptor board
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

typedef enum            //used to trigger the next play in looper mode
{
    PLAYER_NONE,
    PLAYER_A,
    PLAYER_B
}nextPlayer_t;
static volatile nextPlayer_t nextToPlay;

typedef enum
{
    WAV_PLAYER_A,
    WAV_PLAYER_B
} wavPlayerNo_t;
//used manual wav number input
typedef enum
{
    SETNAME_OFF,
    SETNAME_ONES,
    SETNAME_TENS,
}fileNameEdit_t;
fileNameEdit_t fileNameEditMode = SETNAME_OFF;

char currentFile[] = "wave00.wav";
//##############################################################################
// ### SIGnal Generator ###
#define SIGGEN_PHASE_DEFAULT    0
#define SIGGEN_MAX_OCTAVE       8
#define SIGGEN_AMPL_DEFAULT     1
#define SIGGEN_F_LIMIT          8000
#define NOTE_C                  0
#define NOTE_B                  11
#define NOTE_A                  9
#define FREQ_TECH_OCT           9
#define MAX_WAVEFORMS           6

const uint8_t waveIndex[MAX_WAVEFORMS] = {
                                WAVEFORM_SINE,                  //SIN 0
                                WAVEFORM_TRIANGLE,              //TRI 1
                                WAVEFORM_SQUARE,                //SQR 2
                                WAVEFORM_PULSE,                 //PUL 3
                                WAVEFORM_SAWTOOTH,              //RDN 4
                                WAVEFORM_SAWTOOTH_REVERSE       //RUP 5
                            };
uint8_t sigGen_note = NOTE_A;   //default starting value
uint8_t sigGen_oct = 4;         //default octave is 4
short sigGen_wave = 0;
float sigGen_duty = 1.0;
float sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
//##############################################################################
// ### Sin Sweep Generator ###
#define SINSWEEP_DIR_UP     1
#define SINSWEEP_DIR_DWN    -1

const int sinSweepStartF = 16;
const int sinSweepEndF = 22000;
const float sinSweepLvl = 1.0;
const float sinSweepTime_ms[10] =
{
      0.1, 0.2, 0.5,   1,   2,
       10,  20,  50, 100, 200
};
int sinSweepDir = SINSWEEP_DIR_UP;
//##############################################################################
// ### 4x4 keypad config ###
const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {17, 16, 5, 4}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {3, 2, 1, 0}; //connect to the column pinouts of the keypad
volatile char key;

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );
//##############################################################################
// function declarations

void handleKeyPress(char key);                                  //main key press handler
bool keypadHandleWavPlayer(char key);                           //wav player key press handler

bool mixerSetChannel(outputChannel_t ch);

bool setSigGen(float freq, short waveform);                     //mode A - Waveform generator
bool playSinSweep(float time_ms, int dir);                      //mode B - Sine Sweep
bool playFile(const char *filename, wavPlayerNo_t playerNo);    //mode C - WAV player
void playNewFile(void);
void wavLooper(void);
bool setFileName(char waveNo);
bool setFileNameDigit(uint8_t value, uint8_t mask);

void displayClrMainArea(void);                                  //display functions
void displayMainArea(void);
void displayNote(uint8_t octave, uint8_t note);
void displayFreqWarning(void);
void displayWaveSymbol(uint8_t wave);
void displayWavProgress(void);
void displayFileName(void);
void displayMode(void);
void displayNoiseGen(void);
void displaySinSweep(void);
void displayArrow(int dir);
void displayBargraph(int min, int max, int value);
void displayUpdate(void);
void displayHelpTxt(uint8_t mode);
void displayStartScreen(engineState_t mode);

void ISR_checkWavePosition(void);
//##############################################################################
// GUItool: begin automatically generated code
AudioSynthNoisePink      pink;           //xy=419,310
AudioPlaySdWav           playSdWavB;     //xy=431,193
AudioPlaySdWav           playSdWavA;     //xy=432,120
AudioMixer4              mixerSD_R;      //xy=645,205
AudioMixer4              mixerSD_L;      //xy=647,133
AudioSynthToneSweep      tonesweep;      //xy=651,353
AudioSynthWaveform       wave;           //xy=656,263
AudioSynthNoiseWhite     noise;          //xy=658,311
AudioMixer4              mixerL;         //xy=962,230
AudioMixer4              mixerDAC; //xy=963,158
AudioMixer4              mixerR;         //xy=965,302
AudioOutputI2S           i2s;            //xy=1141,228
AudioOutputAnalog        dac12;           //xy=1142,178
AudioConnection          patchCord1(pink, 0, mixerSD_L, 2);
AudioConnection          patchCord2(pink, 0, mixerSD_R, 2);
AudioConnection          patchCord3(playSdWavB, 0, mixerSD_L, 1);
AudioConnection          patchCord4(playSdWavB, 1, mixerSD_R, 1);
AudioConnection          patchCord5(playSdWavA, 0, mixerSD_L, 0);
AudioConnection          patchCord6(playSdWavA, 1, mixerSD_R, 0);
AudioConnection          patchCord7(mixerSD_R, 0, mixerR, 0);
AudioConnection          patchCord8(mixerSD_L, 0, mixerL, 0);
AudioConnection          patchCord9(tonesweep, 0, mixerL, 3);
AudioConnection          patchCord10(tonesweep, 0, mixerR, 3);
AudioConnection          patchCord11(wave, 0, mixerL, 1);
AudioConnection          patchCord12(wave, 0, mixerR, 1);
AudioConnection          patchCord13(wave, 0, mixerDAC, 0);
AudioConnection          patchCord14(noise, 0, mixerL, 2);
AudioConnection          patchCord15(noise, 0, mixerR, 2);
AudioConnection          patchCord16(mixerL, 0, i2s, 0);
AudioConnection          patchCord17(mixerDAC, dac12);
AudioConnection          patchCord18(mixerR, 0, i2s, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=1140,282
// GUItool: end automatically generated code


//##############################################################################
// ### 128x64 SSD1306 OLED config ###
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

//##############################################################################
// ### Function definitions ###
//##############################################################################
// ### onboard 12bit DAC update frequency -
// I2S Fs update            by Frank B (https://forum.pjrc.com/threads/38753)
// WAV player uses standard 44117...Hz Fs
// Waveform generator (A) and Sine Sweep use 2x44117Hz setiing
void setDACFreq(int freq)
{
    const unsigned config = PDB_SC_TRGSEL(15) | PDB_SC_PDBEN | PDB_SC_CONT | PDB_SC_PDBIE | PDB_SC_DMAEN;
    PDB0_IDLY = 1;
    PDB0_MOD = round((float)F_BUS / freq ) - 1;
    PDB0_SC = config | PDB_SC_LDOK;
    PDB0_SC = config | PDB_SC_SWTRIG;
    PDB0_CH0C1 = 0x0101;
}
//##############################################################################
void SGT_setFs(uint32_t freq)
{
    //SGTL5000 sampling freq reconfiguration, CHIP_CLK_CTRL register @ 0x0004
    const int CHIP_CLK_CTRL_addr = 0x0004;      //register address
    uint16_t regValue = 0x000C;                 //default library value for 44.1kHz, 256*Fs 0b00 00 01 00
    if (freq == 44117)          regValue = 0x0004;
    else if (freq == 44117*2)   regValue = 0x000C;

    Wire.beginTransmission(0x0A);       //stock SGTL5000 I2C  slave address
    Wire.write(CHIP_CLK_CTRL_addr >> 8);
    Wire.write(CHIP_CLK_CTRL_addr);
    Wire.write(regValue>>8);
    Wire.write(regValue);
    Wire.endTransmission();
}

void setI2SFreq(uint32_t freq)
{
    typedef struct
    {
        uint8_t mult;
        uint16_t div;
    } tmclk;
    const int numfreqs = 14;
    const uint32_t samplefreqs[numfreqs] = { 8000, 11025, 16000, 22050, 32000, 44100, 44117, 48000, 88200, 44117 * 2, 96000, 176400, 44117 * 4, 192000}; //(changed float values to uint)
    #if (F_PLL==16000000)
    const tmclk clkArr[numfreqs] = {{16, 125}, {148, 839}, {32, 125}, {145, 411}, {64, 125}, {151, 214}, {12, 17}, {96, 125}, {151, 107}, {24, 17}, {192, 125}, {127, 45}, {48, 17}, {255, 83} };
    #elif (F_PLL==72000000)
    const tmclk clkArr[numfreqs] = {{32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {128, 1125}, {98, 625}, {8, 51}, {64, 375}, {196, 625}, {16, 51}, {128, 375}, {249, 397}, {32, 51}, {185, 271} };
    #elif (F_PLL==96000000)
    const tmclk clkArr[numfreqs] = {{8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {32, 375}, {147, 1250}, {2, 17}, {16, 125}, {147, 625}, {4, 17}, {32, 125}, {151, 321}, {8, 17}, {64, 125} };
    #elif (F_PLL==120000000)
    const tmclk clkArr[numfreqs] = {{32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {128, 1875}, {205, 2179}, {8, 85}, {64, 625}, {89, 473}, {16, 85}, {128, 625}, {178, 473}, {32, 85}, {145, 354} };
    #elif (F_PLL==144000000)
    const tmclk clkArr[numfreqs] = {{16, 1125}, {49, 2500}, {32, 1125}, {49, 1250}, {64, 1125}, {49, 625}, {4, 51}, {32, 375}, {98, 625}, {8, 51}, {64, 375}, {196, 625}, {16, 51}, {128, 375} };
    #elif (F_PLL==180000000)
    const tmclk clkArr[numfreqs] = {{46, 4043}, {49, 3125}, {73, 3208}, {98, 3125}, {183, 4021}, {196, 3125}, {16, 255}, {128, 1875}, {107, 853}, {32, 255}, {219, 1604}, {214, 853}, {64, 255}, {219, 802} };
    #elif (F_PLL==192000000)
    const tmclk clkArr[numfreqs] = {{4, 375}, {37, 2517}, {8, 375}, {73, 2483}, {16, 375}, {147, 2500}, {1, 17}, {8, 125}, {147, 1250}, {2, 17}, {16, 125}, {147, 625}, {4, 17}, {32, 125} };
    #elif (F_PLL==216000000)
    const tmclk clkArr[numfreqs] = {{32, 3375}, {49, 3750}, {64, 3375}, {49, 1875}, {128, 3375}, {98, 1875}, {8, 153}, {64, 1125}, {196, 1875}, {16, 153}, {128, 1125}, {226, 1081}, {32, 153}, {147, 646} };
    #elif (F_PLL==240000000)
    const tmclk clkArr[numfreqs] = {{16, 1875}, {29, 2466}, {32, 1875}, {89, 3784}, {64, 1875}, {147, 3125}, {4, 85}, {32, 625}, {205, 2179}, {8, 85}, {64, 625}, {89, 473}, {16, 85}, {128, 625} };
    #endif
    AudioNoInterrupts();  // disable audio library momentarily
    for (int f = 0; f < numfreqs; f++)
    {
        if ( freq == samplefreqs[f] )
        {
            while (I2S0_MCR & I2S_MCR_DUF) ;
            I2S0_MDR = I2S_MDR_FRACT((clkArr[f].mult - 1)) | I2S_MDR_DIVIDE((clkArr[f].div - 1));
            return;
            SGT_setFs(freq);
        }
    }
    AudioInterrupts();  //
}
//##############################################################################
//### handle keypad ###
void keypadEvent(KeypadEvent key)
{
  switch (keypad.getState())
  {
    case IDLE:      break;
    case PRESSED:
                   if (helpMode == true)
                   {
                       helpMode = false;
                       displayMainArea();
                       displayStartScreen(engineState);
                       key = NO_KEY;        //use the press to exit help mode only
                   }
    	           handleKeyPress(key);
                   break;
    case RELEASED:
                    break;
    case HOLD:
                    helpMode = true;
                    switch (key)
                    {
                        case 'A':
                                    displayHelpTxt(0);
                                    break;
                        case 'B':
                                    displayHelpTxt(1);
                                    break;
                        case 'C':
                                    displayHelpTxt(2);
                                    break;
                        case 'D':
                                    displayHelpTxt(3);
                                    break;
                    }
                    break;
    }
}
//##############################################################################
void handleKeyPress(char key)
{
    switch(key)
    {
        case NO_KEY:
                break;
        case 'A':
                engineState = SIG_GEN;
                fileNameEditMode = SETNAME_OFF;     //exit fle name edit mode
                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                setSigGen(sigGen_freq, sigGen_wave);
                setI2SFreq(44117 * 2);
                mixerSetChannel(MUTE_ALL);
                Timer1.stop();
                displayStartScreen(SIG_GEN);
                break;
        case 'B':
                engineState = SIN_SWEEP;
                fileNameEditMode = SETNAME_OFF;     //exit fle name edit mode
                mixerSetChannel(MUTE_ALL);
                setI2SFreq(44117 * 2);
                Timer1.stop();
                displayStartScreen(SIN_SWEEP);
                break;
        case 'C':
                engineState = SD_WAV_PLAY;
                fileNameEditMode = SETNAME_OFF;     //exit fle name edit mode
                mixerSetChannel(MUTE_ALL);
                setI2SFreq(44117);
                displayMode();
                displayClrMainArea();
                if (SDinitComplete==false)
                {
                    if (!(SD.begin(SDCARD_CS_PIN)))
                    {
                        SDinitComplete = false;
                        displayClrMainArea();
                        display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
                        display.println("SD access failed");
                        display.println("Switching back to ");
                        display.println("Sig Gen in 2 sec...");
                        display.display();
                        delay(2000);
                        engineState = SIG_GEN;
                        displayStartScreen(SIG_GEN);
                    }
                    else
                    {
                        SDinitComplete = true;
                        displayStartScreen(SD_WAV_PLAY);
                        Timer1.start();
                    }
                }
                else
                {
                    displayStartScreen(SD_WAV_PLAY);
                    Timer1.start();     //start timer1 = tracking the end of a wav file
                }
                break;
        case 'D':
                engineState = NOISE_GEN;
                fileNameEditMode = SETNAME_OFF;     //exit fle name edit mode
                setI2SFreq(44117);
                mixerSetChannel(MUTE_ALL);
                Timer1.stop();
                displayStartScreen(NOISE_GEN);
                break;
        case '1':
                switch(engineState)
                {
                    case NOISE_GEN:
                                mixerSetChannel(WHITE_NOISE);
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[0], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:   //KEY1 preset
                                sigGen_oct = FREQ_TECH_OCT;
                                sigGen_note = 0;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        case '2':
                switch(engineState)
                {
                    case NOISE_GEN:
                                mixerSetChannel(PINK_NOISE);
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[1], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:       //increase octave
                                sigGen_oct++;
                                if (sigGen_oct > SIGGEN_MAX_OCTAVE) sigGen_oct = SIGGEN_MAX_OCTAVE; //limiter
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                setSigGen(sigGen_freq, sigGen_wave);
                                mixerSetChannel(SIGNAL_GEN);
                                displayClrMainArea();
                                displayNote(sigGen_oct, sigGen_note);
                                break;
                    default:
                                break;
                }
                break;
        case '3':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[2], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:   //1kHz preset
                                sigGen_oct = FREQ_TECH_OCT;
                                sigGen_note = 1;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        case '4':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[3], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:       //decrease note
                                if (sigGen_note) sigGen_note--;
                                if (sigGen_note == NOTE_C && sigGen_oct)
                                {
                                    sigGen_note = NOTE_B;
                                    sigGen_oct--;
                                }
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                setSigGen(sigGen_freq, sigGen_wave);
                                mixerSetChannel(SIGNAL_GEN);
                                displayClrMainArea();
                                displayNote(sigGen_oct, sigGen_note);
                                break;
                    default:
                                break;
                }
                break;
        case '5':
        switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[4], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:
                                sigGen_note = NOTE_A;
                                sigGen_oct = 4;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                setSigGen(sigGen_freq, sigGen_wave);
                                mixerSetChannel(SIGNAL_GEN);
                                displayClrMainArea();
                                displayNote(sigGen_oct, sigGen_note);
                                break;
                    default:
                                break;
                }
                break;
        case '6':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[5], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:       //increase note
                                sigGen_note++;
                                if (sigGen_note>NOTE_B)
                                {
                                    if (sigGen_oct<SIGGEN_MAX_OCTAVE)
                                    {
                                        sigGen_note = NOTE_C;
                                        sigGen_oct++;
                                    }
                                    else    sigGen_note = NOTE_B;
                                }
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                setSigGen(sigGen_freq, sigGen_wave);
                                mixerSetChannel(SIGNAL_GEN);
                                displayClrMainArea();
                                displayNote(sigGen_oct, sigGen_note);
                                break;
                    default:
                                break;
                }
                break;
        case '7':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[6], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:   //5kHz preset
                                sigGen_oct = FREQ_TECH_OCT;
                                sigGen_note = 2;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        case '8':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[7], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:       //decrease octave
                                if (sigGen_oct) sigGen_oct--;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                setSigGen(sigGen_freq, sigGen_wave);
                                mixerSetChannel(SIGNAL_GEN);
                                displayClrMainArea();
                                displayNote(sigGen_oct, sigGen_note);
                                break;
                    default:
                                break;
                }
                break;
        case '9':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[8], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:   //10kHz preset
                                sigGen_oct = FREQ_TECH_OCT;
                                sigGen_note = 3;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        case '0':
                switch(engineState)
                {
                    case NOISE_GEN:
                                break;
                    case SIN_SWEEP:
                                mixerSetChannel(SINUS_SWEEP);
                                playSinSweep(sinSweepTime_ms[9], sinSweepDir);
                                break;
                    case SD_WAV_PLAY:
                                keypadHandleWavPlayer(key);
                                break;
                    case SIG_GEN:   //22kHz preset
                                sigGen_oct = FREQ_TECH_OCT;
                                sigGen_note = 4;
                                sigGen_freq = noteFreqTable[sigGen_oct][sigGen_note];
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        case '#':
                switch(engineState)
                {
                    case NOISE_GEN:
                                mixerSetChannel(MUTE_ALL);
                                break;
                    case SIN_SWEEP:
                                if (tonesweep.isPlaying())      //in play mode
                                {
                                    tonesweep.flipDir();
                                    sinSweepDir *= -1;        //flip the sweep direction
                                }
                                else
                                {
                                    sinSweepDir *= -1;        //flip the sweep direction
                                    tonesweep.setSweepDir(sinSweepDir);  //set the internal dir ctrl to default
                                }

                                displayArrow(tonesweep.getSweepDir());

                                break;
                    case SD_WAV_PLAY:
                                playSdWavA.stop();
                                playSdWavB.stop();
                                displayStartScreen(SD_WAV_PLAY);
                                break;
                    case SIG_GEN:
                                sigGen_duty += 0.1;
                                if (sigGen_duty>1.0)    sigGen_duty = 0.1;
                                wave.pulseWidth(sigGen_duty);
                                break;
                    default:
                                break;
                }
                break;
        case '*':
                switch(engineState)
                {
                    case NOISE_GEN:
                                mixerSetChannel(MUTE_ALL);
                                break;
                    case SIN_SWEEP:
                                tonesweep.pause(tonesweep.getPause()^1);
                                break;
                    case SD_WAV_PLAY:               //manual name input
                                switch (fileNameEditMode)
                                {
                                    case SETNAME_OFF:
                                                    playSdWavA.stop();
                                                    playSdWavB.stop();
                                                    displayClrMainArea();
                                                    display.setCursor(DISP_TXT_COL0, DISP_TXT_ROW0);
                                                    display.print("Play file NO: ");
                                                    display.setCursor(DISP_TXT_COL0, DISP_TXT_ROW2+5);
                                                    display.print("Press * to play");
                                                    fileNameEditMode = SETNAME_TENS;
                                                    break;
                                    case SETNAME_ONES:
                                    case SETNAME_TENS:
                                                    fileNameEditMode = SETNAME_OFF;
                                                    displayClrMainArea();
                                                    displayFileName();
                                                    playNewFile();
                                                    break;
                                }
                                break;
                    case SIG_GEN:                   //scroll through wavefoprms, arb not used
                                sigGen_wave++;
                                if (sigGen_wave > MAX_WAVEFORMS-1)
                                {
                                    sigGen_wave = 0;
                                }
                                displayWaveSymbol(sigGen_wave);
                                if (setSigGen(sigGen_freq, sigGen_wave))
                                {
                                    mixerSetChannel(SIGNAL_GEN);
                                    displayClrMainArea();
                                    displayNote(sigGen_oct, sigGen_note);
                                }
                                else
                                {
                                    mixerSetChannel(MUTE_ALL);
                                    displayClrMainArea();
                                    displayFreqWarning();
                                }
                                break;
                    default:
                                break;
                }
                break;
        default:
                break;
    }
}
//##############################################################################
bool keypadHandleWavPlayer(char key)
{
    key = key - '0';            //get numerical value
    if (key>9)  return false;

    switch (fileNameEditMode)
    {
        case SETNAME_OFF:
                        setFileName(key);
                        playNewFile();
                        displayFileName();
                        break;
        case SETNAME_ONES:
                        setFileNameDigit(key, 0b01);
                        fileNameEditMode=SETNAME_TENS;
                        break;
        case SETNAME_TENS:
                        setFileNameDigit(key, 0b10);
                        fileNameEditMode=SETNAME_ONES;
                        break;
    }
    return true;
}
//##############################################################################
//### set mixer channel ###
/*
    Turns on one channel, mutes the rest
    ch = 0 -> mute all channels
*/
bool mixerSetChannel(outputChannel_t ch)
{
    bool out = true;       //possible future uses
    byte i;
    AudioNoInterrupts();  // disable audio library momentarily
    switch (ch)
    {
        case MUTE_ALL:  //all channels OFF
                        digitalWrite(RELAY_CTRL,LOW);
                        for (i = 0;i<4;i++)
                        {
                            mixerL.gain(i,0);
                            mixerR.gain(i,0);
                            mixerSD_L.gain(i,0);
                            mixerSD_R.gain(i,0);
                            mixerDAC.gain(i,0);
                        }
                        noise.amplitude(0); //switch off
                        pink.amplitude(0);  //all sources
                        playSdWavA.stop();
                        playSdWavB.stop();
                        tonesweep.stop();
                        out = true;
                        break;
        case WAV_PLAYER:
                        mixerSetChannel(MUTE_ALL);
                        mixerSD_R.gain(WAV_PLAY_CHSDA,1);   //----
                        mixerSD_R.gain(WAV_PLAY_CHSDB,1);   //SD mixer
                        mixerSD_L.gain(WAV_PLAY_CHSDA,1);   //
                        mixerSD_L.gain(WAV_PLAY_CHSDB,1);   //----
                        mixerR.gain(WAV_PLAY_CH,1);         //output
                        mixerL.gain(WAV_PLAY_CH,1);         //mixer
                        break;
        case SIGNAL_GEN:
                        mixerSetChannel(MUTE_ALL);
                        mixerR.gain(SIG_GEN_CH,1);
                        mixerL.gain(SIG_GEN_CH,1);

                        mixerDAC.gain(0,1);    //use 12bit DAC

                        // if (sigGen_wave == 2 || sigGen_wave == 3)   //SQR or pulse
                        // {
                        //     mixerDAC.gain(0,1);    //use 12bit DAC
                        //     digitalWrite(RELAY_CTRL,HIGH);  //relay on
                        // }

                        break;
        case WHITE_NOISE:
                        mixerSetChannel(MUTE_ALL);
                        noise.amplitude(1); //turn on noise gen
                        mixerR.gain(WHITE_NOISE_CH,1);
                        mixerL.gain(WHITE_NOISE_CH,1);
                        break;
        case PINK_NOISE:
                        mixerSetChannel(MUTE_ALL);
                        pink.amplitude(1);
                        mixerSD_R.gain(PINK_NOISE_CHSD,1);   //SD
                        mixerSD_L.gain(PINK_NOISE_CHSD,1);   //mixer
                        mixerR.gain(PINK_NOISE_CH,1);        //output
                        mixerL.gain(PINK_NOISE_CH,1);        //mixer
                        break;
        case SINUS_SWEEP:
                        mixerSetChannel(MUTE_ALL);
                        mixerR.gain(SIN_SWEEP_CH,1);        //output
                        mixerL.gain(SIN_SWEEP_CH,1);        //mixer
                        break;
        default:
                        mixerSetChannel(MUTE_ALL);
                        break;
    }
    AudioInterrupts();    // enable, both tones will start together
    return out;
}
//##############################################################################
// ### Parameter display functions ###
// Clear main area
void displayClrMainArea(void)
{
    display.fillRect(0,11, display.width(),display.height(),BLACK);
}
//##############################################################################
// ### Clr + title
void displayMainArea(void)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE,BLACK);
    display.setCursor(0,0);
    display.println("TestGEN:");
    display.drawFastHLine(0, 10, display.width(), WHITE);
    displayMode();

    switch(engineState)
    {
        case NOISE_GEN:
                    displayNoiseGen();
                    break;
        case SIN_SWEEP:
                    displaySinSweep();
                    displayArrow(sinSweepDir);
                    break;
        case SD_WAV_PLAY:
                    if (SDinitComplete==false)
                    {
                        displayClrMainArea();
                        display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
                        display.println("SD access failed");
                        delay(2000);
                    }
                    break;
        case SIG_GEN:                   //scroll throuvh wavefoprms, arb not used
                    displayNote(sigGen_oct, sigGen_note);
                    displayWaveSymbol(sigGen_wave);
                    break;
        default:
                    break;
    }
}
//##############################################################################
// ###  displays the note and it's corresponding freq.
void displayNote(uint8_t octave, uint8_t note)
{
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
    display.setTextSize(3);
    if (octave == FREQ_TECH_OCT)
    {
        display.print(noteNameTable[note+12]);
    }
    else
    {
        display.print(noteNameTable[note]);
        display.print(octave);
    }
    display.setTextSize(2);
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
    display.print(noteFreqTable[octave][note]);
    display.print("Hz");
    display.setTextSize(1);
}
//##############################################################################
// ###  displays warnng if the set frequency exceedes the allowed value .
void displayFreqWarning(void)
{
    display.setCursor(25,DISP_TXT_ROW0);
    display.println("Max frequency");
    display.setCursor(10,DISP_TXT_ROW1);
    display.println("for this waveform");
    display.setCursor(35,DISP_TXT_ROW2);
    display.print("exceeded.");
}
//##############################################################################
// ###  display waveform symbol as bitmap
void displayWaveSymbol(uint8_t wave)
{
    if (wave>MAX_WAVEFORMS-1) return;
    display.fillRect(110,0,16,8,BLACK);
    display.drawBitmap(110, 0, waveSymbols+(wave*16), 16,8, WHITE);
}
//##############################################################################
void displayNoiseGen(void)
{
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
    display.println("1: White Noise");
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW1);
    display.println("2: Pink Noise");
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
    display.print("#: Mute/OFF");
}
//##############################################################################
// print engine mode
void displayMode(void)
{
    display.fillRect(63, 0, 63,8, BLACK);
    display.setCursor(DISP_TXT_COL1,0);
    display.print(engineStateTxt[engineState]);
}
//##############################################################################
void displayMemUsage(void)
{
    display.setCursor(0,63);
    display.print("Mem: ");
    display.print(AudioMemoryUsageMax());
}
void displayCpuUsage(void)
{
    display.setCursor(70,12);
    display.print("CPU:");
    display.print(AudioProcessorUsage());
}
//##############################################################################
void displayFileName(void)
{
    float fileLength_s = 0.0;

    displayClrMainArea();
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
    display.println("Playing: ");
    display.setTextSize(2);
    display.print(currentFile);
    display.setTextSize(1);
    if (playSdWavA.isPlaying()) fileLength_s = playSdWavA.lengthMillis()/1000.0;
    if (playSdWavB.isPlaying()) fileLength_s = playSdWavB.lengthMillis()/1000.0;
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
    display.print("Length = ");
    display.print(fileLength_s);
    display.print("s");
}
//##############################################################################
void displaySinSweep(void)
{
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
    display.print("Fmin = "); display.print(sinSweepStartF); display.print("Hz");
    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW1);
    display.print("Fmax = "); display.print(sinSweepEndF); display.print("Hz");
}

//##############################################################################
void displayWavProgress(void)
{
    if (playSdWavA.isPlaying())
    {
        displayBargraph(0,playSdWavA.lengthMillis(),playSdWavA.positionMillis());
    }
    if (playSdWavB.isPlaying())
    {
        displayBargraph(0,playSdWavB.lengthMillis(),playSdWavB.positionMillis());
    }
}
//##############################################################################
void displayBargraph(int min, int max, int value)
{
    uint32_t pos;
    if (max >= min)     pos = map(value, min, max, 0, display.width()-1);
    else                pos = map(value, max, min, 0, display.width()-1);
    display.drawRect(0, 60, display.width(), 3, WHITE); //border
    display.fillRect(0, 61, pos, 1, WHITE);
    display.fillRect(pos+1, 61,display.width()-pos-2, 1, BLACK);
}
//##############################################################################
void displayArrow(int dir)
{
    if (dir!=1 && dir!=-1)  return;

    display.fillRect(100, 12, 20, 23, BLACK);   //clear area
    display.setCursor(103,23);
    display.setTextSize(2);
    if (dir>0)  display.print((char)25);
    else        display.print((char)24);
    display.setTextSize(1);
}
//##############################################################################
void displayHelpTxt(uint8_t mode)
{
    display.setCursor(0,0);
    display.clearDisplay();
    display.setTextSize(1);
    display.print(help_txt[mode]);
    display.drawFastHLine(0, 10, display.width(), WHITE);
    display.display();
}
//##############################################################################
void displayStartScreen(engineState_t mode)
{
    displayMode();
    displayClrMainArea();
    switch(mode)
    {
        case NOISE_GEN:
                    displayClrMainArea();
                    displayNoiseGen();
                    break;
        case SIN_SWEEP:
                    displaySinSweep();
                    displayArrow(sinSweepDir);
                    break;
        case SD_WAV_PLAY:
                    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW0);
                    display.print("0-9:Play file 00-09");
                    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW1);
                    display.print("*:Enter new file No");
                    display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
                    display.print("#:Stop/Mute");
                    break;
        case SIG_GEN:
                    displayNote(sigGen_oct, sigGen_note);
                    displayWaveSymbol(sigGen_wave);
                    break;
        default:
                    break;
    }
}
//##############################################################################
void displayUpdate(void)
{
    if (helpMode == false)      //in help mode do not update
    {
        //displayCpuUsage();
        switch (engineState)
        {
            case SIG_GEN:
                        break;
            case NOISE_GEN:
                        break;
            case SD_WAV_PLAY:
                            char txtNumber[2];
                            txtNumber[1]='\0';
                            switch(fileNameEditMode)
                            {
                                case SETNAME_OFF:
                                        displayWavProgress();
                                        break;
                                case SETNAME_ONES:
                                        display.setCursor(55, DISP_TXT_ROW1-3);
                                        display.setTextSize(2);
                                        txtNumber[0] = currentFile[WAVE_NO_10]; //print tens
                                        display.print(txtNumber);
                                        display.setTextColor(BLACK, WHITE);     //reverse
                                        txtNumber[0] = currentFile[WAVE_NO_01];
                                        display.print(txtNumber);               //print ones
                                        display.setTextColor(WHITE, BLACK);     //restore
                                        display.setTextSize(1);                 //default
                                        break;
                                case SETNAME_TENS:
                                        display.setCursor(55, DISP_TXT_ROW1-3);
                                        display.setTextSize(2);
                                        display.setTextColor(BLACK, WHITE);     //reverse
                                        txtNumber[0] = currentFile[WAVE_NO_10]; //print tens
                                        display.print(txtNumber);
                                        display.setTextColor(WHITE, BLACK);     //restore
                                        txtNumber[0] = currentFile[WAVE_NO_01];
                                        display.print(txtNumber);               //print ones
                                        display.setTextSize(1);                 //default
                                        break;
                            }
                            break;
            case SIN_SWEEP:
                            display.fillRect(0, 50, display.width(), 8, BLACK);
                            display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
                            if (tonesweep.isPlaying())
                            {
                                display.print("F=");
                                display.print(tonesweep.getFreqExp());
                                display.print("Hz");
                                display.setCursor(97,DISP_TXT_ROW2);
                                if (tonesweep.getPause())   display.print("PAUSE");
                                else                        display.print("     ");
                            }
                            else
                            {
                                display.print("Press 0-9 to start");
                            }
                            uint32_t pos = tonesweep.getFreq();
                            displayBargraph(    sinSweepStartF,
                                                sinSweepEndF,
                                                pos);
                            break;
        }
    }
}
//##############################################################################
// ### start sin sweep ###
bool playSinSweep(float time_ms, int dir)
{
    tonesweep.play(sinSweepLvl, sinSweepStartF, sinSweepEndF, time_ms, dir);
    return true;
}
//##############################################################################
//### play a wav fime from the SD card ###
bool playFile(const char *filename, wavPlayerNo_t playerNo)
{
    bool retVal = false;
    switch(playerNo)
    {
        case WAV_PLAYER_A:
                    if (playSdWavA.play(filename) == true)  retVal = true;
                    delay(5);
                    break;
        case WAV_PLAYER_B:
                    if (playSdWavB.play(filename) == true) retVal = true;
                    delay(5);
                    break;
    }
    return retVal;
}
//##############################################################################
//### stops both players and starts playing wav number "fileNo" ###
void playNewFile(void)
{
    playSdWavA.stop();
    playSdWavB.stop();
    nextToPlay = PLAYER_NONE;
    mixerSetChannel(WAV_PLAYER);
    if (playFile(currentFile,WAV_PLAYER_A) == true)   displayFileName();
    else
    {
        displayClrMainArea();
        display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW1);
        display.print("File not found...");
    }
}
//##############################################################################
/*  sets the global file name WAVXX.WAV
 *  XX is the current wave file number
 */
bool setFileName(char waveNo)
{
    char x;
    if (waveNo > MAX_WAV_FILES) return false;

    x = waveNo / 10;
    currentFile[WAVE_NO_10] = '0'+ x;
    x = waveNo % 10;
    currentFile[WAVE_NO_01] = '0'+ x;

    return true;
}
//##############################################################################
/*  sets only one digit in the global file name WAVXX.WAV
 *  XX is the current wave file number
 */
bool setFileNameDigit(uint8_t value, uint8_t mask)
{
    if (value > 9 || mask > 0b10)  return false;
    if (mask == 0b01)   currentFile[WAVE_NO_01] = '0'+ value;
    if (mask == 0b10)   currentFile[WAVE_NO_10] = '0'+ value;
    return true;
}
//##############################################################################
//### wave loop  ###
/*    creates a looped wav player using two playSdWav components in
      ping-pong mode.
      1st play is started always using playerA and playFile function
*/
void wavLooper(void)
{
    if (engineState == SD_WAV_PLAY)
    {
        switch(nextToPlay)
        {
            case PLAYER_NONE:
                            break;
            case PLAYER_A:
                            playFile(currentFile, WAV_PLAYER_A);
                            delay(5);
                            nextToPlay = PLAYER_NONE;
                            break;
            case PLAYER_B:
                            playFile(currentFile, WAV_PLAYER_B);
                            delay(5);
                            nextToPlay = PLAYER_NONE;
                            break;
        }
    }
    else
    {
        playSdWavA.stop();
        playSdWavB.stop();
        nextToPlay = PLAYER_NONE;
    }
}
//##############################################################################
/* Timer 1 is used to periodically poll the isPlaying() flags and triggers the
   next player to create a continuous loop. This is a workaround, since
   play_sd_wav object in the audio library does not offer succh feature (yet!)
   ISR  sets flags ojnly, the actual play command wiull be issued in the main loop.
   Polling the isPlaying flags in the main loop is not precise enough due to other
   tasks the MCU has to deal with, they can be missed. Hence the use of a timer
   interrupt.
 */
void ISR_checkWavePosition(void)
{
    if (engineState == SD_WAV_PLAY)
    {
        if(  playSdWavA.isPlaying() &&  \
            (playSdWavA.lengthMillis() - playSdWavA.positionMillis()) < XFADE_SIZE &&   \
            !playSdWavB.isPlaying())
        {
            nextToPlay = PLAYER_B;
        }

        if(  playSdWavB.isPlaying() &&  \
            (playSdWavB.lengthMillis() - playSdWavB.positionMillis()) < XFADE_SIZE &&   \
            !playSdWavA.isPlaying())
        {
            nextToPlay = PLAYER_A;
        }
    }
}
//##############################################################################
//###  oscillator setup ###
/*  starts the waveform generator with preset frequency and waveform
 *  available range for waveforms:
 *  SIN/TRI/SQR/PULSE: 16-18kHz
 *  SQR and PULSE use 12bit DAC instead of the codec, hence they are able
 *  to produce a pure square wave
 *  RampUp & Down are heavily aliased beyond 8kHz, hence the limitation
 */
bool setSigGen(float freq, short waveform)
{
    bool out = false;
    if (waveform>MAX_WAVEFORMS-1) return out;
    if (freq > SIGGEN_F_LIMIT && waveform > 3) return out;       //4,5 = RampDown, RampUp
    out = true;
    wave.begin(SIGGEN_AMPL_DEFAULT, freq/2 , waveIndex[waveform]);
    return out;
}
//##############################################################################
void setup()
{
    Serial.begin(115200);
    delay(100);
    pinMode(RELAY_CTRL,OUTPUT);                 //relay ctrl
    digitalWrite(RELAY_CTRL, LOW);              //switch off
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x78
    delay(100);
    Wire.setClock(400000);  //i2c clock speed = 400kHz

    display.clearDisplay();
    display.drawBitmap(0,0, welcomeScrn, 128,64, WHITE);
    display.display();

    AudioMemory(15);
    keypad.addEventListener(keypadEvent); //add an event listener for this keypad

    // Enable the codec, mute the HP out, we're using the line out only
    sgtl5000_1.enable();
    sgtl5000_1.volume(0.5);
    sgtl5000_1.muteHeadphone();

    setDACFreq(44117 * 2);
    setI2SFreq(44117 * 2);

    Timer1.initialize(2000);
    Timer1.attachInterrupt(ISR_checkWavePosition);

    delay(500);

    displayMainArea();

    SPI.setMOSI(SDCARD_MOSI_PIN);
    SPI.setSCK(SDCARD_SCK_PIN);
    if (!(SD.begin(SDCARD_CS_PIN)))
    {
        SDinitComplete = false;
        display.clearDisplay();
        display.setCursor(DISP_TXT_COL0,DISP_TXT_ROW2);
        display.println("SD access failed");
        display.display();
        delay(2000);
        displayMainArea();
    }
    else        SDinitComplete = true;

    setSigGen(noteFreqTable[sigGen_oct][sigGen_note], sigGen_wave);
    wave.pulseWidth(sigGen_duty);
    mixerSetChannel(MUTE_ALL);

    display.display();
}
//##############################################################################
void loop()
{
    key=keypad.getKey();      //update keypad input
    wavLooper();
    displayUpdate();
    display.display();
}
