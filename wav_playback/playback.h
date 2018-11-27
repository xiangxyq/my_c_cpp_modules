#ifndef PLAYBACK_H
#define PLAYBACK_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <alsa/asoundlib.h>

typedef struct {
	snd_pcm_format_t format; /* SND_PCM_FORMAT_S16_LE */
	unsigned int channels;
	unsigned int rate;
} Hwparams;

extern void init_params(Hwparams hwparams_local);  //若不调用此函数，默认为format: SND_PCM_FORMAT_S16_LE， channels:1 rate:16000
extern int playback(char *file_path, char *pcm_name); //file_path:wav文件路径 pcm_name: 输出音频的card:device 比如 "default" "hw:0,1"

#ifdef __cplusplus
}
#endif

#endif
