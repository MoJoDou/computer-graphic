/**
 ФИО: Рыскулов Нияз Талантбекович
 Группа: ЕПИ-2-23
Задание: 4 / OpenGL: Виртуальный Город
 
  Управление:
    W/S/A/D   — поворот свободной камеры
   +/-       — зум
  ПРОБЕЛ    — пауза/запуск анимации
   1-6       — переключение камер
   ESC       — выход
 */

#ifdef _WIN32
  #include <windows.h>
#endif
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 
   ГЛОБАЛЬНОЕ СОСТОЯНИЕ
    */
static float camAngleX = 38.0f;
static float camAngleY = 25.0f;
static float camDist   = 115.0f;
static int   camMode   = 0;
static bool  paused    = false;
static float sunAngle  = 60.0f;
static float timeOfDay = 0.8f;
static float carAngle  = 0.0f;
static float car2Angle = 180.0f;

/* 
   ТЕКСТУРЫ
    */
enum { TEX_WIN_BLUE=0, TEX_WIN_BEIGE, TEX_WIN_GREEN, TEX_WIN_GRAY,
       TEX_GRASS, TEX_ASPHALT, TEX_ROOF, TEX_COUNT };
static GLuint texID[TEX_COUNT];

static void makeWindowTex(GLuint id, float wr, float wg, float wb)
{
    const int W=64, H=128;
    static unsigned char buf[64*128*3];
    float t = timeOfDay;
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        int px=x%8, py=y%16;
        bool win=(px>=1&&px<=5&&py>=2&&py<=12);
        float r,g,b;
        if(win){
            /* днём — голубоватые окна, ночью — жёлтый свет */
            r = wr*t + 0.95f*(1-t);
            g = wg*t + 0.85f*(1-t);
            b = wb*t + 0.25f*(1-t);
        } else {
            r=wr*0.55f; g=wg*0.55f; b=wb*0.55f;
        }
        int i=(y*W+x)*3;
        buf[i+0]=(unsigned char)(r*255);
        buf[i+1]=(unsigned char)(g*255);
        buf[i+2]=(unsigned char)(b*255);
    }
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,W,H,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
}

static void makeGrassTex(GLuint id){
    const int W=64,H=64; static unsigned char buf[64*64*3]; srand(42);
    for(int i=0;i<W*H;i++){
        float v=0.45f+(rand()%30)/100.0f;
        buf[i*3+0]=(unsigned char)(0.15f*v*255);
        buf[i*3+1]=(unsigned char)(0.82f*v*255);
        buf[i*3+2]=(unsigned char)(0.15f*v*255);
    }
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,W,H,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
}

static void makeAsphaltTex(GLuint id){
    const int W=64,H=64; static unsigned char buf[64*64*3]; srand(7);
    for(int i=0;i<W*H;i++){
        float v=0.22f+(rand()%12)/100.0f;
        buf[i*3+0]=buf[i*3+1]=buf[i*3+2]=(unsigned char)(v*255);
    }
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,W,H,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
}

static void makeRoofTex(GLuint id){
    const int W=32,H=32; static unsigned char buf[32*32*3];
    for(int y=0;y<H;y++) for(int x=0;x<W;x++){
        float row=(float)(y%8)/8.0f;
        float v=0.55f+0.15f*row;
        int i=(y*W+x)*3;
        buf[i+0]=(unsigned char)(0.72f*v*255);
        buf[i+1]=(unsigned char)(0.18f*v*255);
        buf[i+2]=(unsigned char)(0.08f*v*255);
    }
    glBindTexture(GL_TEXTURE_2D,id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,W,H,0,GL_RGB,GL_UNSIGNED_BYTE,buf);
}

static void buildTextures(){
    glGenTextures(TEX_COUNT,texID);
    makeWindowTex(texID[TEX_WIN_BLUE],  0.55f,0.70f,0.90f);
    makeWindowTex(texID[TEX_WIN_BEIGE], 0.82f,0.78f,0.68f);
    makeWindowTex(texID[TEX_WIN_GREEN], 0.55f,0.75f,0.55f);
    makeWindowTex(texID[TEX_WIN_GRAY],  0.72f,0.68f,0.58f);
    makeGrassTex (texID[TEX_GRASS]);
    makeAsphaltTex(texID[TEX_ASPHALT]);
    makeRoofTex  (texID[TEX_ROOF]);
}

