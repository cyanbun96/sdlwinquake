// vid_sdl.h -- sdl video driver 

#include <SDL2/SDL.h>
#include "quakedef.h"
#include "d_local.h"

SDL_Surface *windowSurface;
SDL_Surface *scaleBuffer;
SDL_Window *window;
SDL_Renderer *renderer;
SDL_Surface *argbbuffer;
SDL_Texture *texture;
SDL_Rect blitRect;
SDL_Rect destRect;

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
// No support for option menus
void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

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

    if (vid.width > 1280 || vid.height > 1024)
    {
        Con_Printf("WARNING: vanilla maximum resolution is 1280x1024\n");
    	//Sys_Error("Maximum Resolution is 1280 width and 1024 height");
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    window = SDL_CreateWindow(NULL,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          vid.width, vid.height,
                                          flags);
    renderer = SDL_CreateRenderer(window, -1, 0);
    screen = SDL_CreateRGBSurfaceWithFormat(0, vid.width, vid.height, 8, SDL_PIXELFORMAT_INDEX8);
    //TODO render type switching
    argbbuffer = SDL_CreateRGBSurfaceWithFormatFrom(
        NULL, vid.width, vid.height, 0, 0, SDL_PIXELFORMAT_ARGB8888);
    scaleBuffer = SDL_CreateRGBSurfaceWithFormat(
        0, vid.width, vid.height, 8, SDL_GetWindowPixelFormat(window));
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                vid.width, vid.height);
    windowSurface = SDL_GetWindowSurface(window);

    if (!screen)
        Sys_Error("VID: Couldn't set video mode: %s\n", SDL_GetError());
    VID_CalcScreenDimensions();
    SDL_UpdateWindowSurface(window);

    sprintf(caption, "SDL2WinQuake - Version %4.2f", VERSION);
    SDL_SetWindowTitle(window, (const char*)&caption);
    
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
	if (vid_initialized) //TODO clean up the other stuff too
	{
		if (screen != NULL && lockcount > 0)
			SDL_UnlockSurface (screen);

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

    // TODO render type switching
    if (0){ // hardware-accelerated rendering
        SDL_LockTexture(texture, &blitRect, &argbbuffer->pixels,
            &argbbuffer->pitch);
        SDL_LowerBlit(screen, &blitRect, argbbuffer, &blitRect);
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
		    // CyanBun96: this does the same thing, right?
		    // TODO: implement something less aggressive
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                break;
	    case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        windowSurface = SDL_GetWindowSurface(window);
                        VID_CalcScreenDimensions();
                        break;
                }
		// CyanBun96:
		// if we call renderclear here we can avoid calling it every
		// frame, gaining about 1-2% performance on an ancient
		// Acer Aspire One without graphics drivers for Linux.
		// probably other systems too i guess. not that it matters.
                // TODO render type switching
                SDL_FillRect(argbbuffer, NULL, SDL_MapRGB(argbbuffer->format, 0, 0, 0));
                SDL_RenderClear(renderer);
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
