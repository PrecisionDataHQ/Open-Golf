/* Controller pointer, pause/return and private hole recovery for Open Golf.
 * Copyright 2026 Precision Data. GPL-3.0-or-later.
 * Open Golf's rules, physics and scoring remain in its MIT source. */
#include "common/joyorbit_golf_controls.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <jni.h>
#include <stdatomic.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"
#include "common/graphics.h"
#include "common/inputs.h"
#include "common/storage.h"
#include "golf/golf.h"
#include "golf/game.h"
enum {K_A=1,K_B=2,K_X=4,K_Y=8,K_START=16,K_SELECT=32,K_BACK=64,K_LEFT=128,K_RIGHT=256,K_UP=512,K_DOWN=1024};
static unsigned held,pressed,released;
static bool enabled,paused,quitting,restore_checked,restore_pending,suppress_a,back_long_press;
static float axis_x,axis_y,axis_rx,axis_scroll,hat_x,hat_y,cursor_x,cursor_y,save_time;
static double back_started;
static atomic_uint java_commands;
JNIEXPORT void JNICALL Java_io_precisiondata_joyorbit_golf_JoyOrbitGolfActivity_nativeBackCommand(JNIEnv *env,jclass type,jint command) {
 (void)env;(void)type;
 if(command==1||command==2)atomic_fetch_or(&java_commands,(unsigned)command);
}
static int saved_level,saved_strokes,saved_state;
static float saved_values[40];
static void set_paused(bool value);
static double now(void){struct timespec value;clock_gettime(CLOCK_MONOTONIC,&value);return value.tv_sec+value.tv_nsec/1000000000.0;}
typedef struct { const char *key;float *value;float low,high;bool timer; } field;
static int fields(field *out) {
 golf_game_t *g=golf_game_get();golf_graphics_t *graphics=golf_graphics_get();int n=0;
#define FIELD(name,low,high,timer) out[n++]=(field){"joyorbit." #name,&g->name,low,high,timer}
 FIELD(ball.pos.x,-10000,10000,false);FIELD(ball.pos.y,-10000,10000,false);FIELD(ball.pos.z,-10000,10000,false);
 FIELD(ball.vel.x,-1000,1000,false);FIELD(ball.vel.y,-1000,1000,false);FIELD(ball.vel.z,-1000,1000,false);
 FIELD(ball.draw_pos.x,-10000,10000,false);FIELD(ball.draw_pos.y,-10000,10000,false);FIELD(ball.draw_pos.z,-10000,10000,false);
 FIELD(ball.rot_vec.x,-10000,10000,false);FIELD(ball.rot_vec.y,-10000,10000,false);FIELD(ball.rot_vec.z,-10000,10000,false);
 FIELD(ball.rot_vel,-10000,10000,false);
 FIELD(ball.orientation.x,-1.1f,1.1f,false);FIELD(ball.orientation.y,-1.1f,1.1f,false);FIELD(ball.orientation.z,-1.1f,1.1f,false);FIELD(ball.orientation.w,-1.1f,1.1f,false);
 FIELD(ball.time_since_water_ripple,0,1000000,true);FIELD(ball.time_going_slow,0,1000000,true);
 FIELD(ball.time_out_of_bounds,0,1000000,true);FIELD(ball.time_out_of_water,0,1000000,true);FIELD(ball.time_since_impact_sound,0,1000000,true);
 FIELD(cam.angle,-10000,10000,false);
#undef FIELD
#define CAMERA(name,low,high) out[n++]=(field){"joyorbit.camera." #name,&graphics->name,low,high,false}
 CAMERA(cam_pos.x,-10000,10000);CAMERA(cam_pos.y,-10000,10000);CAMERA(cam_pos.z,-10000,10000);
 CAMERA(cam_dir.x,-2,2);CAMERA(cam_dir.y,-2,2);CAMERA(cam_dir.z,-2,2);
 CAMERA(cam_up.x,-2,2);CAMERA(cam_up.y,-2,2);CAMERA(cam_up.z,-2,2);
#undef CAMERA
 return n;
}
static bool read_number(const char *key,float low,float high,float *out) {
 return golf_storage_get_num(key,out)&&isfinite(*out)&&*out>=low&&*out<=high;
}
static bool read_integer(const char *key,int low,int high,int *out) {
 float value;if(!read_number(key,(float)low,(float)high,&value)||floorf(value)!=value)return false;
 *out=(int)value;return true;
}
static void check_restore(void) {
 restore_checked=true;int version=0,active=0;
 if(!read_integer("joyorbit.version",1,1,&version)||!read_integer("joyorbit.active",1,1,&active))return;
 golf_config_t *config=golf_data_get_config("data/config/game.cfg");
 int levels=(int)golf_config_get_num(config,"num_levels");
 if(!read_integer("joyorbit.level",0,levels-1,&saved_level)||!read_integer("joyorbit.strokes",0,100000,&saved_strokes)||!read_integer("joyorbit.state",1,2,&saved_state))return;
 field list[40];int n=fields(list);
 for(int i=0;i<n;i++)if(!read_number(list[i].key,list[i].low,list[i].high,&saved_values[i]))return;
 for(int i=0;i<4;i++){int value;if(!read_integer((const char*[]){"joyorbit.moving","joyorbit.water","joyorbit.out","joyorbit.hole"}[i],0,1,&value))return;saved_values[n+i]=(float)value;}
 float norm=0;for(int i=13;i<17;i++)norm+=saved_values[i]*saved_values[i];
 if(norm<.5f||norm>1.5f)return;
 vec3 direction=V3(saved_values[26],saved_values[27],saved_values[28]),up=V3(saved_values[29],saved_values[30],saved_values[31]);
 if(vec3_length(direction)<.5f||vec3_length(up)<.5f||vec3_length(vec3_cross(direction,up))<.1f)return;
 restore_pending=true;
}
static void restore_hole(void) {
 golf_game_t *g=golf_game_get();field list[40];int n=fields(list);
 for(int i=0;i<n;i++)*list[i].value=saved_values[i];
 g->stroke_count=saved_strokes;
 g->ball.is_moving=saved_values[n]!=0;g->ball.is_in_water=saved_values[n+1]!=0;
 g->ball.is_out_of_bounds=saved_values[n+2]!=0;g->ball.is_in_hole=saved_values[n+3]!=0;
 g->state=saved_state==2?GOLF_GAME_STATE_WATCHING_BALL:GOLF_GAME_STATE_WAITING_FOR_AIM;
 g->physics.time_behind=0;g->cam.auto_rotate=false;g->cam.angle_velocity=0;g->cam.start_angle=g->cam.angle;
 restore_pending=false;
 set_paused(true);
}
void joyorbit_golf_save(void) {
 if(restore_pending)return;
 golf_t *golf=golf_get();golf_game_t *g=golf_game_get();
 golf_game_state_t state=g->state==GOLF_GAME_STATE_PAUSED?g->state_before_pause:g->state;
 bool active=golf->state==GOLF_STATE_IN_GAME&&(state==GOLF_GAME_STATE_WAITING_FOR_AIM||state==GOLF_GAME_STATE_AIMING||state==GOLF_GAME_STATE_WATCHING_BALL);
 if(active) {
  field list[40];int n=fields(list);
  for(int i=0;i<n;i++)if(!isfinite(*list[i].value)||(!list[i].timer&&(*list[i].value<list[i].low||*list[i].value>list[i].high)))return;
  golf_storage_set_num("joyorbit.version",1);golf_storage_set_num("joyorbit.active",1);
  golf_storage_set_num("joyorbit.level",(float)golf->level_num);golf_storage_set_num("joyorbit.strokes",(float)g->stroke_count);
  golf_storage_set_num("joyorbit.state",state==GOLF_GAME_STATE_WATCHING_BALL?2:1);
  for(int i=0;i<n;i++)golf_storage_set_num(list[i].key,list[i].timer?fminf(list[i].high,fmaxf(0,*list[i].value)):*list[i].value);
  golf_storage_set_num("joyorbit.moving",g->ball.is_moving?1:0);golf_storage_set_num("joyorbit.water",g->ball.is_in_water?1:0);
  golf_storage_set_num("joyorbit.out",g->ball.is_out_of_bounds?1:0);golf_storage_set_num("joyorbit.hole",g->ball.is_in_hole?1:0);
 } else if(!restore_pending&&(golf->state==GOLF_STATE_MAIN_MENU||state==GOLF_GAME_STATE_FINISHED||state==GOLF_GAME_STATE_CELEBRATION))golf_storage_set_num("joyorbit.active",0);
 golf_storage_save();
}
static float axis(float value) {return isfinite(value)&&fabsf(value)>.18f?fminf(1,fmaxf(-1,value)):0;}
void joyorbit_golf_pad_axes(float x,float y,float rx,float scroll,float hx,float hy) {
 enabled=true;axis_x=axis(x);axis_y=axis(y);axis_rx=axis(rx);axis_scroll=axis(scroll);hat_x=axis(hx);hat_y=axis(hy);
}
bool joyorbit_golf_pad_key(int key,bool down,bool long_press) {
 unsigned bit=0;
 switch(key) {
  case 96:case 23:case 66:bit=K_A;break;case 97:bit=K_B;break;case 99:bit=K_X;break;case 100:bit=K_Y;break;
  case 108:bit=K_START;break;case 109:bit=K_SELECT;break;case 4:bit=K_BACK;break;
  case 21:bit=K_LEFT;break;case 22:bit=K_RIGHT;break;case 19:bit=K_UP;break;case 20:bit=K_DOWN;break;
  default:return false;
 }
 enabled=true;
 if(down) {if(!(held&bit)){pressed|=bit;if(bit==K_BACK){back_started=now();back_long_press=false;}}held|=bit;}else{if(held&bit)released|=bit;held&=~bit;}
 if(key==4&&long_press)back_long_press=true;
 return true;
}
static void set_paused(bool value) {
 if(value==paused)return;
 golf_game_t *g=golf_game_get();
 if(value) {
  if(g->state==GOLF_GAME_STATE_AIMING)g->state=GOLF_GAME_STATE_WAITING_FOR_AIM;
  if(g->state!=GOLF_GAME_STATE_PAUSED)golf_game_pause();
  joyorbit_golf_save();
 } else if(g->state==GOLF_GAME_STATE_PAUSED)golf_game_resume();
 paused=value;suppress_a=true;
}
void joyorbit_golf_control_event(const sapp_event *event) {
 if(event->type==SAPP_EVENTTYPE_SUSPENDED){set_paused(true);joyorbit_golf_save();}
 if(event->type==SAPP_EVENTTYPE_TOUCHES_BEGAN||event->type==SAPP_EVENTTYPE_TOUCHES_MOVED||event->type==SAPP_EVENTTYPE_TOUCHES_ENDED||event->type==SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
  enabled=false;
  ImGuiIO *io=igGetIO();
  if(event->num_touches>0)io->MousePos=(ImVec2){event->touches[0].pos_x,event->touches[0].pos_y};
  io->MouseDown[0]=event->type==SAPP_EVENTTYPE_TOUCHES_BEGAN||event->type==SAPP_EVENTTYPE_TOUCHES_MOVED;
 }
}
bool joyorbit_golf_is_paused(void){return paused;}
static void mouse_event(sapp_event_type type) {
 sapp_event event={0};event.type=type;event.mouse_x=cursor_x;event.mouse_y=cursor_y;event.mouse_button=SAPP_MOUSEBUTTON_LEFT;
 golf_inputs_get()->is_touch=false;golf_inputs_handle_event(&event);
}
bool joyorbit_golf_control_frame(float dt) {
 unsigned commands=atomic_exchange(&java_commands,0);
 if(commands&1)quitting=true;
 if(commands&2)set_paused(!paused);
 if(!restore_checked){
  ImGuiStyle *style=igGetStyle();style->WindowRounding=14;style->FrameRounding=12;
  style->Colors[ImGuiCol_WindowBg]=(ImVec4){.04f,.08f,.13f,1};style->Colors[ImGuiCol_Button]=(ImVec4){.08f,.17f,.22f,1};
  style->Colors[ImGuiCol_ButtonHovered]=(ImVec4){.14f,.31f,.29f,1};style->Colors[ImGuiCol_ButtonActive]=(ImVec4){.20f,.40f,.34f,1};
  style->Colors[ImGuiCol_Text]=(ImVec4){.91f,.96f,.98f,1};check_restore();
 }
 golf_t *golf=golf_get();golf_game_t *g=golf_game_get();
 if(restore_pending) {
  if(golf->state==GOLF_STATE_MAIN_MENU)golf_start_level(saved_level);
  else if(golf->state==GOLF_STATE_IN_GAME&&golf->level_num==saved_level&&g->state==GOLF_GAME_STATE_WAITING_FOR_AIM)restore_hole();
 }
 if(g->state==GOLF_GAME_STATE_PAUSED&&!paused)paused=true;
 float step=fminf(.05f,fmaxf(0,dt));
 if((held&(K_START|K_SELECT))==(K_START|K_SELECT))quitting=true;
 if(released&K_BACK){if(back_long_press||now()-back_started>.65)quitting=true;else set_paused(!paused);}
 if(pressed&(K_START|K_B))set_paused(!paused);
 if(paused&&pressed&K_X)quitting=true;
 if(quitting){joyorbit_golf_save();sapp_quit();return true;}
 if(paused&&pressed&K_A)set_paused(false);
 golf_inputs_t *input=golf_inputs_get();
 if(cursor_x==0&&cursor_y==0){cursor_x=sapp_width()*.5f;cursor_y=sapp_height()*.6f;}
 if(enabled&&!paused) {
  float speed=450*step*(float)sapp_height()/720;
  cursor_x+=speed*(axis_x+hat_x+((held&K_RIGHT)?1:0)-((held&K_LEFT)?1:0));
  cursor_y+=speed*(axis_y+hat_y+((held&K_DOWN)?1:0)-((held&K_UP)?1:0));
  cursor_x=fminf(sapp_width()-5,fmaxf(5,cursor_x));cursor_y=fminf(sapp_height()-5,fmaxf(5,cursor_y));
  if(pressed&K_X&&golf->state==GOLF_STATE_IN_GAME) {
   vec2 point=golf_graphics_world_to_screen(g->ball.pos);cursor_x=point.x;cursor_y=point.y;
  }
  mouse_event(SAPP_EVENTTYPE_MOUSE_MOVE);
  if(!suppress_a&&pressed&K_A)mouse_event(SAPP_EVENTTYPE_MOUSE_DOWN);
  if(!suppress_a&&released&K_A)mouse_event(SAPP_EVENTTYPE_MOUSE_UP);
  input->mouse_scroll_delta.y=-axis_scroll*12*step;
  if(golf->state==GOLF_STATE_IN_GAME&&axis_rx!=0){g->cam.auto_rotate=false;g->cam.angle+=axis_rx*step*1.8f;g->cam.angle_velocity=0;g->cam.start_angle=g->cam.angle;}
 }
 if(suppress_a&&!(held&K_A)){suppress_a=false;input->mouse_down[0]=false;input->mouse_clicked[0]=false;}
 if(paused){input->touch_down=false;input->touch_began=false;input->touch_ended=false;memset(input->mouse_down,0,sizeof(input->mouse_down));memset(input->mouse_clicked,0,sizeof(input->mouse_clicked));}
 pressed=0;released=0;
 save_time+=dt;if(save_time>2){joyorbit_golf_save();save_time=0;}
 return false;
}
void joyorbit_golf_control_draw(void) {
 float width=(float)sapp_width(),height=(float)sapp_height();
 if(paused) {
  ImDrawList_AddRectFilled(igGetBackgroundDrawList_Nil(),(ImVec2){0,0},(ImVec2){width,height},0xd909111c,0,0);
  igSetNextWindowPos((ImVec2){width*.5f,height*.5f},ImGuiCond_Always,(ImVec2){.5f,.5f});
  igSetNextWindowSize((ImVec2){width*.62f,height*.55f},ImGuiCond_Always);
  igSetNextWindowBgAlpha(1);
  igBegin("JoyOrbit Golf pause",NULL,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoCollapse);
  igSetWindowFontScale(fmaxf(1.6f,height/400));
  igTextUnformatted("Paused",NULL);igSeparator();
  igTextWrapped("A / B / Start: resume\nX: return to library\nLong Back or Start + Select: return");
  if(igButton("Resume",(ImVec2){width*.5f,65}))set_paused(false);
  if(igButton("Return to library",(ImVec2){width*.5f,65}))quitting=true;
  igEnd();
 } else if(enabled) {
  ImDrawList *draw=igGetForegroundDrawList_Nil();ImVec2 position={cursor_x,cursor_y};
  ImDrawList_AddCircle(draw,position,12,0xffe9f4ef,20,3);
  ImDrawList_AddLine(draw,(ImVec2){cursor_x-18,cursor_y},(ImVec2){cursor_x+18,cursor_y},0xffb7df8b,2);
  ImDrawList_AddLine(draw,(ImVec2){cursor_x,cursor_y-18},(ImVec2){cursor_x,cursor_y+18},0xffb7df8b,2);
 }
}