static void rebuildWinTextures(){
    makeWindowTex(texID[TEX_WIN_BLUE],  0.55f,0.70f,0.90f);
    makeWindowTex(texID[TEX_WIN_BEIGE], 0.82f,0.78f,0.68f);
    makeWindowTex(texID[TEX_WIN_GREEN], 0.55f,0.75f,0.55f);
    makeWindowTex(texID[TEX_WIN_GRAY],  0.72f,0.68f,0.58f);
}

/* 
   МАТЕРИАЛ
    */
static void setMat(float r,float g,float b,float sh=20.0f){
    float a[4]={r*.3f,g*.3f,b*.3f,1},d[4]={r,g,b,1},s[4]={.4f,.4f,.4f,1};
    glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,a);
    glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,d);
    glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,s);
    glMaterialf (GL_FRONT_AND_BACK,GL_SHININESS,sh);
}

/* 
   ПРИМИТИВЫ
    */
static void drawBox(float cx,float by,float cz,float w,float h,float d){
    float x0=cx-w*.5f,x1=cx+w*.5f,y0=by,y1=by+h,z0=cz-d*.5f,z1=cz+d*.5f;
    glBegin(GL_QUADS);
    glNormal3f(0,0,1); glVertex3f(x0,y0,z1);glVertex3f(x1,y0,z1);glVertex3f(x1,y1,z1);glVertex3f(x0,y1,z1);
    glNormal3f(0,0,-1);glVertex3f(x1,y0,z0);glVertex3f(x0,y0,z0);glVertex3f(x0,y1,z0);glVertex3f(x1,y1,z0);
    glNormal3f(-1,0,0);glVertex3f(x0,y0,z0);glVertex3f(x0,y0,z1);glVertex3f(x0,y1,z1);glVertex3f(x0,y1,z0);
    glNormal3f(1,0,0); glVertex3f(x1,y0,z1);glVertex3f(x1,y0,z0);glVertex3f(x1,y1,z0);glVertex3f(x1,y1,z1);
    glNormal3f(0,1,0); glVertex3f(x0,y1,z0);glVertex3f(x0,y1,z1);glVertex3f(x1,y1,z1);glVertex3f(x1,y1,z0);
    glNormal3f(0,-1,0);glVertex3f(x0,y0,z1);glVertex3f(x0,y0,z0);glVertex3f(x1,y0,z0);glVertex3f(x1,y0,z1);
    glEnd();
}

/* Параллелепипед с текстурой окон */
static void drawBoxTex(float cx,float by,float cz,float w,float h,float d,GLuint tex){
    float x0=cx-w*.5f,x1=cx+w*.5f,y0=by,y1=by+h,z0=cz-d*.5f,z1=cz+d*.5f;
    float uW=w/4.0f,uD=d/4.0f,vH=h/4.0f;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBegin(GL_QUADS);
    glNormal3f(0,0,1);
    glTexCoord2f(0,0);glVertex3f(x0,y0,z1); glTexCoord2f(uW,0);glVertex3f(x1,y0,z1);
    glTexCoord2f(uW,vH);glVertex3f(x1,y1,z1); glTexCoord2f(0,vH);glVertex3f(x0,y1,z1);
    glNormal3f(0,0,-1);
    glTexCoord2f(0,0);glVertex3f(x1,y0,z0); glTexCoord2f(uW,0);glVertex3f(x0,y0,z0);
    glTexCoord2f(uW,vH);glVertex3f(x0,y1,z0); glTexCoord2f(0,vH);glVertex3f(x1,y1,z0);
    glNormal3f(-1,0,0);
    glTexCoord2f(0,0);glVertex3f(x0,y0,z0); glTexCoord2f(uD,0);glVertex3f(x0,y0,z1);
    glTexCoord2f(uD,vH);glVertex3f(x0,y1,z1); glTexCoord2f(0,vH);glVertex3f(x0,y1,z0);
    glNormal3f(1,0,0);
    glTexCoord2f(0,0);glVertex3f(x1,y0,z1); glTexCoord2f(uD,0);glVertex3f(x1,y0,z0);
    glTexCoord2f(uD,vH);glVertex3f(x1,y1,z0); glTexCoord2f(0,vH);glVertex3f(x1,y1,z1);
    glNormal3f(0,1,0);
    glTexCoord2f(0,0);glVertex3f(x0,y1,z0); glTexCoord2f(1,0);glVertex3f(x0,y1,z1);
    glTexCoord2f(1,1);glVertex3f(x1,y1,z1); glTexCoord2f(0,1);glVertex3f(x1,y1,z0);
    glNormal3f(0,-1,0);
    glTexCoord2f(0,0);glVertex3f(x0,y0,z1); glTexCoord2f(1,0);glVertex3f(x0,y0,z0);
    glTexCoord2f(1,1);glVertex3f(x1,y0,z0); glTexCoord2f(0,1);glVertex3f(x1,y0,z1);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static void drawCylinder(float cx,float by,float cz,float r,float h,GLuint tex,int sl=28){
    glPushMatrix();
    glTranslatef(cx,by,cz); glRotatef(-90,1,0,0);
    GLUquadric* q=gluNewQuadric(); gluQuadricNormals(q,GLU_SMOOTH);
    if(tex){ glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,tex);
             glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
             gluQuadricTexture(q,GL_TRUE); }
    gluCylinder(q,r,r,h,sl,8); glDisable(GL_TEXTURE_2D);
    glTranslatef(0,0,h); gluDisk(q,0,r,sl,1);
    glTranslatef(0,0,-h); glRotatef(180,1,0,0); gluDisk(q,0,r,sl,1);
    gluDeleteQuadric(q); glPopMatrix();
}

