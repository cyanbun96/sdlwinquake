// vid_sdl.h -- sdl video driver 

#include <SDL2/SDL.h>
#include "quakedef.h"
#include "d_local.h"

// CyanBun96: Made window global for checks in options
SDL_Window *window;
SDL_Surface *windowSurface;
SDL_Renderer *renderer;
SDL_Surface *argbbuffer;
SDL_Texture *texture;
SDL_Rect blitRect;
SDL_Rect destRect;

// old-style rendering process, enabled with -forceoldrender
SDL_Surface *scaleBuffer;
int force_old_render;
Uint32 SDLWindowFlags;

cvar_t _windowed_mouse = {"_windowed_mouse","0", true};

viddef_t    vid;                // global video state
unsigned short  d_8to16table[256];

#define BASEWIDTH           320
#define BASEHEIGHT          240

int    VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte    *VGA_pagebase;

static int	lockcount;
static qboolean	vid_initialized = false;
static SDL_Surface *screen;
static qboolean	palette_changed;
static unsigned char	vid_curpal[256*3];	/* save for mode changes */
static qboolean mouse_avail;
static float   mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

void VID_CalcScreenDimensions();

void VID_MenuDraw (void);
void VID_MenuKey (int key);
// No support for option menus TODO
void (*vid_menudrawfn)(void) = VID_MenuDraw;
void (*vid_menukeyfn)(int key) = VID_MenuKey;

void    VID_SetPalette (unsigned char *palette)
{
	int		i;
	SDL_Color colors[256];

	palette_changed = true;

	if (palette != vid_curpal)
		memcpy(vid_curpal, palette, sizeof(vid_curpal));

	for (i = 0; i < 256; ++i)
	{
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}

	SDL_SetPaletteColors(screen->format->palette, colors, 0, 256);
}

void    VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_LockBuffer (void)
{
	lockcount++;

	if (lockcount > 1)
		return;

	SDL_LockSurface(screen);

	// Update surface pointer for linear access modes
	vid.buffer = vid.conbuffer = vid.direct = (pixel_t *) screen->pixels;
	vid.rowbytes = vid.conrowbytes = screen->pitch;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = vid.buffer;

	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.rowbytes;
}

void VID_UnlockBuffer (void)
{
	lockcount--;

	if (lockcount > 0)
		return;

	if (lockcount < 0)
		return;

	SDL_UnlockSurface (screen);

// to turn up any unlocked accesses
	//vid.buffer = vid.conbuffer = vid.direct = d_viewbuffer = NULL;
}

