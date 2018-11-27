/**
** Function: 用于播放wav文件，默认wav参数为 16 bit ，1 channels，16K rate 程序基于alsa aplay修改
** Author: Chen Jiang
** Date: 2018/8/12 release:1.0.0
**/

#include <playback.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define LLONG_MAX               9223372036854775807LL
#define FORMAT_WAVE		2
#define WAV_FMT			COMPOSE_ID('f','m','t',' ')
#define WAV_DATA		COMPOSE_ID('d','a','t','a')
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define check_wavefile_space(buffer, len, blimit) \
	if (len > blimit) { \
		blimit = len; \
		if ((buffer = realloc(buffer, blimit)) == NULL) { \
			printf("not enough memory");		  \
		} \
	}

static int err;
static snd_pcm_t *handle;
static snd_pcm_info_t *info;

//static char *pcm_name = "default";
static int open_mode = 0;
static snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
static Hwparams hwparams = {SND_PCM_FORMAT_S16_LE,1,16000};

static snd_pcm_sframes_t (*writei_func)(snd_pcm_t *handle, const void *buffer, snd_pcm_uframes_t size);
static snd_pcm_uframes_t chunk_size = 0;
static unsigned long pbrec_count = LLONG_MAX,fdcount;
static unsigned char *audiobuf = NULL;
static size_t significant_bits_per_sample, bits_per_sample, bits_per_frame;
static size_t chunk_bytes;

typedef struct {
	u_int magic;		/* 'RIFF' */
	u_int length;		/* filelen */
	u_int type;		/* 'WAVE' */
} WaveHeader;

typedef struct {
	u_short format;		/* see WAV_FMT_* */
	u_short channels;
	u_int sample_fq;	/* frequence of sample */
	u_int byte_p_sec;
	u_short byte_p_spl;	/* samplesize; 1 or 2 bytes */
	u_short bit_p_spl;	/* 8, 12 or 16 bit */
} WaveFmtBody;

typedef struct {
	u_int type;		/* 'data' */
	u_int length;		/* samplecount */
} WaveChunkHeader;

static ssize_t test_wavefile(int fd, unsigned char *_buffer, size_t size);
static size_t test_wavefile_read(int fd, unsigned char *buffer, size_t *size, size_t reqsize, int line);
static void set_params(void);
static ssize_t safe_read(int fd, void *buf, size_t count);
static void playback_go(int fd, size_t loaded, unsigned long count, int rtype, char *name);
static ssize_t pcm_write(u_char *data, size_t count);
static void xrun(void);

extern void init_params(Hwparams hwparams_local)
{
	hwparams.format = hwparams_local.format;
	hwparams.channels = hwparams_local.channels;
	hwparams.rate = hwparams_local.rate;
}

extern int playback(char *name, char *pcm_name)
{
	ssize_t dtawave;
        int fd = -1;

	pbrec_count = LLONG_MAX;
        fdcount = 0;
        chunk_size = 1024;

        snd_pcm_info_alloca(&info);

	err = snd_pcm_open(&handle, pcm_name, stream, open_mode);
   	if (err < 0) {
		printf("audio open error: %s", snd_strerror(err));
		return 0;
   	}

   	audiobuf = (u_char *)malloc(1024);
   	if (audiobuf == NULL) {
		printf("not enough memory");
		return 0;
   	}

  	writei_func = snd_pcm_writei;

   	if ((err = snd_pcm_info(handle, info)) < 0) {
		printf("info error: %s", snd_strerror(err));
		return 0;
   	}

	if ((fd = open(name, O_RDONLY, 0)) == -1) {
		printf("open fd error: %s", snd_strerror(err));
		return 0;
	}

	/* read the file header */
	read(fd, audiobuf, 26);

	if ((dtawave = test_wavefile(fd, audiobuf, 26)) >= 0) {
		playback_go(fd, dtawave, pbrec_count, FORMAT_WAVE, name);
	}

	close(fd);
   	snd_pcm_close(handle);
	handle = NULL;
	free(audiobuf);

	return 1;
}

