#include "ui.hpp"
#include "fonts/fonts.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace piensaware;
namespace piensaware {
struct ShellProbe {
 static void home(Shell&s){s.boot_=false;s.home();s.tick();}
 static void settings(Shell&s){s.settings();}
 static void network(Shell&s){s.network();s.tick();}
 static void password(Shell&s){s.password({"Red de prueba con nombre largo",-50,true});}
 static void date(Shell&s){s.dismiss();s.date_time();}
 static void dismiss(Shell&s){s.dismiss();}
 static void tick(Shell&s){s.tick();}
 static bool locked(Shell&s){return s.locked_;}
 static void app(Shell&s,const AppDescriptor&a){s.open_app(a);}
};
}
static std::vector<uint8_t> pixels;
static int width,height;
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*buf){
 int stride=(a->x2-a->x1+1)*3;
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){
 auto src=buf+(y-a->y1)*stride+(x-a->x1)*3;auto dst=pixels.data()+(y*width+x)*3;
 dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];}
 lv_display_flush_ready(d);
}
static void capture(const char*name){
 lv_obj_invalidate(lv_screen_active());lv_refr_now(nullptr);
 char path[200];snprintf(path,sizeof(path),"%s-%dx%d.ppm",name,width,height);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",width,height);fwrite(pixels.data(),1,pixels.size(),f);fclose(f);
}
int main(){
 lv_init();
 Preferences prefs;WifiService wifi;ClockService clock;OrientationService orientation;PowerService power;AppRegistry apps;
 assert(!apps.add({}));
 int opens=0,closes=0;AppDescriptor sample{"sample","Ejemplo","+",[&](lv_obj_t*root){opens++;lv_label_set_text(lv_label_create(root),"Mi primera app");},[&]{closes++;}};
 assert(apps.add(sample));assert(!apps.add(sample));
 for(auto size:{std::pair<int,int>{1280,720},{720,1280}}){
 width=size.first;height=size.second;pixels.resize(width*height*3);
 auto*d=lv_display_create(width,height);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);
 std::vector<uint8_t> buffer(width*height*3);lv_display_set_buffers(d,buffer.data(),nullptr,buffer.size(),LV_DISPLAY_RENDER_MODE_FULL);lv_display_set_flush_cb(d,flush);
 AppRegistry empty;Shell shell(prefs,wifi,clock,orientation,power,empty);shell.start(d);
 lv_tick_inc(1400);lv_timer_handler();capture("boot");
 ShellProbe::home(shell);capture("home");ShellProbe::settings(shell);capture("settings");
 wifi.state.networks={{"Piensa",-50,true},{"Una red con nombre muy largo 123",-72,true}};wifi.state.saved={"Mi casa"};wifi.state.revision++;
 ShellProbe::network(shell);capture("wifi");ShellProbe::password(shell);assert(ShellProbe::locked(shell));capture("keyboard");
 ShellProbe::date(shell);assert(ShellProbe::locked(shell));capture("date");ShellProbe::home(shell);
 ShellProbe::app(shell,sample);assert(opens==closes+1);ShellProbe::home(shell);assert(opens==closes);
 for(int i=0;i<50;i++){ShellProbe::settings(shell);ShellProbe::network(shell);ShellProbe::password(shell);ShellProbe::dismiss(shell);ShellProbe::home(shell);}
 for(auto font:{&poppins_24,&poppins_40}){lv_font_glyph_dsc_t g;for(uint32_t cp:{0xF1u,0xE1u,0xE9u})assert(lv_font_get_glyph_dsc(font,&g,cp,0));}
 // Timers retain the shell pointer; delete before it leaves scope.
 shell.stop();
 lv_display_delete(d);
 printf("%dx%d: navegación, diálogo, fuentes y ciclo de apps OK\n",width,height);
 }
 lv_deinit();
}