static void drawSpire(float cx,float by,float cz,float br,float h,int sl=16){
    glPushMatrix(); glTranslatef(cx,by,cz); glRotatef(-90,1,0,0);
    GLUquadric* q=gluNewQuadric(); gluQuadricNormals(q,GLU_SMOOTH);
    gluCylinder(q,br,0,h,sl,1); glRotatef(180,1,0,0); gluDisk(q,0,br,sl,1);
    gluDeleteQuadric(q); glPopMatrix();
}

static void drawRoof(float cx,float by,float cz,float w,float d,float h){
    float hw=w*.5f,hd=d*.5f,l1=sqrtf(h*h+hd*hd),l2=sqrtf(h*h+hw*hw);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texID[TEX_ROOF]);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBegin(GL_TRIANGLES);
    glNormal3f(0,h/l1,hd/l1);
    glTexCoord2f(0,0);glVertex3f(cx-hw,by,cz+hd);
    glTexCoord2f(1,0);glVertex3f(cx+hw,by,cz+hd);
    glTexCoord2f(.5f,1);glVertex3f(cx,by+h,cz);
    glNormal3f(0,h/l1,-hd/l1);
    glTexCoord2f(0,0);glVertex3f(cx+hw,by,cz-hd);
    glTexCoord2f(1,0);glVertex3f(cx-hw,by,cz-hd);
    glTexCoord2f(.5f,1);glVertex3f(cx,by+h,cz);
    glNormal3f(hw/l2,h/l2,0);
    glTexCoord2f(0,0);glVertex3f(cx+hw,by,cz+hd);
    glTexCoord2f(1,0);glVertex3f(cx+hw,by,cz-hd);
    glTexCoord2f(.5f,1);glVertex3f(cx,by+h,cz);
    glNormal3f(-hw/l2,h/l2,0);
    glTexCoord2f(0,0);glVertex3f(cx-hw,by,cz-hd);
    glTexCoord2f(1,0);glVertex3f(cx-hw,by,cz+hd);
    glTexCoord2f(.5f,1);glVertex3f(cx,by+h,cz);
    glEnd(); glDisable(GL_TEXTURE_2D);
}

/* 
   ДЕРЕВО
    */
static void drawTree(float cx,float by,float cz,float sc=1.0f){
    setMat(0.40f,0.24f,0.10f,8);
    drawCylinder(cx,by,cz,0.3f*sc,2.0f*sc,0);
    for(int i=0;i<3;i++){
        float yBase=by+(1.5f+i*1.2f)*sc, br=(1.6f-i*0.35f)*sc, bh=(2.5f-i*0.4f)*sc;
        setMat(0.12f+i*0.04f, 0.48f+i*0.08f, 0.12f, 12);
        glPushMatrix(); glTranslatef(cx,yBase,cz); glRotatef(-90,1,0,0);
        GLUquadric* q=gluNewQuadric(); gluQuadricNormals(q,GLU_SMOOTH);
        gluCylinder(q,br,0,bh,16,1); gluDeleteQuadric(q); glPopMatrix();
    }
}

