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

SNDFILE *open_output(const char *file_output, SF_INFO *snd_info)
{
SNDFILE *snd_output;

if ((snd_output=sf_open(file_output, SFM_WRITE, snd_info)) == NULL) {
 fprintf(stderr, "Failed to open output file: %s (%s)\n", file_output, sf_strerror (NULL));
 return NULL;
}

sf_command(snd_output, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);

return snd_output;
}

SNDFILE *reopen_output(const char *file_output, SNDFILE *snd_output, SF_INFO *snd_info)
{
sf_close(snd_output);
return open_output(file_output, snd_info);
}

int main(int argc, char **argv)
{
const char *file_input, *file_output;
SNDFILE *snd_input=NULL, *snd_output=NULL;
SF_INFO info_in, info_out;
DenoiseState **st;
uint channels=1,ch;
float *data;
int split=0,splits=0;
int sframes=0,nframes=0;
float rn[4]={0.0, 0.0}, prn[4]={0.0, 0.0};
float silence=0.003;

if (argc<3) {
 fprintf(stderr, "Usage: %s input.wav output.wav\n", argv[0]);
 return 1;
}

file_input=argv[1];
file_output=argv[2];

fprintf(stderr, "Input: %s\nOutput: %s\n", file_input, file_output);

if (argc>3) {
 split=atoi(argv[3]);
 fprintf(stderr, "Split on silence frames: %d\n", split);
}

if ((snd_input=sf_open(file_input, SFM_READ, &info_in)) == NULL) {
 fprintf(stderr, "Failed to open input file: %s (%s)\n", file_input, sf_strerror (NULL));
 return 1;
}

if (info_in.channels>2) {
 fprintf(stderr, "Only mono or stereo input file supported.\n");
 return 1;
}
channels=info_in.channels;

if (info_in.samplerate!=48000) {
 fprintf(stderr, "Only 48kHz samplerate input file supported.\n");
 return 1;
}

/* Make a copy, but adjust output format to always be 16-bit PCM wav */
memcpy(&info_out, &info_in, sizeof(info_out));
info_out.format=SF_FORMAT_WAV | SF_FORMAT_PCM_16;

if ((snd_output=open_output(file_output, &info_out))==NULL) {
 fprintf(stderr, "Failed to open output file: %s (%s)\n", file_output, sf_strerror (NULL));
 return 1;
}

st=malloc(channels * sizeof(DenoiseState *));

for (ch=0;ch<channels;ch++) {
 st[ch]=rnnoise_create(NULL);
}

data=malloc(channels * FRAME_SIZE * sizeof(float));

/* Don't normalize to -1.0 - 1.0 */
sf_command(snd_input, SFC_SET_NORM_FLOAT, NULL, SF_FALSE);

while (1) {
 int r, w, frames=FRAME_SIZE, sc;
 float chd[FRAME_SIZE];

 r=sf_readf_float(snd_input, data, frames);
 if (r==0)
   break;

 for (ch=0;ch<channels;ch++) {
  for (sc=0;sc<FRAME_SIZE;sc++)
   chd[sc]=data[sc*channels+ch];

  prn[ch]=rn[ch];
  rn[ch]=rnnoise_process_frame(st[ch], chd, chd);

  for (sc=0;sc<FRAME_SIZE;sc++)
   data[sc*channels+ch]=chd[sc];
 }

 if (split>0 && prn[0]<silence && rn[0]<silence) {
  sframes++;
 } else if (split>0 && rn[0]>silence) {
  sframes=0;
  nframes++;
 }

 // fprintf(stderr, "%d/%d/%d/%f %f\n", r, w, sframes, prn[0], rn[0]);

 if (split>0 && sframes>split && nframes>0) {
  char *newfile;
  int sn;

  // fprintf(stderr, "SPLIT\n");
  splits++;

  sn=strlen(file_output)+16;

  newfile=malloc(sn);

  sn=snprintf(newfile, sn, "%s-%08d", file_output, splits);

  memcpy(&info_out, &info_in, sizeof(info_out));
  info_out.format=SF_FORMAT_WAV | SF_FORMAT_PCM_16;

  if ((snd_output=reopen_output(newfile, snd_output, &info_out))==NULL) {
    fprintf(stderr, "Failed to open split output file: %s (%s)\n", newfile, sf_strerror (NULL));
    return 1;
  }
  sframes=0;
  nframes=0;

  free(newfile);
 }

 w=sf_writef_float(snd_output, data, r);
 if (w!=r) {
  fprintf(stderr, "Output file write fail!\n");
  break;
 }

}

for (ch=0;ch<channels;ch++) {
 rnnoise_destroy(st[ch]);
}

free(data);
free(st);

sf_close(snd_input);
sf_close(snd_output);

return 0;
}

