/* JoyOrbit font-cache builder. GPL-3.0-or-later.
 * Atlas layout follows Open Golf's MIT importer, copyright 2020 Michael Gerdes.
 * stb and parson retain their original notices in the pinned game source. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "parson/parson.h"
typedef struct { unsigned char *bytes; size_t length; } buffer;
static void append(void *context,void *data,int size) {
 buffer *b=context;
 unsigned char *next=realloc(b->bytes,b->length+(size_t)size);
 if (!next) exit(2);
 b->bytes=next;memcpy(b->bytes+b->length,data,(size_t)size);b->length+=(size_t)size;
}
static char *encode(const unsigned char *bytes,size_t length) {
 const char *alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 char *out=malloc(4*((length+2)/3)+1);if(!out)exit(2);
 size_t written=0;
 for(size_t i=0;i<length;i+=3) {
  unsigned int value=(unsigned int)bytes[i]<<16;
  if(i+1<length)value|=(unsigned int)bytes[i+1]<<8;
  if(i+2<length)value|=bytes[i+2];
  out[written++]=alphabet[(value>>18)&63];out[written++]=alphabet[(value>>12)&63];
  out[written++]=i+1<length?alphabet[(value>>6)&63]:'=';out[written++]=i+2<length?alphabet[value&63]:'=';
 }
 out[written]=0;return out;
}
static JSON_Value *atlas(const unsigned char *font,int size,int dimension) {
 unsigned char *bitmap=calloc((size_t)dimension*(size_t)dimension,1);
 if(!bitmap)exit(2);
 stbtt_bakedchar chars[96]={0};
 if(stbtt_BakeFontBitmap(font,0,(float)-size,bitmap,dimension,dimension,32,95,chars)<=0) {
  fprintf(stderr,"Font does not fit atlas at %d\n",size);exit(3);
 }
 float ascent,descent,linegap;stbtt_GetScaledFontVMetrics(font,0,(float)-size,&ascent,&descent,&linegap);
 JSON_Value *value=json_value_init_object();JSON_Object *object=json_value_get_object(value);
 json_object_set_number(object,"font_size",size);json_object_set_number(object,"ascent",ascent);
 json_object_set_number(object,"descent",descent);json_object_set_number(object,"linegap",linegap);json_object_set_number(object,"img_size",dimension);
 JSON_Value *list=json_value_init_array();JSON_Array *array=json_value_get_array(list);
 for(int i=0;i<96;i++) {
  JSON_Value *glyph=json_value_init_object();JSON_Object *record=json_value_get_object(glyph);
  json_object_set_number(record,"c",32+i);json_object_set_number(record,"x0",chars[i].x0);json_object_set_number(record,"x1",chars[i].x1);
  json_object_set_number(record,"y0",chars[i].y0);json_object_set_number(record,"y1",chars[i].y1);
  json_object_set_number(record,"xoff",chars[i].xoff);json_object_set_number(record,"yoff",chars[i].yoff);json_object_set_number(record,"xadvance",chars[i].xadvance);
  json_array_append_value(array,glyph);
 }
 json_object_set_value(object,"char_datas",list);
 buffer png={0};stbi_write_png_to_func(append,&png,dimension,dimension,1,bitmap,dimension);
 char *encoded=encode(png.bytes,png.length);json_object_set_string(object,"img_data",encoded);
 free(encoded);free(png.bytes);free(bitmap);return value;
}
int main(int count,char **args) {
 if(count!=3)return 1;
 FILE *input=fopen(args[1],"rb");if(!input)return 1;
 fseek(input,0,SEEK_END);long length=ftell(input);rewind(input);
 if(length<12||length>4*1024*1024){fclose(input);return 1;}
 unsigned char *font=malloc((size_t)length);if(!font)return 2;
 if(fread(font,1,(size_t)length,input)!=(size_t)length){fclose(input);free(font);return 1;}fclose(input);
 stbtt_fontinfo info;if(!stbtt_InitFont(&info,font,stbtt_GetFontOffsetForIndex(font,0))){free(font);return 1;}
 JSON_Value *root=json_value_init_object(),*list=json_value_init_array();JSON_Array *array=json_value_get_array(list);
 const int sizes[8]={16,24,32,40,48,56,64,128},dimensions[8]={256,256,256,512,512,512,512,1024};
 for(int i=0;i<8;i++)json_array_append_value(array,atlas(font,sizes[i],dimensions[i]));
 json_object_set_value(json_value_get_object(root),"atlases",list);
 int result=json_serialize_to_file(root,args[2])==JSONSuccess?0:1;
 json_value_free(root);free(font);return result;
}
