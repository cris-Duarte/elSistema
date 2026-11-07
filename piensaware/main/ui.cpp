#include "ui.hpp"
#include "fonts/fonts.h"
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"
extern "C" { extern const lv_image_dsc_t piensa_logo; }
namespace piensaware {
namespace {
constexpr uint32_t BG=0xF7F5EF,INK=0x1D6877,ACCENT=0xD25827,MUTED=0x4D686B;
lv_obj_t* label(lv_obj_t* p,const char* text,const lv_font_t* font=&poppins_24,uint32_t color=INK) {
 auto o=lv_label_create(p);lv_label_set_text(o,text);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;
}
void on(lv_obj_t* o,std::function<void()> fn) {
 auto f=new std::function<void()>(std::move(fn));
 lv_obj_add_event_cb(o,[](lv_event_t*e){auto fn=*static_cast<std::function<void()>*>(lv_event_get_user_data(e)); fn();},LV_EVENT_CLICKED,f);
 lv_obj_add_event_cb(o,[](lv_event_t*e){delete static_cast<std::function<void()>*>(lv_event_get_user_data(e));},LV_EVENT_DELETE,f);
}
lv_obj_t* button(lv_obj_t*p,const char* text,std::function<void()> action,uint32_t bg=0xE5ECE5){
 auto b=lv_button_create(p);lv_obj_set_size(b,LV_SIZE_CONTENT,60);lv_obj_set_style_bg_color(b,lv_color_hex(bg),0);lv_obj_set_style_shadow_width(b,0,0);lv_obj_set_style_radius(b,20,0);lv_obj_set_style_pad_hor(b,24,0);auto l=label(b,text);lv_obj_center(l);on(b,std::move(action));return b;
}
lv_obj_t* panel(lv_obj_t*p,uint32_t bg=0xFFFFFF){auto o=lv_obj_create(p);lv_obj_set_style_bg_color(o,lv_color_hex(bg),0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,28,0);lv_obj_set_style_pad_all(o,28,0);return o;}

void configure_spanish_keyboard(lv_obj_t* keyboard){
 static const char* const lower[]={"1#","q","w","e","r","t","y","u","i","o","p",LV_SYMBOL_BACKSPACE,"\n","ABC","a","s","d","f","g","h","j","k","l",LV_SYMBOL_NEW_LINE,"\n","ñ","á","z","x","c","v","b","n","m","é","í","ó","\n",LV_SYMBOL_KEYBOARD,"ú"," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
 static const char* const upper[]={"1#","Q","W","E","R","T","Y","U","I","O","P",LV_SYMBOL_BACKSPACE,"\n","abc","A","S","D","F","G","H","J","K","L",LV_SYMBOL_NEW_LINE,"\n","Ñ","Á","Z","X","C","V","B","N","M","É","Í","Ó","\n",LV_SYMBOL_KEYBOARD,"Ú"," ",LV_SYMBOL_RIGHT,LV_SYMBOL_OK,""};
 static const auto widths=[] {std::array<lv_buttonmatrix_ctrl_t,40> a{};a.fill(static_cast<lv_buttonmatrix_ctrl_t>(1));a[37]=static_cast<lv_buttonmatrix_ctrl_t>(6);return a;}();
 lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_LOWER,lower,widths.data());
 lv_keyboard_set_map(keyboard,LV_KEYBOARD_MODE_TEXT_UPPER,upper,widths.data());
}

void fade(lv_obj_t*o,int delay,int duration){lv_obj_set_style_opa(o,0,0);lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,o);lv_anim_set_values(&a,0,255);lv_anim_set_duration(&a,duration);lv_anim_set_delay(&a,delay);lv_anim_set_exec_cb(&a,[](void*v,int32_t n){lv_obj_set_style_opa(static_cast<lv_obj_t*>(v),n,0);});lv_anim_start(&a);}
}
void Shell::start(lv_display_t* display) {
 display_=display;root_=lv_screen_active();lv_obj_set_style_bg_color(root_,lv_color_hex(BG),0);lv_obj_set_style_text_color(root_,lv_color_hex(INK),0);lv_obj_set_style_text_font(root_,&poppins_24,0);
 auto group=lv_obj_create(root_);lv_obj_remove_style_all(group);lv_obj_set_size(group,400,270);lv_obj_center(group);lv_obj_remove_flag(group,LV_OBJ_FLAG_SCROLLABLE);
 auto image=lv_image_create(group);lv_image_set_src(image,&piensa_logo);lv_obj_align(image,LV_ALIGN_TOP_MID,0,0);
 auto title=label(group,"Piensaware",&poppins_40);lv_obj_align(title,LV_ALIGN_BOTTOM_MID,0,-15);
 if(!prefs_.reduced_motion){fade(image,0,700);fade(title,650,650);lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,group);lv_anim_set_values(&a,20,0);lv_anim_set_duration(&a,1100);lv_anim_set_path_cb(&a,lv_anim_path_ease_out);lv_anim_set_exec_cb(&a,[](void*v,int32_t n){lv_obj_set_style_translate_y(static_cast<lv_obj_t*>(v),n,0);});lv_anim_start(&a);}
 auto t=lv_timer_create([](lv_timer_t*t){auto s=static_cast<Shell*>(lv_timer_get_user_data(t));s->boot_timer_=nullptr;s->boot_=false;s->home();},prefs_.reduced_motion?250:2100,this);lv_timer_set_repeat_count(t,1);boot_timer_=t;
 tick_timer_=lv_timer_create([](lv_timer_t*t){static_cast<Shell*>(lv_timer_get_user_data(t))->tick();},200,this);
}
void Shell::stop(){
 if(boot_timer_){lv_timer_delete(boot_timer_);boot_timer_=nullptr;}
 if(tick_timer_){lv_timer_delete(tick_timer_);tick_timer_=nullptr;}
 if(active_){active_->close();active_=nullptr;}
}
void Shell::clear(int page){
 if(active_){active_->close();active_=nullptr;}
 overlay_=password_=keyboard_=nullptr;locked_=false;clock_label_=status_=wifi_list_=message_=nullptr;
 lv_obj_clean(root_);page_=page;lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
}
void Shell::header(const char* title){
 auto h=lv_obj_create(root_);lv_obj_remove_style_all(h);lv_obj_set_size(h,lv_pct(100),104);lv_obj_set_style_pad_all(h,24,0);lv_obj_set_flex_flow(h,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(h,20,0);lv_obj_set_flex_align(h,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
 button(h,page_==0?"Piensaware":"‹ Inicio",[this]{home();});auto text=label(h,title,&poppins_28);lv_obj_set_flex_grow(text,1);
 if(page_==0)button(h,"Ajustes",[this]{settings();});
}
void Shell::home(){
 clear(0);header("");int w=lv_display_get_horizontal_resolution(display_),h=lv_display_get_vertical_resolution(display_);bool wide=w>h;
 auto hero=panel(root_,0xE5ECE5);lv_obj_set_pos(hero,32,128);lv_obj_set_size(hero,wide?int(w*.43)-40:w-64,wide?h-176:440);
 auto logo=lv_image_create(hero);lv_image_set_src(logo,&piensa_logo);lv_obj_align(logo,LV_ALIGN_TOP_LEFT,0,0);
 auto greeting=label(hero,"Un espacio\npara tus ideas",&poppins_40);lv_obj_align(greeting,LV_ALIGN_TOP_LEFT,0,180);
 clock_label_=label(hero,"",&poppins_28);lv_obj_align(clock_label_,LV_ALIGN_BOTTOM_LEFT,0,-42);
 status_=label(hero,"",&poppins_18,MUTED);lv_obj_set_width(status_,lv_pct(100));lv_label_set_long_mode(status_,LV_LABEL_LONG_DOT);lv_obj_align(status_,LV_ALIGN_BOTTOM_LEFT,0,0);
 auto area=panel(root_);lv_obj_set_pos(area,wide?int(w*.43)+16:32,wide?128:592);lv_obj_set_size(area,wide?w-int(w*.43)-48:w-64,wide?h-176:h-640);
 auto title=label(area,"Tu espacio",&poppins_28);lv_obj_align(title,LV_ALIGN_TOP_LEFT,0,0);
 if(apps_.all().empty()){
 auto mark=label(area,"+",&poppins_48,ACCENT);lv_obj_align(mark,LV_ALIGN_CENTER,0,-52);
 auto empty=label(area,"Aquí crecerán tus ideas",&poppins_28);lv_obj_set_width(empty,lv_pct(100));lv_obj_set_style_text_align(empty,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(empty,LV_ALIGN_CENTER,0,12);
 auto sub=label(area,"Pronto, nuevas apps para explorar.",&poppins_20,MUTED);lv_obj_set_width(sub,lv_pct(100));lv_obj_set_style_text_align(sub,LV_TEXT_ALIGN_CENTER,0);lv_obj_align(sub,LV_ALIGN_CENTER,0,68);
 }else{
 auto list=lv_obj_create(area);lv_obj_remove_style_all(list);lv_obj_set_pos(list,0,65);lv_obj_set_size(list,lv_pct(100),lv_pct(75));lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(list,16,0);
 for(auto&a:apps_.all()){const auto* app=&a;auto b=button(list,(a.icon+"  "+a.name).c_str(),[this,app]{open_app(*app);});lv_obj_set_width(b,lv_pct(100));}
 }
}
void Shell::notice(const char* s){if(message_)lv_label_set_text(message_,s);}
void Shell::settings(){
 clear(1);header("Ajustes");int h=lv_display_get_vertical_resolution(display_);
 auto list=panel(root_);lv_obj_set_pos(list,32,128);lv_obj_set_size(list,lv_display_get_horizontal_resolution(display_)-64,h-160);lv_obj_set_flex_flow(list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(list,24,0);
 label(list,"A tu manera",&poppins_32);
 label(list,"Brillo",&poppins_24);
 auto slider=lv_slider_create(list);lv_obj_set_size(slider,lv_pct(90),48);lv_slider_set_range(slider,10,100);lv_slider_set_value(slider,prefs_.brightness,LV_ANIM_OFF);lv_obj_set_style_bg_color(slider,lv_color_hex(INK),LV_PART_INDICATOR);lv_obj_set_style_bg_color(slider,lv_color_hex(INK),LV_PART_KNOB);
 lv_obj_add_event_cb(slider,[](lv_event_t*e){auto s=static_cast<Shell*>(lv_event_get_user_data(e));s->prefs_.brightness=lv_slider_get_value(static_cast<lv_obj_t*>(lv_event_get_target(e)));bsp_display_brightness_set(s->prefs_.brightness);},LV_EVENT_VALUE_CHANGED,this);
 lv_obj_add_event_cb(slider,[](lv_event_t*e){auto s=static_cast<Shell*>(lv_event_get_user_data(e));s->notice(s->prefs_.save()?"Preferencias guardadas":"No se pudo guardar la preferencia");},LV_EVENT_RELEASED,this);
 auto row=lv_obj_create(list);lv_obj_remove_style_all(row);lv_obj_set_size(row,lv_pct(100),64);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_flex_align(row,LV_FLEX_ALIGN_SPACE_BETWEEN,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);label(row,"Movimiento reducido");auto sw=lv_switch_create(row);lv_obj_set_size(sw,88,48);if(prefs_.reduced_motion)lv_obj_add_state(sw,LV_STATE_CHECKED);lv_obj_set_style_bg_color(sw,lv_color_hex(INK),LV_PART_INDICATOR|LV_STATE_CHECKED);
 lv_obj_add_event_cb(sw,[](lv_event_t*e){auto s=static_cast<Shell*>(lv_event_get_user_data(e));s->prefs_.reduced_motion=lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)),LV_STATE_CHECKED);s->notice(s->prefs_.save()?"Preferencias guardadas":"No se pudo guardar la preferencia");},LV_EVENT_VALUE_CHANGED,this);
 button(list,"Wi-Fi",[this]{network();});button(list,"Fecha y hora",[this]{date_time();});
 auto info=label(list,"Piensaware 0.1.0\nHecho para explorar · M5Stack Tab5\nHora local: Paraguay (UTC−3)",&poppins_20,MUTED);lv_obj_set_width(info,lv_pct(100));
 message_=label(list,prefs_.writable?"":"Almacenamiento no disponible. Los cambios serán temporales.",&poppins_18,MUTED);lv_obj_set_width(message_,lv_pct(100));
}
void Shell::network(){
 clear(2);header("Wi-Fi");int w=lv_display_get_horizontal_resolution(display_),h=lv_display_get_vertical_resolution(display_);
 auto card=panel(root_);lv_obj_set_pos(card,32,128);lv_obj_set_size(card,w-64,h-160);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(card,18,0);
 status_=label(card,"",&poppins_24);lv_obj_set_width(status_,lv_pct(100));
 button(card,"Buscar redes",[this]{if(!wifi_.scan())notice("Espera un momento e intenta otra vez.");});
 message_=label(card,"",&poppins_18,MUTED);lv_obj_set_width(message_,lv_pct(100));
 wifi_list_=lv_obj_create(card);lv_obj_remove_style_all(wifi_list_);lv_obj_set_width(wifi_list_,lv_pct(100));lv_obj_set_flex_grow(wifi_list_,1);lv_obj_set_flex_flow(wifi_list_,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(wifi_list_,16,0);
 wifi_revision_=~0u;refresh_network();wifi_.scan();
}
void Shell::refresh_network(){
 if(!wifi_list_||locked_){return;}
 auto s=wifi_.snapshot();if(wifi_revision_==s.revision)return;wifi_revision_=s.revision;lv_obj_clean(wifi_list_);
 for(auto&ssid:s.saved){auto card=panel(wifi_list_,0xE5ECE5);lv_obj_set_size(card,lv_pct(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(card,12,0);auto l=label(card,(ssid+(s.ssid==ssid?" · Conectada":" · Guardada")).c_str());lv_obj_set_width(l,lv_pct(100));
 if(s.ssid!=ssid)button(card,"Conectar",[this,ssid]{if(!wifi_.connect_known(ssid))notice("Espera y vuelve a intentar.");});
 button(card,"Olvidar red",[this,ssid]{
 if(locked_){return;}
 locked_=true;overlay_=panel(root_,0xE5ECE5);lv_obj_set_size(overlay_,lv_pct(90),300);lv_obj_center(overlay_);lv_obj_set_flex_flow(overlay_,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(overlay_,20,0);auto l=label(overlay_,("¿Olvidar "+ssid+"?").c_str());lv_obj_set_width(l,lv_pct(100));button(overlay_,"Olvidar",[this,ssid]{if(!wifi_.forget(ssid))notice("No se pudo enviar la solicitud.");dismiss();});button(overlay_,"Cancelar",[this]{dismiss();});}); }
 for(auto n:s.networks){auto text=n.ssid+(n.secured?"  ·  Protegida":"  ·  Abierta");auto b=button(wifi_list_,text.c_str(),[this,n]{password(n);},0xF0EEE7);lv_obj_set_width(b,lv_pct(100));}
 if(s.networks.empty()&&s.saved.empty())label(wifi_list_,"Las redes disponibles aparecerán aquí.",&poppins_20,MUTED);
}
void Shell::dismiss(){if(overlay_)lv_obj_delete(overlay_);overlay_=password_=keyboard_=nullptr;locked_=false;wifi_revision_=~0u;}
void Shell::password(const Network& n){
 if(locked_){return;}
 locked_=true;chosen_ssid_=n.ssid;
 overlay_=panel(root_);lv_obj_set_size(overlay_,lv_pct(96),lv_pct(96));lv_obj_center(overlay_);lv_obj_set_flex_flow(overlay_,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(overlay_,14,0);
 auto title=label(overlay_,n.ssid.c_str(),&poppins_28);lv_obj_set_width(title,lv_pct(100));
 password_=lv_textarea_create(overlay_);lv_obj_set_size(password_,lv_pct(100),60);lv_textarea_set_one_line(password_,true);lv_textarea_set_password_mode(password_,true);lv_textarea_set_max_length(password_,64);lv_textarea_set_placeholder_text(password_,n.secured?"Contraseña":"Red abierta, sin contraseña");
 auto row=lv_obj_create(overlay_);lv_obj_remove_style_all(row);lv_obj_set_size(row,lv_pct(100),64);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(row,14,0);
 button(row,"Conectar",[this,n]{std::string pass=lv_textarea_get_text(password_);if(n.secured&&pass.size()<8){lv_textarea_set_placeholder_text(password_,"Mínimo 8 caracteres");return;}if(wifi_.connect(chosen_ssid_,pass)){dismiss();}else lv_textarea_set_placeholder_text(password_,"Espera y vuelve a intentar");});
 button(row,"Cancelar",[this]{dismiss();});
 auto show=button(row,"Ver",[this]{lv_textarea_set_password_mode(password_,!lv_textarea_get_password_mode(password_));});(void)show;
 keyboard_=lv_keyboard_create(overlay_);lv_obj_set_width(keyboard_,lv_pct(100));lv_obj_set_flex_grow(keyboard_,1);lv_keyboard_set_textarea(keyboard_,password_);
 lv_obj_set_style_text_font(keyboard_,&poppins_24,LV_PART_ITEMS);
 configure_spanish_keyboard(keyboard_);
 lv_obj_add_event_cb(keyboard_,[](lv_event_t*e){static_cast<Shell*>(lv_event_get_user_data(e))->dismiss();},LV_EVENT_CANCEL,this);
}
void Shell::date_time(){
 clear(3);locked_=true;header("Fecha y hora");int w=lv_display_get_horizontal_resolution(display_),h=lv_display_get_vertical_resolution(display_);
 auto card=panel(root_);lv_obj_set_pos(card,32,128);lv_obj_set_size(card,w-64,h-160);lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(card,20,0);
 label(card,"Día / mes / año · hora / minuto",&poppins_24);
 auto row=lv_obj_create(card);lv_obj_remove_style_all(row);lv_obj_set_size(row,lv_pct(100),240);lv_obj_set_flex_flow(row,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(row,10,0);
 time_t t=clock_.now_utc()-180*60;std::tm tm{};gmtime_r(&t,&tm);
 std::array<lv_obj_t*,5> rollers{};int starts[]={1,1,2024,0,0},ends[]={31,12,2099,23,59},values[]={tm.tm_mday,tm.tm_mon+1,tm.tm_year+1900,tm.tm_hour,tm.tm_min};
 for(int i=0;i<5;i++){std::string options;for(int n=starts[i];n<=ends[i];n++){char b[16];snprintf(b,sizeof(b),i==2?"%04d":"%02d",n);if(!options.empty())options+='\n';options+=b;}rollers[i]=lv_roller_create(row);lv_roller_set_options(rollers[i],options.c_str(),LV_ROLLER_MODE_NORMAL);lv_roller_set_visible_row_count(rollers[i],3);lv_obj_set_width(rollers[i],i==2?125:82);lv_roller_set_selected(rollers[i],std::clamp(values[i],starts[i],ends[i])-starts[i],LV_ANIM_OFF);lv_obj_set_style_bg_color(rollers[i],lv_color_hex(INK),LV_PART_SELECTED);}
 button(card,"Guardar fecha y hora",[this,rollers]{int d=lv_roller_get_selected(rollers[0])+1,m=lv_roller_get_selected(rollers[1])+1,y=lv_roller_get_selected(rollers[2])+2024,hr=lv_roller_get_selected(rollers[3]),min=lv_roller_get_selected(rollers[4]);
 std::tm check{};check.tm_year=y-1900;check.tm_mon=m-1;check.tm_mday=d;mktime(&check);if(check.tm_mday!=d||check.tm_mon!=m-1){notice("Esa fecha no existe. Revisa el día y el mes.");return;}
 notice(clock_.set_local(y,m,d,hr,min,-180)?"Fecha y hora guardadas":"No se pudo guardar en el reloj del dispositivo.");});
 message_=label(card,"Hora de Paraguay · UTC−3",&poppins_20,MUTED);lv_obj_set_width(message_,lv_pct(100));
}
void Shell::open_app(const AppDescriptor& app){clear(4);active_=&app;header(app.name.c_str());auto content=lv_obj_create(root_);lv_obj_remove_style_all(content);lv_obj_set_pos(content,0,104);lv_obj_set_size(content,lv_display_get_horizontal_resolution(display_),lv_display_get_vertical_resolution(display_)-104);app.open(content);}
void Shell::tick(){
 if(boot_)return;
 if(!locked_){int r=orientation_.poll();if(r>=0&&r!=rotation_){rotation_=r;lv_display_set_rotation(display_,static_cast<lv_display_rotation_t>(r));orientation_.set_current(r);int p=page_;if(p==1)settings();else if(p==2)network();else if(p==4&&active_){const auto*a=active_;open_app(*a);}else home();}}
 auto net=wifi_.snapshot();
 if(clock_label_){if(clock_.is_valid()){auto text=clock_.format_status(clock_.now_utc(),-180);lv_label_set_text(clock_label_,text.c_str());}else lv_label_set_text(clock_label_,"Configura la fecha y hora");}
 if(status_){std::string text=net.ssid.empty()?net.status:"Wi-Fi · "+net.ssid;if(page_==0){auto power=power_.poll();if(power.state==BatteryState::Available)text+="  ·  "+std::to_string(power.battery_pct)+"%";}lv_label_set_text(status_,text.c_str());}
 if(page_==2)refresh_network();
}
}
