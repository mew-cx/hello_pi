static void init_ogl( CUBE_STATE_T *state, bool useES2 )
{
   static EGL_DISPMANX_WINDOW_T nativewindow;

{
   int32_t success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );
printf( "graphics_get_display_size %d %d\n", state->screen_width, state->screen_height );

   VC_RECT_T dst_rect;
   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;

   VC_RECT_T src_rect;
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;

   DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open( 0 /* LCD */);

   DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start( 0 );

   DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/,
      &dst_rect,
      0/*src*/,
      &src_rect,
      DISPMANX_PROTECTION_NONE,
      0 /*alpha*/,
      0/*clamp*/,
      DISPMANX_NO_ROTATE );

   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
}


   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 16,
      //EGL_SAMPLES, 4,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };

   static const EGLint contextAttrsES2[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);

   EGLBoolean result;
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);

   EGLint numConfig(0);
   EGLConfig config;
   //result = eglChooseConfig( state->display, attribute_list, &config, 1, &numConfig );
   result = eglSaneChooseConfigBRCM( state->display, attribute_list, &config, 1, &numConfig );
printf( "EGL numConfig %d\n", numConfig );
   assert(EGL_FALSE != result);

   result = eglBindAPI(EGL_OPENGL_ES_API);
   assert(EGL_FALSE != result);

   const EGLint* cxtAttr( useES2 ?  contextAttrsES2 : NULL );
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, cxtAttr );
   assert(state->context!=EGL_NO_CONTEXT);

   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);

   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);
}
