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

// A rotating cube rendered with OpenGL|ES. Three images used as textures on the cube faces.

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
#include "../common/cube_texture_and_coords.h"
void startVideoThread( EGLDisplay dpy, EGLContext ctx, EGLClientBuffer buffer );
void killVideoThread();
}

#define IMAGE_SIZE_WIDTH 1920
#define IMAGE_SIZE_HEIGHT 1080

#ifndef M_PI
   #define M_PI 3.141592654
#endif


typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   GLuint tex;
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
} CUBE_STATE_T;
static CUBE_STATE_T _state, *state=&_state;

#include "../common/init_ogl.h"

/////////////////////////////////////////////////////////////////////////////

static void reset_model(CUBE_STATE_T *state)
{
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.f, 0.f, -50.f);

   state->rot_angle_x = 45.f;
   state->rot_angle_y = 30.f;
   state->rot_angle_z = 0.f;
   state->rot_angle_x_inc = 0.5f;
   state->rot_angle_y_inc = 0.3f;
   state->rot_angle_z_inc = 0.1f;
   state->distance = 40.f;
}

/////////////////////////////////////////////////////////////////////////////

static void init_projection(CUBE_STATE_T *state)
{
   float nearp = 1.0f;
   float farp = 500.0f;

   float hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   float hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
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

/////////////////////////////////////////////////////////////////////////////

static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc)
{
   distance += distance_inc;

   if (distance >= 120.0f)
      distance = 120.f;
   else if (distance <= 40.0f)
      distance = 40.0f;

   return distance;
}

/////////////////////////////////////////////////////////////////////////////

static void update_model(CUBE_STATE_T *state)
{
   // update position
   state->rot_angle_x = inc_and_wrap_angle(state->rot_angle_x, state->rot_angle_x_inc);
   state->rot_angle_y = inc_and_wrap_angle(state->rot_angle_y, state->rot_angle_y_inc);
   state->rot_angle_z = inc_and_wrap_angle(state->rot_angle_z, state->rot_angle_z_inc);
   state->distance    = inc_and_clip_distance(state->distance, state->distance_inc);

   glLoadIdentity();
   // move camera back to see the cube
   glTranslatef(0.f, 0.f, -state->distance);

   // Rotate model to new position
   glRotatef(state->rot_angle_x, 1.f, 0.f, 0.f);
   glRotatef(state->rot_angle_y, 0.f, 1.f, 0.f);
   glRotatef(state->rot_angle_z, 0.f, 0.f, 1.f);
}

/////////////////////////////////////////////////////////////////////////////

static void redraw_scene(CUBE_STATE_T *state)
{
   // Start with a clear screen
   glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

   // Need to rotate textures - do this by rotating each cube face
   glRotatef(270.f, 0.f, 0.f, 1.f ); // front face normal along z axis

   // draw first 4 vertices
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);

   // same pattern for other 5 faces - rotation chosen to make image orientation 'nice'
   glRotatef(90.f, 0.f, 0.f, 1.f ); // back face normal along z axis
   glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);

   glRotatef(90.f, 1.f, 0.f, 0.f ); // left face normal along x axis
   glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);

   glRotatef(90.f, 1.f, 0.f, 0.f ); // right face normal along x axis
   glDrawArrays( GL_TRIANGLE_STRIP, 12, 4);

   glRotatef(270.f, 0.f, 1.f, 0.f ); // top face normal along y axis
   glDrawArrays( GL_TRIANGLE_STRIP, 16, 4);

   glRotatef(90.f, 0.f, 1.f, 0.f ); // bottom face normal along y axis
   glDrawArrays( GL_TRIANGLE_STRIP, 20, 4);

   eglSwapBuffers(state->display, state->surface);
}

/////////////////////////////////////////////////////////////////////////////

static void init_textures(CUBE_STATE_T *state)
{
   glGenTextures(1, &state->tex);
   glBindTexture(GL_TEXTURE_2D, state->tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, IMAGE_SIZE_WIDTH, IMAGE_SIZE_HEIGHT, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, NULL);

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
   printf("Note: ensure you have sufficient gpu_mem configured\n");

   memset( state, 0, sizeof( *state ) );

   init_ogl( state, false );
   init_projection(state);
   init_textures(state);


   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
   glViewport( 0, 0, state->screen_width, state->screen_height);

   glEnable( GL_DEPTH_TEST );
   glClearColor( 1, 0, 0, 0 );

   glVertexPointer( 3, GL_BYTE, 0, quadx );
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glEnableClientState( GL_VERTEX_ARRAY );

   glBindTexture(GL_TEXTURE_2D, state->tex);
   glEnable(GL_TEXTURE_2D);

   reset_model(state);

   while(1)
   {
      update_model(state);
      redraw_scene(state);
   }
   exit_func();
   return 0;
}