/* 
   ЗЕМЛЯ
    */
static void drawGround(){
    setMat(0.28f,0.55f,0.18f,5);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texID[TEX_GRASS]);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBegin(GL_QUADS); glNormal3f(0,1,0);
    glTexCoord2f(0,0); glVertex3f(-150,0,-150);
    glTexCoord2f(20,0);glVertex3f( 150,0,-150);
    glTexCoord2f(20,20);glVertex3f(150,0, 150);
    glTexCoord2f(0,20);glVertex3f(-150,0, 150);
    glEnd(); glDisable(GL_TEXTURE_2D);
}

/* 
   ДОРОГИ
    */
static void drawRingRoad(float inner,float outer,int seg=80){
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texID[TEX_ASPHALT]);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    setMat(0.22f,0.22f,0.22f,4);
    float da=2*(float)M_PI/seg;
    glBegin(GL_QUAD_STRIP); glNormal3f(0,1,0);
    for(int i=0;i<=seg;i++){
        float a=i*da,cs=cosf(a),sn=sinf(a),u=i/(float)seg*8;
        glTexCoord2f(u,1);glVertex3f(outer*cs,0.03f,outer*sn);
        glTexCoord2f(u,0);glVertex3f(inner*cs,0.03f,inner*sn);
    }
    glEnd(); glDisable(GL_TEXTURE_2D);
    /* разметка */
    setMat(0.9f,0.9f,0.9f,6); float mid=(inner+outer)*.5f,lw=0.22f;
    da=2*(float)M_PI/40;
    for(int i=0;i<40;i+=2){
        float a0=i*da,a1=(i+1)*da;
        glBegin(GL_QUAD_STRIP); glNormal3f(0,1,0);
        for(int j=0;j<=8;j++){
            float a=a0+(a1-a0)*j/8.0f,cs=cosf(a),sn=sinf(a);
            glVertex3f((mid+lw)*cs,0.05f,(mid+lw)*sn);
            glVertex3f((mid-lw)*cs,0.05f,(mid-lw)*sn);
        }
        glEnd();
    }
}

static void drawRoad2pts(float x1,float z1,float x2,float z2,float hw){
    float dx=x2-x1,dz=z2-z1,len=sqrtf(dx*dx+dz*dz);
    float px=-dz/len*hw,pz=dx/len*hw;
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texID[TEX_ASPHALT]);
    glTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    setMat(0.22f,0.22f,0.22f,4);
    glBegin(GL_QUADS); glNormal3f(0,1,0);
    glTexCoord2f(0,0);glVertex3f(x1+px,0.02f,z1+pz);
    glTexCoord2f(1,0);glVertex3f(x1-px,0.02f,z1-pz);
    glTexCoord2f(1,4);glVertex3f(x2-px,0.02f,z2-pz);
    glTexCoord2f(0,4);glVertex3f(x2+px,0.02f,z2+pz);
    glEnd(); glDisable(GL_TEXTURE_2D);
}

static void drawRadialRoads(){
    /* от кольца к двум кластерам */
    drawRoad2pts(-10,14,-36,32, 2.8f);
    drawRoad2pts( 10,14, 36,32, 2.8f);
}

/* 
   ДЕЛОВОЙ ЦЕНТР
    */
static void drawCityCenter(){
    setMat(0.55f,0.70f,0.90f,95);
    drawBoxTex(-5.5f,0,-5.5f, 5.0f,26.0f,5.0f, texID[TEX_WIN_BLUE]);

    setMat(0.82f,0.78f,0.68f,30);
    drawBoxTex(6.0f,0,-5.0f, 5.5f,17.0f,5.5f, texID[TEX_WIN_BEIGE]);
    setMat(0.72f,0.12f,0.12f,70);
    drawSpire(6.0f,17.0f,-5.0f,0.7f,8.0f);

    setMat(0.72f,0.68f,0.58f,20);
    drawBoxTex(-5.5f,0,6.0f, 8.0f,10.0f,6.0f, texID[TEX_WIN_GRAY]);

    setMat(0.55f,0.75f,0.55f,65);
    drawCylinder(6.5f,0,6.5f,3.5f,21.0f,texID[TEX_WIN_GREEN]);
}