static ssize_t test_wavefile(int fd, u_char *_buffer, size_t size)
{
	u_char *buffer = NULL;
	size_t blimit = 0;
	WaveChunkHeader *c;
	u_int type, len;

	if (size > sizeof(WaveHeader)) {
		check_wavefile_space(buffer, size - sizeof(WaveHeader), blimit);
		memcpy(buffer, _buffer + sizeof(WaveHeader), size - sizeof(WaveHeader));
	}

	size -= sizeof(WaveHeader);
	while (1) {
		check_wavefile_space(buffer, sizeof(WaveChunkHeader), blimit);
		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;

		type = c->type;
		len = c->length;
		len += len % 2;

		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_FMT)
			break;
		check_wavefile_space(buffer, len, blimit);
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	if (len < sizeof(WaveFmtBody)) {
		printf("unknown length of 'fmt ' chunk (read %u, should be %u at least)",
		      len, (u_int)sizeof(WaveFmtBody));
		exit(1);
	}
	check_wavefile_space(buffer, len, blimit);
	test_wavefile_read(fd, buffer, &size, len, __LINE__);

	if (size > len)
		memmove(buffer, buffer + len, size - len);
	size -= len;
	
	while (1) {
		u_int type, len;

		check_wavefile_space(buffer, sizeof(WaveChunkHeader), blimit);
		test_wavefile_read(fd, buffer, &size, sizeof(WaveChunkHeader), __LINE__);
		c = (WaveChunkHeader*)buffer;
		type = c->type;
		len = c->length;
		if (size > sizeof(WaveChunkHeader))
			memmove(buffer, buffer + sizeof(WaveChunkHeader), size - sizeof(WaveChunkHeader));
		size -= sizeof(WaveChunkHeader);
		if (type == WAV_DATA) {
			if (len < pbrec_count && len < 0x7ffffffe)
				pbrec_count = len;
			if (size > 0)
				memcpy(_buffer, buffer, size);
			free(buffer);
			return size;
		}
		len += len % 2;
		check_wavefile_space(buffer, len, blimit);
		test_wavefile_read(fd, buffer, &size, len, __LINE__);
		if (size > len)
			memmove(buffer, buffer + len, size - len);
		size -= len;
	}

	/* shouldn't be reached */
	return -1;
}

static size_t test_wavefile_read(int fd, u_char *buffer, size_t *size, size_t reqsize, int line)
{
	if (*size >= reqsize)
		return *size;
	if ((size_t)safe_read(fd, buffer + *size, reqsize - *size) != reqsize - *size) {
		printf("read error (called from line %i)", line);
		exit(1);
	}
	return *size = reqsize;
}

static void set_params(void)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t buffer_size;

	snd_pcm_hw_params_alloca(&params);
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for this PCM: no configurations available");
		exit(1);
	}
	
	err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		printf("Access type not available");
		exit(1);
	}

	err = snd_pcm_hw_params_set_format(handle, params, hwparams.format);
	if (err < 0) {
		printf("Sample format non available");
		exit(1);
	}

	err = snd_pcm_hw_params_set_channels(handle, params, hwparams.channels);
	if (err < 0) {
		printf("Channels count non available");
		exit(1);
	}

	err = snd_pcm_hw_params_set_rate_near(handle, params, &hwparams.rate, 0);

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to install hw params!");
		exit(1);
	}

	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	if (chunk_size == buffer_size) {
		printf("Can't use period equal to buffer size (%lu == %lu)",
		      chunk_size, buffer_size);
		exit(1);
	}

	bits_per_sample = snd_pcm_format_physical_width(hwparams.format);
	significant_bits_per_sample = snd_pcm_format_width(hwparams.format);
	bits_per_frame = bits_per_sample * hwparams.channels;
	chunk_bytes = chunk_size * bits_per_frame / 8;
	audiobuf = realloc(audiobuf, chunk_bytes);
	if (audiobuf == NULL) {
		printf("not enough memory");
		exit(1);
	}
}

/* Safe read (for pipes) */
static ssize_t safe_read(int fd, void *buf, size_t count)
{
	ssize_t result = 0, res;

	while (count > 0) {
		if ((res = read(fd, buf, count)) == 0)
			break;
		if (res < 0)
			return result > 0 ? result : res;
		count -= res;
		result += res;
		buf = (char *)buf + res;
	}

	return result;
}

/* Playing raw data */
static void playback_go(int fd, size_t loaded, unsigned long count, int rtype, char *name)
{
	int l, r;
	unsigned long written = 0;
	unsigned long c;

	set_params();

	l = loaded;
	while (written < count) {
		do {
			c = count - written;
			if (c > chunk_bytes)
				c = chunk_bytes;
			c -= l;

			if (c == 0)
				break;
			r = safe_read(fd, audiobuf + l, c);
			if (r < 0) {
				perror(name);
				exit(1);
			}
			fdcount += r;
			if (r == 0)
				break;
			l += r;
		} while ((size_t)l < chunk_bytes);
		l = l * 8 / bits_per_frame;
		r = pcm_write(audiobuf, l);
		if (r != l)
			break;
		r = r * bits_per_frame / 8;
		written += r;
		l = 0;
	}
	snd_pcm_drain(handle);
}

/* Write prepared data to pcm */
static ssize_t pcm_write(u_char *data, size_t count)
{
	ssize_t r;
	ssize_t result = 0;

	while (count > 0) {

		r = writei_func(handle, data, count);

		if (r == -EPIPE) {
			xrun();
		}
                else if (r < 0)
		{

		}

		if (r > 0) {

			result += r;
			count -= r;
			data += r * bits_per_frame / 8;
		}
	}
	return result;
}

/* I/O error handler */
static void xrun(void)
{
	snd_pcm_status_t *status;
	int res;
	
	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status))<0) {
		printf("status error: %s", snd_strerror(res));
		exit(1);
	}

	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {

		if ((res = snd_pcm_prepare(handle))<0) {
			printf("xrun: prepare error: %s", snd_strerror(res));
			exit(1);
		}
		return;		/* ok, data should be accepted again */
	}
}

#ifdef __cplusplus
}
#endif
