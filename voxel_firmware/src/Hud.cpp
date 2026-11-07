#include "Hud.h"
#include "Renderer.h"
#include <algorithm>
#include <cstdio>
namespace voxel {
namespace {
// Original 3x5 bitmap alphabet, stored in read-only memory. No font engine or
// strings allocated in the frame loop. Bits are rows, high bit on the left.
struct Glyph { char c; uint16_t bits; };
constexpr Glyph Font[]={
 {'A',0b010101111101101},{'B',0b110101110101110},{'C',0b011100100100011},
 {'D',0b110101101101110},{'E',0b111100110100111},{'F',0b111100110100100},
 {'G',0b011100101101011},{'H',0b101101111101101},{'I',0b111010010010111},
 {'J',0b001001001101010},{'K',0b101101110101101},{'L',0b100100100100111},
 {'M',0b101111111101101},{'N',0b101111111111101},{'O',0b010101101101010},
 {'P',0b110101110100100},{'Q',0b010101101111011},{'R',0b110101110101101},
 {'S',0b011100010001110},{'T',0b111010010010010},{'U',0b101101101101111},
 {'V',0b101101101101010},{'W',0b101101111111101},{'X',0b101101010101101},
 {'Y',0b101101010010010},{'Z',0b111001010100111},
 {'0',0b111101101101111},{'1',0b010110010010111},{'2',0b110001010100111},
 {'3',0b110001010001110},{'4',0b101101111001001},{'5',0b111100110001110},
 {'6',0b011100111101111},{'7',0b111001010010010},{'8',0b111101111101111},
 {'9',0b111101111001110},{'.',0b000000000000010},{':',0b000010000010000},
 {'-',0b000000111000000},{'/',0b001001010100100},{'+',0b000010111010000}
};
}
void Hud::rect(int x,int y,int w,int h,uint16_t color,bool fill) {
    const int left=std::max(0,x),top=std::max(0,y),right=std::min(cfg::Width,x+w),bottom=std::min(cfg::Height,y+h);
    for(int py=top;py<bottom;++py) for(int px=left;px<right;++px)
        if(fill || px==x || px==x+w-1 || py==y || py==y+h-1) pixels_[py*cfg::Width+px]=color;
}
void Hud::text(int x,int y,const char* value,uint16_t color) {
    for(;*value;++value,x+=4) {
        uint16_t bits=0;
        for(const auto& g:Font) if(g.c==*value) { bits=g.bits; break; }
        for(int row=0;row<5;++row) for(int col=0;col<3;++col)
            if(bits&(1u<<(14-row*3-col))) rect(x+col,y+row,1,1,color);
    }
}
void Hud::pattern(uint32_t frame) {
    constexpr uint16_t colors[]={0xf800,0x07e0,0x001f,0xffff,0x0000,0xffe0,0xf81f,0x07ff};
    for(int y=0;y<cfg::Height;++y) for(int x=0;x<cfg::Width;++x)
        pixels_[y*cfg::Width+x]=colors[x*8/cfg::Width];
    rect(static_cast<int>(frame%cfg::Width),0,2,cfg::Height,0xffff);
    text(2,12,"DSI TEST - TEST PARA SALIR");
    text(2,20,"ROJO VERDE AZUL BLANCO");
    text(2,28,"ARRIBA"); text(2,cfg::Height-6,"ABAJO");
}
void Hud::draw(const Game& game,const Diagnostics& stats) {
    const auto X=[](int x){return x*cfg::Width/cfg::ScreenW;};
    const auto Y=[](int y){return y*cfg::Height/cfg::ScreenH;};
    const uint16_t panel=rgb565(22,30,38);
    rect(0,0,cfg::Width,8,panel); text(2,1,"TEST"); text(cfg::Width-17,1,"MENU");
    char line[64]; std::snprintf(line,sizeof(line),"FPS %.0f R%.0f P%.0f",stats.fps,stats.renderMs,stats.presentMs);
    text(22,1,line);
    if(game.diagnostic) {
        rect(0,Y(300),cfg::Width,30,panel);
        std::snprintf(line,sizeof(line),"PSRAM %u K",static_cast<unsigned>(stats.freePsram/1024)); text(2,Y(305),line);
        std::snprintf(line,sizeof(line),"HEAP %u MIN %u",static_cast<unsigned>(stats.freeInternal/1024),static_cast<unsigned>(stats.minInternal/1024)); text(2,Y(365),line);
        std::snprintf(line,sizeof(line),"TACTOS %u P95 %.0f",stats.contacts,stats.frameP95); text(2,Y(425),line);
        return;
    }
    const int cx=cfg::Width/2,cy=cfg::Height/2;
    const uint16_t cross=game.target.valid?0xffe0:0xffff;
    rect(cx-2,cy,5,1,cross); rect(cx,cy-2,1,5,cross);
    rect(X(55),Y(425),X(250),Y(250),panel,false);
    rect(X(175),Y(435),1,Y(230),0xffff,false);
    rect(X(65),Y(545),X(230),1,0xffff,false);
    text(X(95),Y(680),"MOVER");
    const auto button=[&](int x,int y,int w,int h,const char* label) {
        rect(X(x),Y(y),X(w),Y(h),panel); rect(X(x),Y(y),X(w),Y(h),0xffff,false);
        text(X(x+12),Y(y+h/2)-2,label);
    };
    button(1080,400,190,140,"SALTO"); button(930,560,130,150,"MINA"); button(1080,560,190,150,"PONER");
    for(int i=0;i<6;++i) {
        const int x=X(400+i*80),y=Y(610),w=X(80),h=Y(100);
        rect(x,y,w,h,Renderer::color(static_cast<Block>(i+1)));
        rect(x,y,w,h,game.selected==static_cast<Block>(i+1)?0xffff:panel,false);
        const char label[]={static_cast<char>('1'+i),0}; text(x+3,y+3,label);
    }
    if(game.paused) {
        rect(X(200),Y(150),X(880),Y(400),panel);
        text(X(345),Y(190),"TAB5 VOXEL");
        text(X(265),Y(250),"CREATIVO - SIN GUARDADO");
        button(440,280,400,130,"JUGAR");
        text(X(245),Y(445),"EL MUNDO SE REINICIA AL APAGAR");
    }
}
}
