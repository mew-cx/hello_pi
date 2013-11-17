// triangle2.c
/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// OpenGL|ES 2 demo using shader to compute mandelbrot/julia sets
// Thanks to Peter de Rivas for original Python code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>      // for O_RDONLY O_NONBLOCK

#include "bcm_host.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;

   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

   GLuint vertShader;
   GLuint juliaShader;
   GLuint mandelShader;

   GLuint juliaProgram;
   GLuint mandelProgram;

   GLuint tex_fb;
   GLuint tex;

   GLuint buf;

// julia attribs
   GLuint unif_color, attr_vertex, unif_scale, unif_offset, unif_tex, unif_centre;
// mandelbrot attribs
   GLuint attr_vertex2, unif_scale2, unif_offset2, unif_centre2;
} CUBE_STATE_T;
static CUBE_STATE_T _state, *state=&_state;

#include "../common/init_ogl.h"


/////////////////////////////////////////////////////////////////////////////

static void showlog(GLint shader)
{
   char log[1024] = {0};
   glGetShaderInfoLog(shader,sizeof(log),NULL,log);
   printf("shader %d: \"%s\"\n", shader, log);
}

static void showprogramlog(GLint shader)
{
   char log[1024] = {0};
   glGetProgramInfoLog(shader,sizeof(log),NULL,log);
   printf("program %d: \"%s\"\n", shader, log);
}

/////////////////////////////////////////////////////////////////////////////

static void init_shaders(CUBE_STATE_T *state)
{
   const GLchar *vshader_source =
"attribute vec4 vertex;"
"varying vec2 tcoord;"
"void main(void) {"
" gl_Position = vertex;"
" tcoord = vertex.xy * 0.5 + 0.5;"
"}";

   //Mandelbrot
   const GLchar *mandelbrot_fshader_source =
"uniform vec4 color;"
"uniform vec2 scale;"
"uniform vec2 centre;"
"varying vec2 tcoord;"
"void main(void) {"
"  float intensity;"
"  vec4 color2;"
"  float cr=(gl_FragCoord.x-centre.x)*scale.x;"
"  float ci=(gl_FragCoord.y-centre.y)*scale.y;"
"  float ar=cr;"
"  float ai=ci;"
"  float tr,ti;"
"  float col=0.0;"
"  float p=0.0;"
"  int i=0;"
"  for(int i2=1;i2<16;i2++)"
"  {"
"    tr=ar*ar-ai*ai+cr;"
"    ti=2.0*ar*ai+ci;"
"    p=tr*tr+ti*ti;"
"    ar=tr;"
"    ai=ti;"
"    if (p>16.0)"
"    {"
"      i=i2;"
"      break;"
"    }"
"  }"
"  color2 = vec4(float(i)*0.0625,0,0,1);"
"  gl_FragColor = color2;"
"}";

   // Julia
   const GLchar *julia_fshader_source =
"uniform vec4 color;"
"uniform vec2 scale;"
"uniform vec2 centre;"
"uniform vec2 offset;"
"varying vec2 tcoord;"
"uniform sampler2D tex;"
"void main(void) {"
"  float intensity;"
"  vec4 color2;"
"  float ar=(gl_FragCoord.x-centre.x)*scale.x;"
"  float ai=(gl_FragCoord.y-centre.y)*scale.y;"
"  float cr=(offset.x-centre.x)*scale.x;"
"  float ci=(offset.y-centre.y)*scale.y;"
"  float tr,ti;"
"  float col=0.0;"
"  float p=0.0;"
"  int i=0;"
"  vec2 t2;"
"  t2.x=tcoord.x+(offset.x-centre.x)*(0.5/centre.y);"
"  t2.y=tcoord.y+(offset.y-centre.y)*(0.5/centre.x);"
"  for(int i2=1;i2<16;i2++)"
"  {"
"    tr=ar*ar-ai*ai+cr;"
"    ti=2.0*ar*ai+ci;"
"    p=tr*tr+ti*ti;"
"    ar=tr;"
"    ai=ti;"
"    if (p>16.0)"
"    {"
"      i=i2;"
"      break;"
"    }"
"  }"
"  color2 = vec4(0,float(i)*0.0625,0,1);"
"  color2 = color2 + texture2D(tex,t2);"
"  gl_FragColor = color2;"
"}";

        state->vertShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(state->vertShader, 1, &vshader_source, 0);
        glCompileShader(state->vertShader);
        showlog(state->vertShader);

        state->juliaShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(state->juliaShader, 1, &julia_fshader_source, 0);
        glCompileShader(state->juliaShader);
        showlog(state->juliaShader);

        state->mandelShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(state->mandelShader, 1, &mandelbrot_fshader_source, 0);
        glCompileShader(state->mandelShader);
        showlog(state->mandelShader);

        // julia
        state->juliaProgram = glCreateProgram();
        glAttachShader(state->juliaProgram, state->vertShader);
        glAttachShader(state->juliaProgram, state->juliaShader);
        glLinkProgram(state->juliaProgram);
        showprogramlog(state->juliaProgram);

        state->attr_vertex = glGetAttribLocation(state->juliaProgram, "vertex");
        state->unif_color  = glGetUniformLocation(state->juliaProgram, "color");
        state->unif_scale  = glGetUniformLocation(state->juliaProgram, "scale");
        state->unif_offset = glGetUniformLocation(state->juliaProgram, "offset");
        state->unif_tex    = glGetUniformLocation(state->juliaProgram, "tex");
        state->unif_centre = glGetUniformLocation(state->juliaProgram, "centre");

        // mandelbrot
        state->mandelProgram = glCreateProgram();
        glAttachShader(state->mandelProgram, state->vertShader);
        glAttachShader(state->mandelProgram, state->mandelShader);
        glLinkProgram(state->mandelProgram);
        showprogramlog(state->mandelProgram);

        state->attr_vertex2 = glGetAttribLocation(state->mandelProgram, "vertex");
        state->unif_scale2  = glGetUniformLocation(state->mandelProgram, "scale");
        state->unif_offset2 = glGetUniformLocation(state->mandelProgram, "offset");
        state->unif_centre2 = glGetUniformLocation(state->mandelProgram, "centre");



        // create mandelbrot texture
        glGenTextures(1, &state->tex);
        glBindTexture(GL_TEXTURE_2D,state->tex);
        glTexImage2D( GL_TEXTURE_2D,0,GL_RGB,
            state->screen_width, state->screen_height,
            0, GL_RGB,GL_UNSIGNED_SHORT_5_6_5, 0 );
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // FBO to generate mandelbrot texture
        glGenFramebuffers(1,&state->tex_fb);
        glBindFramebuffer(GL_FRAMEBUFFER,state->tex_fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,state->tex,0);
        glBindFramebuffer(GL_FRAMEBUFFER,0);

        // Upload vertex data to a buffer
   static const GLfloat vertex_data[] = {
        -1.0, -1.0,  1.0,
         1.0, -1.0,  1.0,
         1.0,  1.0,  1.0,
        -1.0,  1.0,  1.0,
   };

        glGenBuffers(1, &state->buf);
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data),
                             vertex_data, GL_STATIC_DRAW);
        glVertexAttribPointer( state->attr_vertex, 3, GL_FLOAT, GL_FALSE, 0, 0 );
        glEnableVertexAttribArray(state->attr_vertex);
}