/* 
   ПРИГОРОД — 2 кластера по 4 дома
    */
struct House{float x,z,w,d,wH,rH,rot,wr,wg,wb;};
static const House cA[]={
    {-38,28,6.5f,5.5f,4.5f,3.0f,  0,0.88f,0.72f,0.52f},
    {-28,30,6.0f,5.0f,3.8f,2.6f, 10,0.90f,0.82f,0.62f},
    {-38,40,6.0f,5.5f,4.0f,2.8f,  0,0.80f,0.70f,0.55f},
    {-28,42,5.5f,4.5f,3.5f,2.5f,-10,0.86f,0.74f,0.54f},
};
static const House cB[]={
    { 38,28,6.5f,5.5f,4.2f,3.0f,  0,0.84f,0.68f,0.48f},
    { 28,30,6.0f,5.0f,3.8f,2.8f, -8,0.88f,0.76f,0.58f},
    { 38,40,6.0f,5.5f,4.5f,3.0f,  0,0.92f,0.80f,0.65f},
    { 28,42,5.5f,4.5f,3.5f,2.5f, 12,0.85f,0.73f,0.53f},
};
static void drawCluster(const House* arr,int n){
    for(int i=0;i<n;i++){
        const House& h=arr[i];
        glPushMatrix(); glTranslatef(h.x,0,h.z); glRotatef(h.rot,0,1,0);
        setMat(h.wr,h.wg,h.wb,12);
        drawBoxTex(0,0,0,h.w,h.wH,h.d,texID[TEX_WIN_BEIGE]);
        setMat(0.72f,0.18f,0.08f,18);
        drawRoof(0,h.wH,0,h.w,h.d,h.rH);
        glPopMatrix();
    }
}
static void drawSuburbs(){drawCluster(cA,4);drawCluster(cB,4);}

/* 
   ДЕРЕВЬЯ
    */
static void drawAllTrees(){
    drawTree( 22,0, 10,1.0f); drawTree(-22,0,-12,0.9f);
    drawTree(  0,0,-22,1.1f); drawTree(-15,0, 18,0.85f);
    drawTree( 18,0,-18,1.0f); drawTree(-43,0, 35,0.8f);
    drawTree( 43,0, 35,0.8f); drawTree(-23,0, 48,0.75f);
    drawTree( 23,0, 48,0.75f);
}

/* 
   МАШИНЫ
    */
static void drawCar(float angle,float roadR,float r,float g,float b){
    float a=angle*(float)M_PI/180;
    float x=roadR*cosf(a),z=roadR*sinf(a);
    glPushMatrix(); glTranslatef(x,0.2f,z); glRotatef(angle+90,0,1,0);
    setMat(r,g,b,40); drawBox(0,0,0,3.0f,1.0f,1.5f);
    setMat(r*.7f,g*.7f,b*.7f,20); drawBox(-0.2f,1.0f,0,1.8f,0.75f,1.3f);
    setMat(0.08f,0.08f,0.08f,10);
    for(int wx=-1;wx<=1;wx+=2) for(int wz=-1;wz<=1;wz+=2){
        glPushMatrix(); glTranslatef(wx*0.9f,0,wz*0.65f); glRotatef(90,1,0,0);
        GLUquadric* q=gluNewQuadric();
        gluCylinder(q,0.3f,0.3f,0.2f,12,1); gluDeleteQuadric(q); glPopMatrix();
    }
    glPopMatrix();
}

/* 
   НЕБО И СВЕТ
    */
static void updateSky(){
    float t=sinf(sunAngle*(float)M_PI/180.0f); if(t<0)t=0;
    timeOfDay=t;
    glClearColor(0.05f+0.45f*t, 0.08f+0.65f*t, 0.15f+0.82f*t, 1);
    float lr=0.3f+0.7f*t, lg=0.2f+0.77f*t, lb=0.05f+0.83f*t;
    float d[4]={lr,lg,lb,1}, am[4]={lr*.3f,lg*.3f,lb*.3f,1};
    glLightfv(GL_LIGHT0,GL_DIFFUSE,d); glLightfv(GL_LIGHT0,GL_AMBIENT,am);
}

