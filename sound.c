#include <stdlib.h>
#include <esd.h>
#include "main.h"
#include "sound.h"

static int _af_ulaw2linear(unsigned char ulawbyte)
{
	static int exp_lut[8] = { 0, 132, 396, 924, 1980, 4092, 8316, 16764 };
	int sign, exponent, mantissa, sample;

	ulawbyte = ~ulawbyte;
	sign = (ulawbyte & 0x80);
	exponent = (ulawbyte >> 4) & 0x07;
	mantissa = ulawbyte & 0x0F;
	sample = exp_lut[exponent] + (mantissa << (exponent + 3));
	if (sign != 0)
		sample = -sample;

	return (sample);
}

void play()
{
	int i;
	unsigned short *lineardata;

	int esd_fd;
	esd_format_t format = ESD_BITS16 | ESD_STREAM | ESD_PLAY | ESD_MONO;

	if ((esd_fd = esd_play_stream(format, 8012, NULL, PROG)) < 0)
		return;

	lineardata = malloc(sizeof(Receive) * 2);
	for (i = 0; i < sizeof(Receive); i++)
		lineardata[i] = _af_ulaw2linear(Receive[i]);

	write(esd_fd, lineardata, sizeof(Receive) * 2);

	close(esd_fd);
	free(lineardata);
}
