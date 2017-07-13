/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Pete (El Supremo)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "synth_tonesweep.h"
#include "arm_math.h"

extern "C" {
extern const int16_t exp_table[257];
};

#define TONESWEEP_UP    	1
#define TONESWEEP_DWN       0


/******************************************************************/

//                A u d i o S y n t h T o n e S w e e p
// Written by Pete (El Supremo) Feb 2014


boolean AudioSynthToneSweep::play(float t_amp,int t_lo,int t_hi,float t_time, int t_dir)
{
  float tone_tmp;

if(0) {
  Serial.print("AudioSynthToneSweep.begin(tone_amp = ");
  Serial.print(t_amp);
  Serial.print(", tone_lo = ");
  Serial.print(t_lo);
  Serial.print(", tone_hi = ");
  Serial.print(t_hi);
  Serial.print(", tone_time = ");
  Serial.print(t_time,1);
  Serial.println(")");
}
  tone_amp = 0;
  if(t_amp < 0)return false;
  if(t_amp > 1)return false;
  if(t_lo < 1)return false;
  if(t_hi < 1)return false;
  if(t_hi >= (int) AUDIO_SAMPLE_RATE_EXACT / 2)return false;
  if(t_lo >= (int) AUDIO_SAMPLE_RATE_EXACT / 2)return false;
  if(t_time <= 0)return false;
  if(t_dir != 1 && t_dir!=-1)  return false;

  tone_phase = 0;
  tone_sign = t_dir;

  tone_amp = t_amp * 32767.0;
  if (t_hi >= t_lo)
  {
    tone_hi = t_hi;
    tone_lo = t_lo;
    tone_tmp = t_hi - t_lo;
    tone_sign_mult = 1;
    if (tone_sign > 0)    tone_freq = tone_lo*0x100000000LL;
    else                  tone_freq = tone_hi*0x100000000LL;
  }
  else
  {
    tone_hi = t_lo;
    tone_lo = t_hi;
    tone_tmp = tone_hi - tone_lo;
    tone_sign_mult = -1;
    if (tone_sign > 0)    tone_freq = tone_hi*0x100000000LL;
    else                  tone_freq = tone_lo*0x100000000LL;
  }

  tone_tmp = tone_tmp / t_time / AUDIO_SAMPLE_RATE_EXACT;   //freq step pro one sample
  tone_incr = (tone_tmp * 0x100000000LL);                   //phaseacc adder
  sweep_pause = 0;
  sweep_busy = 1;

  return(true);
}
//------------------------------------------------------------------------------
/*
    Exponential function shaper. Takes linear input and returns
    value mapped to exponential curve.
    Uses exp_table placed in  data_waveforms.c
    Internal range is 15bit
*/

uint32_t AudioSynthToneSweep::getExpFreq(uint32_t linFreq)
{
    uint32_t pos, index, scale;
    uint32_t val1, val2, val;

    if (linFreq > (int)AUDIO_SAMPLE_RATE_EXACT / 2) return 0;
    if (linFreq < tone_lo)  linFreq = tone_lo;
    if (linFreq > tone_hi)  linFreq = tone_hi;

    pos = map(linFreq,tone_lo,tone_hi, 0, 32767);   //freq range mapped to 15bit
    //linear interpolation
    index = pos >> 7;
    val1 = exp_table[index];
    val2 = exp_table[index+1];

    scale = pos & 0x7F;
    val2 *= scale;
    val1 *= 0x80 - scale;
    val = (val1 + val2) >> 7;
    return (map(val,0,32767,tone_lo,tone_hi));
}


//------------------------------------------------------------------------------
/*

*/
void AudioSynthToneSweep::update(void)
{
  audio_block_t *block;
  short *bp;
  int i;

  if(!sweep_busy)return;

  //          L E F T  C H A N N E L  O N L Y
  block = allocate();
  if(block) {
    bp = block->data;
    uint32_t tmp  = tone_freq >> 32;
    freq_exp = getExpFreq(tmp);

    uint64_t tone_tmp = (0x400000000000LL * (int)(freq_exp&0x7fffffff)) / (int) (AUDIO_SAMPLE_RATE_EXACT*2); //for 88.2kHz I2S
    // Generate the sweep
    for(i = 0;i < AUDIO_BLOCK_SAMPLES;i++)
    {
      *bp++ = (short)(( (short)(arm_sin_q31((uint32_t)((tone_phase >> 15)&0x7fffffff))>>16) *tone_amp) >> 15);      //

      tone_phase +=  tone_tmp;
      if(tone_phase & 0x800000000000LL)tone_phase &= 0x7fffffffffffLL;

      if (!sweep_pause)   //update frequency only if pause = 0
      {
          if(((int)(tone_sign*tone_sign_mult)) > 0)
          {
            if(tmp > tone_hi)
            {
              sweep_busy = 0;
              break;
            }
            tone_freq += tone_incr;
          }
          else
          {
              if(freq_exp < tone_lo || tone_freq < tone_incr)         //we need to check if the actual frequency
              {                                                  //is not less than the freq step, too
                  sweep_busy = 0;                                //otherwise tone_freq will underflow...
                  break;
              }
              tone_freq -= tone_incr;                            //...right here
          }
      }
    }

    while(i < AUDIO_BLOCK_SAMPLES) {
      *bp++ = 0;
      i++;
    }
    // send the samples to the left channel
    transmit(block,0);
    release(block);
  }
}

int AudioSynthToneSweep::getFreqExp(void)
{
    return(freq_exp);
}


int AudioSynthToneSweep::getFreq(void)
{
    return(tone_freq>>32);
}

unsigned char AudioSynthToneSweep::isPlaying(void)
{
  return(sweep_busy);
}

// ### Stops the ongoing sweep and resets the freq to 0 ###
void AudioSynthToneSweep::stop(void)
{
    sweep_busy = 0;
    tone_freq = 0;
}
// ### pauses the ongoing sweep ###
void AudioSynthToneSweep::pause(unsigned char pause_state)
{
    sweep_pause = pause_state ? 1 : 0;
}
// ### return the current pause state ###
unsigned char  AudioSynthToneSweep::getPause(void)
{
    return (sweep_pause);
}
// ### changes the direction of the sweep ###
void AudioSynthToneSweep::setSweepDir(int sweep_dir)
{
    if (sweep_dir != 1 && sweep_dir != -1)  return;
    tone_sign = sweep_dir;

}
// ### returns the current sweep direction value ###
int AudioSynthToneSweep::getSweepDir(void)
{
    return (tone_sign);
}

void AudioSynthToneSweep::flipDir(void)
{
    tone_sign *= -1;
}