static void drawSun(){
    float rad=sunAngle*(float)M_PI/180;
    float sy=70*sinf(rad); if(sy<-5)return;
    float t=sy/70.0f;
    glPushMatrix(); glTranslatef(70*cosf(rad),sy,-40);
    glDisable(GL_LIGHTING);
    glColor3f(1.0f,0.85f+0.10f*t,0.30f+0.65f*t);
    glutSolidSphere(4.5,24,24);
    glEnable(GL_LIGHTING); glPopMatrix();
}

/* 
   КАМЕРЫ
    */
static void applyCamera(){
    switch(camMode){
    case 0:{ float ry=camAngleY*(float)M_PI/180,rx=camAngleX*(float)M_PI/180;
             gluLookAt(camDist*sinf(ry)*cosf(rx),camDist*sinf(rx),camDist*cosf(ry)*cosf(rx),0,5,0,0,1,0);break;}
    case 1: gluLookAt(0,100,0,    0,0,0,  0,0,-1);break;
    case 2: gluLookAt(130,45,0,   0,8,0,  0,1,0); break;
    case 3: gluLookAt(0,18,110,   0,5,0,  0,1,0); break;
    case 4: gluLookAt(-60,30,-60, 0,8,0,  0,1,0); break;
    case 5: gluLookAt(8,5,8,      0,15,0, 0,1,0); break;
    }
}

/* 
   DISPLAY
    */
static void display(){
    updateSky();
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glLoadIdentity(); applyCamera();
    float rad=sunAngle*(float)M_PI/180;
    float lp[4]={70*cosf(rad),70*sinf(rad),-40,1};
    glLightfv(GL_LIGHT0,GL_POSITION,lp);
    drawGround(); drawRadialRoads(); drawRingRoad(14.0f,20.0f);
    drawCityCenter(); drawSuburbs(); drawAllTrees();
    drawCar(carAngle, 17.0f,0.85f,0.12f,0.12f);
    drawCar(car2Angle,17.0f,0.12f,0.28f,0.75f);
    drawSun(); glutSwapBuffers();
}

static void timer(int){
    if(!paused){
        sunAngle+=0.12f; if(sunAngle>360)sunAngle-=360;
        carAngle +=0.8f; if(carAngle >360)carAngle -=360;
        car2Angle+=0.8f; if(car2Angle>360)car2Angle-=360;
        static int fc=0; if(++fc%25==0) rebuildWinTextures();
    }
    glutPostRedisplay(); glutTimerFunc(16,timer,0);
}

static void reshape(int w,int h){
    if(!h)h=1; glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    gluPerspective(45.0,(double)w/h,0.5,700.0);
    glMatrixMode(GL_MODELVIEW);
}

static void keyboard(unsigned char key,int,int){
    switch(key){
        case 'a':case 'A':camAngleY-=5;camMode=0;break;
        case 'd':case 'D':camAngleY+=5;camMode=0;break;
        case 'w':case 'W':camAngleX+=3;camMode=0;break;
        case 's':case 'S':camAngleX-=3;camMode=0;break;
        case '+':case '=':camDist-=6;camMode=0;break;
        case '-':case '_':camDist+=6;camMode=0;break;
        case '1':camMode=0;break; case '2':camMode=1;break;
        case '3':camMode=2;break; case '4':camMode=3;break;
        case '5':camMode=4;break; case '6':camMode=5;break;
        case ' ':paused=!paused;break;
        case 27:exit(0);
    }
    if(camAngleX<5)camAngleX=5; if(camAngleX>85)camAngleX=85;
    if(camDist<15)camDist=15;   if(camDist>260)camDist=260;
    glutPostRedisplay();
}

static void initGL(){
    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0); glEnable(GL_NORMALIZE);
    float sp[4]={1,1,1,1}; glLightfv(GL_LIGHT0,GL_SPECULAR,sp);
    glShadeModel(GL_SMOOTH);
    buildTextures();
}

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB|GLUT_DEPTH|GLUT_MULTISAMPLE);
    glutInitWindowSize(1024,680); glutInitWindowPosition(60,40);
    glutCreateWindow("Virtual City  |  W/S/A/D камера  +/- зум  1-6 виды  SPACE пауза  ESC выход");
    initGL();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard); glutTimerFunc(16,timer,0);
    glutMainLoop(); return 0;
}
