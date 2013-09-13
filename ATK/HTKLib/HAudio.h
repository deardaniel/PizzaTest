/* ----------------------------------------------------------- */
/*           _ ___   	     ___                               */
/*          |_| | |_/	  |_| | |_/    SPEECH                  */
/*          | | | | \  +  | | | | \    RECOGNITION             */
/*          =========	  =========    SOFTWARE                */
/*                                                             */
/* ================> ATK COMPATIBLE VERSION <================= */
/*                                                             */
/* ----------------------------------------------------------- */
/* developed at:                                               */
/*                                                             */
/*      Machine Intelligence Laboratory (Speech Group)         */
/*      Cambridge University Engineering Department            */
/*      http://mi.eng.cam.ac.uk/                               */
/*                                                             */
/*      Entropic Cambridge Research Laboratory                 */
/*      (now part of Microsoft)                                */
/*                                                             */
/* ----------------------------------------------------------- */
/*         Copyright: Microsoft Corporation                    */
/*          1995-2000 Redmond, Washington USA                  */
/*                    http://www.microsoft.com                 */
/*                                                             */
/*          2001-2007 Cambridge University                     */
/*                    Engineering Department                   */
/*                                                             */
/*   Use of this software is governed by a License Agreement   */
/*    ** See the file License for the Conditions of Use  **    */
/*    **     This banner notice must not be removed      **    */
/*                                                             */
/* ----------------------------------------------------------- */
/*           File: HAudio.h -    Audio Input/Output            */
/* ----------------------------------------------------------- */

/* !HVER!HAudio: 1.6.0 [SJY 01/06/07] */

#ifndef _HAUDIO_H_
#define _HAUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AudioIn  *AudioIn;    /* Abstract audio input stream */
typedef struct _AudioOut *AudioOut;   /* Abstract audio output stream */

#define WAVEPACKETSIZE 1024   /* size of all audio packets and buffers */

void InitAudio(void);
/*
   Initialise audio module
*/

AudioIn OpenAudioInput(HTime *sampPeriod);
/*
   Initialise and return an audio stream to sample with given period.
   If *sampPeriod is 0.0 then a default period is used (eg it might
   be set by some type of audio control panel) and this default
   period is assigned to *sampPeriod.
*/

void StartAudioInput(AudioIn a);
/*
   Start audio input sampling.
*/

void StopAudioInput(AudioIn a);
/*
   Stop audio device and set AI_STOPPED.
*/

void CloseAudioInput(AudioIn a);
/*
   Terminate audio input stream and free buffer memory
*/

int PacketsInAudio(AudioIn a);
/*
   Return number of input packets which are
   currently available from the given audio stream.
*/

void GetAudio(AudioIn a, short *buf);
/*
   Get WAVEPACKETSIZE samples and store in buf.
*/

AudioOut OpenAudioOutput(HTime *sampPeriod);
/*
   Initialise and return an audio stream for output at given sample
   rate.  If *sampPeriod is 0.0 then a default period is used (eg it might
   be set by some type of audio control panel) and this default
   period is assigned to *sampPeriod.
*/

void PlayAudioOutput(AudioOut a, long nSamples, short *buf,
                     Boolean isLast, AudioIn ain);
/*
   Output nSamples to audio stream a using data stored in buf.
   This can be called multiple times to send a stream of audio.
   isLast should be set true for the final block.
   nSamples must be less than or equal to WAVEPACKETSIZE.
   If ain is given, then echo cancellation is enabled.
*/

void FlushAudioOutput(AudioOut a);
/*
   Flush any buffered output blocks.  Note that a finished event will
   be sent for every endblock encountered.
*/

void CloseAudioOutput(AudioOut a);
/*
   Terminate audio stream a
*/

void SetOutVolume(AudioOut a, int volume);
/*
   Set the volume of the given audio device. Volume range
   is 0 to 100.
*/

float GetCurrentVol(AudioIn a);
/*
   Obtain current volume of audio input device
*/

int SamplesToPlay(AudioOut a);
/*
   Return num samples left to play in output stream a
*/

int FakeSilenceSample();
/*
   Return a fake silence sample
*/

#ifdef __cplusplus
}
#endif

#endif  /* _HAUDIO_H_ */

/* ------------------------ End of HAudio.h ----------------------- */