/////////////////////////////////////////////////////////////////////////////

static void draw_mandelbrot_to_texture(CUBE_STATE_T *state, GLfloat cx, GLfloat cy, GLfloat scale)
{
printf( "draw_mandelbrot_to_texture %f %f %f\n", cx, cy, scale );
        glBindFramebuffer(GL_FRAMEBUFFER,state->tex_fb);

        glUseProgram ( state->mandelProgram );
        glUniform2f(state->unif_scale2, scale, scale);
        glUniform2f(state->unif_centre2, cx, cy);

        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
}

/////////////////////////////////////////////////////////////////////////////

static void draw_triangles(CUBE_STATE_T *state, GLfloat cx, GLfloat cy, GLfloat scale, GLfloat x, GLfloat y)
{
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        //glClear( GL_COLOR_BUFFER_BIT );

        glActiveTexture(0);
        glBindTexture(GL_TEXTURE_2D,state->tex);

        glUseProgram ( state->juliaProgram );
        glUniform4f(state->unif_color, 0.5, 0.5, 0.8, 1.0);
        glUniform2f(state->unif_scale, scale, scale);
        glUniform2f(state->unif_offset, x, y);
        glUniform2f(state->unif_centre, cx, cy);
        glUniform1i(state->unif_tex, 0);

        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );

        eglSwapBuffers(state->display, state->surface);
}

/////////////////////////////////////////////////////////////////////////////

static int get_mouse( int width, int height, int *outx, int *outy)
{
    static int x( 0 );
    static int y( 0 );

    static int fd( -1 );
if( fd < 0 ) fd = open( "/dev/input/mouse0", O_RDONLY|O_NONBLOCK );

    assert( fd >= 0 );

    struct { unsigned char buttons, dx, dy; } m;
    while (1)
    {
       size_t bytes = read( fd, &m, sizeof(m) );
       if( bytes < sizeof(m) ) goto _exit;

       if( m.buttons & 0x08 ) break; // a valid packet

       bytes = read( fd, &m, 1 ); // Try to sync up again
       if( bytes < 1 ) goto _exit;
    }

    if( m.buttons & 0x07 )
        return m.buttons & 0x07;

    x += m.dx; if( m.buttons & 0x10 )  x -= 256;
    y += m.dy; if( m.buttons & 0x20 )  y -= 256;

    if (x<0) x=0;
    if (y<0) y=0;
    if (x>width) x = width;
    if (y>height) y = height;

_exit:
   if (outx) *outx = x;
   if (outy) *outy = y;
   return 0;
}

/////////////////////////////////////////////////////////////////////////////

int main ()
{
   GLfloat cx, cy;

   bcm_host_init();

   memset( state, 0, sizeof( *state ) );

   init_ogl( state, true );
   init_shaders(state);

    glViewport( 0, 0, state->screen_width, state->screen_height );

   //glClearColor(0.15f, 0.25f, 0.35f, 1.0f);

   cx = state->screen_width/2;
   cy = state->screen_height/2;
   draw_mandelbrot_to_texture(state, cx, cy, 0.003);

printf( "start render loop...\n" );
   while (1)
   {
      int x(0), y(0), b(0);
      b = get_mouse( state->screen_width, state->screen_height, &x, &y );
printf( "mouse %d %d %d\n", b, x, y );
      if( b )  break;
      draw_triangles(state, cx, cy, 0.003, x, y);
   }
   return 0;
}