void    VID_Init (unsigned char *palette)
{
    Cvar_RegisterVariable (&_windowed_mouse);

    int pnum, chunk;
    byte *cache;
    int cachesize;
    Uint8 video_bpp;
    Uint16 video_w, video_h;
    Uint32 flags;
    char caption[50];

    // Load the SDL library
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
        Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());
    // Set up display mode (width and height)
    vid.width = BASEWIDTH;
    vid.height = BASEHEIGHT;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;

    // check for command-line window size
    if ((pnum=COM_CheckParm("-winsize")))
    {
        if (pnum >= com_argc-2)
            Sys_Error("VID: -winsize <width> <height>\n");
        vid.width = Q_atoi(com_argv[pnum+1]);
        vid.height = Q_atoi(com_argv[pnum+2]);
        if (!vid.width || !vid.height)
            Sys_Error("VID: Bad window width/height\n");
    }
    if ((pnum=COM_CheckParm("-width"))) {
        if (pnum >= com_argc-1)
            Sys_Error("VID: -width <width>\n");
        vid.width = Q_atoi(com_argv[pnum+1]);
        if (!vid.width)
            Sys_Error("VID: Bad window width\n");
    }
    if ((pnum=COM_CheckParm("-height"))) {
        if (pnum >= com_argc-1)
            Sys_Error("VID: -height <height>\n");
        vid.height = Q_atoi(com_argv[pnum+1]);
        if (!vid.height)
            Sys_Error("VID: Bad window height\n");
    }

    // Set video width, height and flags
    // CyanBun96: copying mindlessly from ChocolateDoom. expect bugs.
    
    // In windowed mode, the window can be resized while the game is
    // running.
    flags = SDL_WINDOW_RESIZABLE;

    // Set the highdpi flag - this makes a big difference on Macs with
    // retina displays, especially when using small window sizes.
    flags |= SDL_WINDOW_ALLOW_HIGHDPI;

    // CyanBun96: what most modern games call "Fullscreen borderless
    // can be achieved by passing -fullscreen_desktop and -borderless
    // -fullscreen will try to change the display resolution

    if ( COM_CheckParm ("-fullscreen") )
        flags |= SDL_WINDOW_FULLSCREEN;

    else if ( COM_CheckParm ("-fullscreen_desktop") )
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    if ( COM_CheckParm ("-window") )
        flags &= ~SDL_WINDOW_FULLSCREEN;
    
    if ( COM_CheckParm ("-borderless") )
        flags |= SDL_WINDOW_BORDERLESS;

    if ( COM_CheckParm ("-forceoldrender") )
        force_old_render = 1;
    else
        force_old_render = 0;

    if (vid.width > 1280 || vid.height > 1024)
    {
        Con_Printf("WARNING: vanilla maximum resolution is 1280x1024\n");
    	//Sys_Error("Maximum Resolution is 1280 width and 1024 height");
    }
    window = SDL_CreateWindow(NULL,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          vid.width, vid.height,
                                          flags);
    screen = SDL_CreateRGBSurfaceWithFormat(0, vid.width, vid.height, 8, SDL_PIXELFORMAT_INDEX8);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!force_old_render) {
        argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
            NULL, vid.width, vid.height, 0, 0, SDL_PIXELFORMAT_ARGB8888);
        texture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    vid.width, vid.height);
    }
    else {
        scaleBuffer = SDL_CreateRGBSurfaceWithFormat(
            0, vid.width, vid.height, 8, SDL_GetWindowPixelFormat(window));
    }
    windowSurface = SDL_GetWindowSurface(window);

    if (!screen)
        Sys_Error("VID: Couldn't set video mode: %s\n", SDL_GetError());
    VID_CalcScreenDimensions();
    SDL_UpdateWindowSurface(window);

    sprintf(caption, "SDL2WinQuake - Version %4.2f", VERSION);
    SDL_SetWindowTitle(window, (const char*)&caption);
    SDLWindowFlags = SDL_GetWindowFlags(window);
    
    // now know everything we need to know about the buffer
    VID_SetPalette(palette);
    VGA_width = vid.conwidth = vid.width;
    VGA_height = vid.conheight = vid.height;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    VGA_pagebase = vid.buffer = screen->pixels;
    VGA_rowbytes = vid.rowbytes = screen->pitch;
    vid.conbuffer = vid.buffer;
    vid.conrowbytes = vid.rowbytes;
    vid.direct = (pixel_t *) screen->pixels;
    
    // allocate z buffer and surface cache
    chunk = vid.width * vid.height * sizeof (*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes (vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL)
        Sys_Error ("Not enough memory for video mode\n");

    // initialize the cache memory 
        cache = (byte *) d_pzbuffer
                + vid.width * vid.height * sizeof (*d_pzbuffer);
    D_InitCaches (cache, cachesize);

    // initialize the mouse
    SDL_ShowCursor(0);
    
    vid_initialized = true;
}

