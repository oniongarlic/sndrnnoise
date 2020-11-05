/*
CLI app to run rnnoise on "any" libsndfile supported audio file.
Copyright (C) 2020  Kaj-Michael Lang <milang@tal.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sndfile.h>

#include <rnnoise.h>

#define FRAME_SIZE 480

int main(int argc, char **argv)
{
const char *file_input, *file_output;
SNDFILE *snd_input=NULL, *snd_output=NULL;
SF_INFO info_in, info_out;
DenoiseState *st;
float data[FRAME_SIZE];

if (argc!=3) {
 fprintf(stderr, "Usage: %s input.wav output.wav\n", argv[0]);
 return 1;
}

file_input=argv[1];
file_output=argv[2];

/* printf("Input: %s\nOutput: %s\n", file_input, file_output);*/

if ((snd_input=sf_open(file_input, SFM_READ, &info_in)) == NULL) {
 fprintf(stderr, "Failed to open input file: %s\n", file_input);
 return 1;
}

/* XXX: Add support for denoising any amount of channels */
if (info_in.channels!=1) {
 fprintf(stderr, "Only 1 channel input file supported.\n");
 return 1;
}

if (info_in.samplerate!=48000) {
 fprintf(stderr, "Only 48kHz samplerate input file supported.\n");
 return 1;
}

/* Make a copy, but adjust output format to always be 16-bit PCM wav */
memcpy(&info_out, &info_in, sizeof(info_out));
info_out.format=SF_FORMAT_WAV | SF_FORMAT_PCM_16;

if ((snd_output=sf_open(file_output, SFM_WRITE, &info_out)) == NULL) {
 printf("Failed to open output file: %s (%s)\n", file_output, sf_strerror (NULL));
 return 1;
}

st=rnnoise_create(NULL);

/* Don't normalize to -1.0 - 1.0 */
sf_command(snd_input, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);
sf_command(snd_output, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);

while (1) {
 int r, w, frames=FRAME_SIZE;
 float rn;

 r=sf_readf_float(snd_input, data, frames);
 if (r==0)
   break;

 rn=rnnoise_process_frame(st, data, data);

 w=sf_writef_float(snd_output, data, r);
 if (w!=r) {
  fprintf(stderr, "Output file write fail!\n");
  break;
 }

 /* printf("%d/%d/%f\n", r, w, rn); */
}

rnnoise_destroy(st);

sf_close(snd_input);
sf_close(snd_output);

return 0;
}

