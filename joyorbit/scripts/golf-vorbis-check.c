/* Validate the actual pinned game decoder against our replacement effects.
 * GPL-3.0-or-later; stb retains its MIT/public-domain notices. */
#include <math.h>
#include <stdio.h>
#include "stb/stb_vorbis.h"
int main(int count,char **files) {
 if(count<2)return 1;
 for(int i=1;i<count;i++) {
  int error=0;stb_vorbis *stream=stb_vorbis_open_filename(files[i],&error,NULL);
  if(!stream||error){fprintf(stderr,"Decoder rejected effect, error %d\n",error);if(stream)stb_vorbis_close(stream);return 1;}
  stb_vorbis_info info=stb_vorbis_get_info(stream);
  unsigned expected=stb_vorbis_stream_length_in_samples(stream);
  if(info.channels!=1||info.sample_rate!=44100||expected==0||expected>44100){stb_vorbis_close(stream);return 1;}
  float buffer[512],*channels[1]={buffer};unsigned decoded=0;int got;
  while((got=stb_vorbis_get_samples_float(stream,1,channels,512))>0) {
   decoded+=(unsigned)got;
   for(int j=0;j<got;j++)if(!isfinite(buffer[j])||fabsf(buffer[j])>1.1f){stb_vorbis_close(stream);return 1;}
  }
  if(decoded!=expected||!stb_vorbis_seek(stream,0)||stb_vorbis_get_samples_float(stream,1,channels,512)<=0){stb_vorbis_close(stream);return 1;}
  stb_vorbis_close(stream);
 }
 printf("GOLF_EFFECT_DECODER_PASS %d effects\n",count-1);return 0;
}