void    VID_Shutdown (void)
{
	if (vid_initialized)
	{
		if (screen != NULL && lockcount > 0)
			SDL_UnlockSurface (screen);

        if (!force_old_render) {
            SDL_UnlockTexture(texture);
        }
		vid_initialized = 0;
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}

void    VID_CalcScreenDimensions ()
{
    // Scaling code, courtesy of ChatGPT
    // Original, pre-scale screen size
    blitRect.x = 0;
    blitRect.y = 0;
    blitRect.w = vid.width;
    blitRect.h = vid.height;
        
    // Get window dimensions
    int winW = windowSurface->w;
    int winH = windowSurface->h;
    
    // Get scaleBuffer dimensions
    int bufW = vid.width;
    int bufH = vid.height;
    float bufAspect = (float)bufW / bufH;
    
    // Calculate scaled dimensions
    int destW, destH;
    if ((float)winW / winH > bufAspect) {
        // Window is wider than buffer, black bars on sides
        destH = winH;
        destW = (int)(winH * bufAspect);
    } else {
        // Window is taller than buffer, black bars on top/bottom
        destW = winW;
        destH = (int)(winW / bufAspect);
    }
    
    // Center the destination rectangle
    destRect.x = (winW - destW) / 2;
    destRect.y = (winH - destH) / 2;
    destRect.w = destW;
    destRect.h = destH;
}

void    VID_Update (vrect_t *rects)
{
    /*
    SDL_Rect *sdlrects;
    int n, i;
    vrect_t *rect;

    // Two-pass system, since Quake doesn't do it the SDL way...

    // First, count the number of rectangles
    n = 0;
    for (rect = rects; rect; rect = rect->pnext)
        ++n;

    // Second, copy them to SDL rectangles and update
    if (!(sdlrects = (SDL_Rect *)alloca(n*sizeof(*sdlrects))))
        Sys_Error("Out of memory");
    i = 0;
    for (rect = rects; rect; rect = rect->pnext)
    {
        sdlrects[i].x = rect->x;
        sdlrects[i].y = rect->y;
        sdlrects[i].w = rect->width;
        sdlrects[i].h = rect->height;
        ++i;
    }
    */
    // CyanBun96: the above part would update different areas of the screen
    // seperately with SDL_UpdateRects, but SDL2 replaced it with the whole
    // SDL_Renderer system. The following code just updates the entire screen
    // every frame, which I guess could be a bit inefficient, but I haven't
    // run into any issues even on my weakest machine

    // Machines without a proper GPU will try to simulate one with software,
    // adding a lot of overhead. In my tests, software rendering accomplished
    // the same result with almost a 200% performance increase.

    if (!force_old_render) { // hardware-accelerated rendering
        SDL_LockTexture(texture, &blitRect, &argbbuffer->pixels,
            &argbbuffer->pitch);
        SDL_LowerBlit(screen, &blitRect, argbbuffer, &blitRect);
        SDL_RenderClear(renderer);
        SDL_UnlockTexture(texture);
        SDL_RenderCopy(renderer, texture, NULL, &destRect);
        SDL_RenderPresent(renderer);
    }
    else { // pure software rendering
        SDL_UpperBlit(screen, NULL, scaleBuffer, NULL);
        SDL_UpperBlitScaled(scaleBuffer, &blitRect, windowSurface, &destRect);
        SDL_UpdateWindowSurface(window);
    }
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
    Uint8 *offset;


    if (!screen) return;
    if ( x < 0 ) x = screen->w+x-1;
    offset = (Uint8 *)screen->pixels + y*screen->pitch + x;
    while ( height-- )
    {
        memcpy(offset, pbitmap, width);
        offset += screen->pitch;
        pbitmap += width;
    }
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
    return;
    // Unneeded with SDL2
    // if (!screen) return;
    // if (x < 0) x = screen->w+x-1;
    // SDL_UpdateRect(screen, x, y, width, height);
}


/*
================
Sys_SendKeyEvents
================
*/

// TODO: what the beep is input doing in the fucking video source anyway?
void Sys_SendKeyEvents(void)
{
    SDL_Event event;
    int sym, state;
    int modstate;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                sym = event.key.keysym.sym;
                state = event.key.state;
                modstate = SDL_GetModState();
                switch(sym)
                {
                   case SDLK_DELETE: sym = K_DEL; break;
                   case SDLK_BACKSPACE: sym = K_BACKSPACE; break;
                   case SDLK_F1: sym = K_F1; break;
                   case SDLK_F2: sym = K_F2; break;
                   case SDLK_F3: sym = K_F3; break;
                   case SDLK_F4: sym = K_F4; break;
                   case SDLK_F5: sym = K_F5; break;
                   case SDLK_F6: sym = K_F6; break;
                   case SDLK_F7: sym = K_F7; break;
                   case SDLK_F8: sym = K_F8; break;
                   case SDLK_F9: sym = K_F9; break;
                   case SDLK_F10: sym = K_F10; break;
                   case SDLK_F11: sym = K_F11; break;
                   case SDLK_F12: sym = K_F12; break;
                   //case SDLK_BREAK:
                   case SDLK_PAUSE: sym = K_PAUSE; break;
                   case SDLK_UP: sym = K_UPARROW; break;
                   case SDLK_DOWN: sym = K_DOWNARROW; break;
                   case SDLK_RIGHT: sym = K_RIGHTARROW; break;
                   case SDLK_LEFT: sym = K_LEFTARROW; break;
                   case SDLK_INSERT: sym = K_INS; break;
                   case SDLK_HOME: sym = K_HOME; break;
                   case SDLK_END: sym = K_END; break;
                   case SDLK_PAGEUP: sym = K_PGUP; break;
                   case SDLK_PAGEDOWN: sym = K_PGDN; break;
                   case SDLK_RSHIFT:
                   case SDLK_LSHIFT: sym = K_SHIFT; break;
                   case SDLK_RCTRL:
                   case SDLK_LCTRL: sym = K_CTRL; break;
                   case SDLK_RALT:
                   case SDLK_LALT: sym = K_ALT; break;
                   case SDLK_KP_0: 
                       if(modstate & KMOD_NUM) sym = K_INS; 
                       else sym = SDLK_0;
                       break;
                   case SDLK_KP_1:
                       if(modstate & KMOD_NUM) sym = K_END;
                       else sym = SDLK_1;
                       break;
                   case SDLK_KP_2:
                       if(modstate & KMOD_NUM) sym = K_DOWNARROW;
                       else sym = SDLK_2;
                       break;
                   case SDLK_KP_3:
                       if(modstate & KMOD_NUM) sym = K_PGDN;
                       else sym = SDLK_3;
                       break;
                   case SDLK_KP_4:
                       if(modstate & KMOD_NUM) sym = K_LEFTARROW;
                       else sym = SDLK_4;
                       break;
                   case SDLK_KP_5: sym = SDLK_5; break;
                   case SDLK_KP_6:
                       if(modstate & KMOD_NUM) sym = K_RIGHTARROW;
                       else sym = SDLK_6;
                       break;
                   case SDLK_KP_7:
                       if(modstate & KMOD_NUM) sym = K_HOME;
                       else sym = SDLK_7;
                       break;
                   case SDLK_KP_8:
                       if(modstate & KMOD_NUM) sym = K_UPARROW;
                       else sym = SDLK_8;
                       break;
                   case SDLK_KP_9:
                       if(modstate & KMOD_NUM) sym = K_PGUP;
                       else sym = SDLK_9;
                       break;
                   case SDLK_KP_PERIOD:
                       if(modstate & KMOD_NUM) sym = K_DEL;
                       else sym = SDLK_PERIOD;
                       break;
                   case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
                   case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK; break;
                   case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
                   case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
                   case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
                   case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
                }
                // If we're not directly handled and still above 255
                // just force it to 0
                if(sym > 255) sym = 0;
                Key_Event(sym, state);
                break;

            // WinQuake behavior: Use Mouse OFF disables mouse input entirely
            // ON grabs the mouse, kinda like SetRelativeMouseMode(SDL_TRUE)
            // Fullscreen grabs the mouse unconditionally

            case SDL_MOUSEMOTION:
                if ( (event.motion.x != (vid.width/2)) ||
                     (event.motion.y != (vid.height/2)) ) {
                    mouse_x = event.motion.xrel*10;
                    mouse_y = event.motion.yrel*10;
                    /*if ( (event.motion.x < ((vid.width/2)-(vid.width/4))) ||
                         (event.motion.x > ((vid.width/2)+(vid.width/4))) ||
                         (event.motion.y < ((vid.height/2)-(vid.height/4))) ||
                         (event.motion.y > ((vid.height/2)+(vid.height/4))) ) {
                        SDL_WarpMouse(vid.width/2, vid.height/2);
                    }*/
                }
                break;
	    case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        SDLWindowFlags = SDL_GetWindowFlags(window);
                        windowSurface = SDL_GetWindowSurface(window);
                        VID_CalcScreenDimensions();
                        break;
                }
                break;
            case SDL_QUIT:
                CL_Disconnect ();
                Host_ShutdownServer(false);        
                Sys_Quit ();
                break;
            default:
                break;
        }
    }
}

