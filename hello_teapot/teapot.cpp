// teapot.cpp
/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2012, OtherCrashOverride
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "bcm_host.h"

#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

extern "C" {
#include "models.h"
#include "../common/cube_texture_and_coords.h"
void startVideoThread( EGLDisplay dpy, EGLContext ctx, EGLClientBuffer buffer );
void killVideoThread();
}

#define IMAGE_WIDTH 1920
#define IMAGE_HEIGHT 1080

#ifndef M_PI
   #define M_PI 3.141592654
#endif


typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;

   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

// model rotation vector and direction
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
// current model rotation angles
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
// current distance from camera
   GLfloat distance;
   GLfloat distance_inc;

   GLuint tex;
   MODEL_T model;
} CUBE_STATE_T;

static CUBE_STATE_T _state, *state=&_state;

#include "../common/init_ogl.h"

/////////////////////////////////////////////////////////////////////////////

static void init_projection(CUBE_STATE_T *state)
{
   float nearp = 0.1f;
   float farp = 500.0f;
   float hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   float hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
}

/////////////////////////////////////////////////////////////////////////////

static void reset_model(CUBE_STATE_T *state)
{
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   state->rot_angle_x = 45.f;
   state->rot_angle_y = 30.f;
   state->rot_angle_z = 0.f;
   state->rot_angle_x_inc = 0.5f;
   state->rot_angle_y_inc = 0.5f;
   state->rot_angle_z_inc = 0.f;
   state->distance = 1.2f*1.5f;
}

/////////////////////////////////////////////////////////////////////////////

static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc)
{
   angle += angle_inc;

   if (angle >= 360.0)
      angle -= 360.f;
   else if (angle <=0)
      angle += 360.f;

   return angle;
}

static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc)
{
   distance += distance_inc;

   if (distance >= 10.0f)
      distance = 10.f;
   else if (distance <= 1.0f)
      distance = 1.0f;

   return distance;
}

/////////////////////////////////////////////////////////////////////////////

static void update_model(CUBE_STATE_T *state)
{
   state->rot_angle_x = inc_and_wrap_angle(state->rot_angle_x, state->rot_angle_x_inc);
   state->rot_angle_y = inc_and_wrap_angle(state->rot_angle_y, state->rot_angle_y_inc);
   state->rot_angle_z = inc_and_wrap_angle(state->rot_angle_z, state->rot_angle_z_inc);
   state->distance    = inc_and_clip_distance(state->distance, state->distance_inc);

   glLoadIdentity();
   glTranslatef(0.f, 0.f, -state->distance);

   glRotatef(state->rot_angle_x, 1.f, 0.f, 0.f);
   glRotatef(state->rot_angle_y, 0.f, 1.f, 0.f);
   glRotatef(state->rot_angle_z, 0.f, 0.f, 1.f);
}

/////////////////////////////////////////////////////////////////////////////

static void redraw_scene(CUBE_STATE_T *state)
{
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
   draw_wavefront(state->model, state->tex);
   eglSwapBuffers(state->display, state->surface);
}

/////////////////////////////////////////////////////////////////////////////

static void init_textures(CUBE_STATE_T *state)
{
   glGenTextures(1, &state->tex);
   glBindTexture(GL_TEXTURE_2D, state->tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_WIDTH, IMAGE_HEIGHT,
       0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    startVideoThread( state->display, state->context, (EGLClientBuffer)state->tex );
}

/////////////////////////////////////////////////////////////////////////////

static void exit_func(void)
{
    killVideoThread();

   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
   eglDestroySurface( state->display, state->surface );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );
}

/////////////////////////////////////////////////////////////////////////////

int main ()
{
   bcm_host_init();

   memset( state, 0, sizeof( *state ) );

   init_ogl( state, false );
   init_projection(state);
   reset_model(state);

   init_textures(state);

   state->model = load_wavefront("./teapot.obj.dat", NULL);

   glEnable(GL_DEPTH_TEST);
   glClearDepthf(1.0);
   glDepthFunc(GL_LEQUAL);

   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

   glEnableClientState( GL_VERTEX_ARRAY );
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, state->tex);

   float noAmbient[] = {1.0f, 1.0f, 1.0f, 1.0f};
   glLightfv(GL_LIGHT0, GL_AMBIENT, noAmbient);
   glEnable(GL_LIGHT0);
   glEnable(GL_LIGHTING);

   glClearColor(0.15f, 0.25f, 0.35f, 0.0f);
   glEnable(GL_CULL_FACE);
   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    glViewport( 0, 0, state->screen_width, state->screen_height);

   while (1)
   {
      update_model(state);
      redraw_scene(state);
   }
   exit_func();
   return 0;
}