void IN_Init (void)
{
    if ( COM_CheckParm ("-nomouse") )
        return;
    mouse_x = mouse_y = 0.0;
    mouse_avail = 1;
}

void IN_Shutdown (void)
{
    mouse_avail = 0;
}

void IN_Commands (void)
{
    int i;
    int mouse_buttonstate;
   
    if (!mouse_avail) return;
   
    i = SDL_GetMouseState(NULL, NULL);
    /* Quake swaps the second and third buttons */
    mouse_buttonstate = (i & ~0x06) | ((i & 0x02)<<1) | ((i & 0x04)>>1);
    for (i=0 ; i<3 ; i++) {
        if ( (mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, true);

        if ( !(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, false);
    }
    mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move (usercmd_t *cmd)
{
    if (!mouse_avail)
        return;
    if (!(SDLWindowFlags & (SDL_WINDOW_FULLSCREEN
                | SDL_WINDOW_FULLSCREEN_DESKTOP))
                && !_windowed_mouse.value)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // TODO aspect ratio-based sensitivity
    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;
   
    if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
        cmd->sidemove += m_side.value * mouse_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    if (in_mlook.state & 1)
        V_StopPitchDrift ();
   
    if ( (in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * mouse_y;
        else
            cmd->forwardmove -= m_forward.value * mouse_y;
    }
    mouse_x = mouse_y = 0.0;
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
    return 0;
}

/*
================
VID_MenuDraw
================
*/
void VID_MenuDraw (void)
{
    puts("TODO");
    /*qpic_t          *p;
    char            *ptr;
    int                     lnummodes, i, j, k, column, row, dup, dupmode;
    char            temp[100];
    vmode_t         *pv;
    modedesc_t      tmodedesc;

    p = Draw_CachePic ("gfx/vidmodes.lmp");
    M_DrawPic ( (320-p->width)/2, 4, p);

    for (i=0 ; i<3 ; i++)
    {
            ptr = VID_GetModeDescriptionMemCheck (i);
            modedescs[i].modenum = modelist[i].modenum;
            modedescs[i].desc = ptr;
            modedescs[i].ismode13 = 0;
            modedescs[i].iscur = 0;

            if (vid_modenum == i)
                    modedescs[i].iscur = 1;
    }

    vid_wmodes = 3;
    lnummodes = VID_NumModes ();

    for (i=3 ; i<lnummodes ; i++)
    {
            ptr = VID_GetModeDescriptionMemCheck (i);
            pv = VID_GetModePtr (i);

    // we only have room for 15 fullscreen modes, so don't allow
    // 360-wide modes, because if there are 5 320-wide modes and
    // 5 360-wide modes, we'll run out of space
            if (ptr && ((pv->width != 360) || COM_CheckParm("-allow360")))
            {
                    dup = 0;

                    for (j=3 ; j<vid_wmodes ; j++)
                    {
                            if (!strcmp (modedescs[j].desc, ptr))
                            {
                                    dup = 1;
                                    dupmode = j;
                                    break;
                            }
                    }

                    if (dup || (vid_wmodes < MAX_MODEDESCS))
                    {
                            if (!dup || !modedescs[dupmode].ismode13 || COM_CheckParm("-noforcevga"))
                            {
                                    if (dup)
                                    {
                                            k = dupmode;
                                    }
                                    else
                                    {
                                            k = vid_wmodes;
                                    }

                                    modedescs[k].modenum = i;
                                    modedescs[k].desc = ptr;
                                    modedescs[k].ismode13 = pv->mode13;
                                    modedescs[k].iscur = 0;
                                    modedescs[k].width = pv->width;

                                    if (i == vid_modenum)
                                            modedescs[k].iscur = 1;

                                    if (!dup)
                                            vid_wmodes++;
                            }
                    }
            }
    }

    // sort the modes on width (to handle picking up oddball dibonly modes
    // after all the others)
    for (i=3 ; i<(vid_wmodes-1) ; i++)
    {
            for (j=(i+1) ; j<vid_wmodes ; j++)
            {
                    if (modedescs[i].width > modedescs[j].width)
                    {
                            tmodedesc = modedescs[i];
                            modedescs[i] = modedescs[j];
                            modedescs[j] = tmodedesc;
                    }
            }
    }


    M_Print (13*8, 36, "Windowed Modes");

    column = 16;
    row = 36+2*8;

    for (i=0 ; i<3; i++)
    {
            if (modedescs[i].iscur)
                    M_PrintWhite (column, row, modedescs[i].desc);
            else
                    M_Print (column, row, modedescs[i].desc);

            column += 13*8;
    }

    if (vid_wmodes > 3)
    {
            M_Print (12*8, 36+4*8, "Fullscreen Modes");

            column = 16;
            row = 36+6*8;

            for (i=3 ; i<vid_wmodes ; i++)
            {
                    if (modedescs[i].iscur)
                            M_PrintWhite (column, row, modedescs[i].desc);
                    else
                            M_Print (column, row, modedescs[i].desc);

                    column += 13*8;

                    if (((i - 3) % VID_ROW_SIZE) == (VID_ROW_SIZE - 1))
                    {
                            column = 16;
                            row += 8;
                    }
            }
    }

    // line cursor
    if (vid_testingmode)
    {
            sprintf (temp, "TESTING %s",
                            modedescs[vid_line].desc);
            M_Print (13*8, 36 + MODE_AREA_HEIGHT * 8 + 8*4, temp);
            M_Print (9*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6,
                            "Please wait 5 seconds...");
    }
    else
    {
            M_Print (9*8, 36 + MODE_AREA_HEIGHT * 8 + 8,
                            "Press Enter to set mode");
            M_Print (6*8, 36 + MODE_AREA_HEIGHT * 8 + 8*3,
                            "T to test mode for 5 seconds");
            ptr = VID_GetModeDescription2 (vid_modenum);

            if (ptr)
            {
                    sprintf (temp, "D to set default: %s", ptr);
                    M_Print (2*8, 36 + MODE_AREA_HEIGHT * 8 + 8*5, temp);
            }

            ptr = VID_GetModeDescription2 ((int)_vid_default_mode_win.value);

            if (ptr)
            {
                    sprintf (temp, "Current default: %s", ptr);
                    M_Print (3*8, 36 + MODE_AREA_HEIGHT * 8 + 8*6, temp);
            }

            M_Print (15*8, 36 + MODE_AREA_HEIGHT * 8 + 8*8,
                            "Esc to exit");

            row = 36 + 2*8 + (vid_line / VID_ROW_SIZE) * 8;
            column = 8 + (vid_line % VID_ROW_SIZE) * 13*8;

            if (vid_line >= 3)
                    row += 3*8;

            M_DrawCharacter (column, row, 12+((int)(realtime*4)&1));
    }*/
}

/*
================
VID_MenuKey
================
*/
void VID_MenuKey (int key)
{
    puts("TODO");
    /*
    if (vid_testingmode)
        return;

    switch (key)
    {
    case K_ESCAPE:
        S_LocalSound ("misc/menu1.wav");
        M_Menu_Options_f ();
        break;

    case K_LEFTARROW:
        S_LocalSound ("misc/menu1.wav");
        vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
                   ((vid_line + 2) % VID_ROW_SIZE);

        if (vid_line >= vid_wmodes)
            vid_line = vid_wmodes - 1;
        break;

    case K_RIGHTARROW:
        S_LocalSound ("misc/menu1.wav");
        vid_line = ((vid_line / VID_ROW_SIZE) * VID_ROW_SIZE) +
                   ((vid_line + 4) % VID_ROW_SIZE);

        if (vid_line >= vid_wmodes)
            vid_line = (vid_line / VID_ROW_SIZE) * VID_ROW_SIZE;
        break;

    case K_UPARROW:
        S_LocalSound ("misc/menu1.wav");
        vid_line -= VID_ROW_SIZE;

        if (vid_line < 0)
        {
            vid_line += ((vid_wmodes + (VID_ROW_SIZE - 1)) /
                    VID_ROW_SIZE) * VID_ROW_SIZE;

            while (vid_line >= vid_wmodes)
                vid_line -= VID_ROW_SIZE;
        }
        break;

    case K_DOWNARROW:
        S_LocalSound ("misc/menu1.wav");
        vid_line += VID_ROW_SIZE;

        if (vid_line >= vid_wmodes)
        {
            vid_line -= ((vid_wmodes + (VID_ROW_SIZE - 1)) /
                    VID_ROW_SIZE) * VID_ROW_SIZE;

            while (vid_line < 0)
                vid_line += VID_ROW_SIZE;
        }
        break;

    case K_ENTER:
        S_LocalSound ("misc/menu1.wav");
        VID_SetMode (modedescs[vid_line].modenum, vid_curpal);
        break;

    case 'T':
    case 't':
        S_LocalSound ("misc/menu1.wav");
    // have to set this before setting the mode because WM_PAINT
    // happens during the mode set and does a VID_Update, which
    // checks vid_testingmode
        vid_testingmode = 1;
        vid_testendtime = realtime + 5.0;

        if (!VID_SetMode (modedescs[vid_line].modenum, vid_curpal))
        {
            vid_testingmode = 0;
        }
        break;

    case 'D':
    case 'd':
        S_LocalSound ("misc/menu1.wav");
        firstupdate = 0;
        Cvar_SetValue ("_vid_default_mode_win", vid_modenum);
        break;

    default:
        break;
    }*/
}
