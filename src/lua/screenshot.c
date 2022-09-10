#include "screenshot.h"
#include <string.h>

#if defined(_WIN32)

int auto_lua_take_screenshot(lua_State* L)
{
    (void)L;
    // TODO
    return 0;
}

#else

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

static cairo_status_t _on_write_png(void *closure, const unsigned char *data,
    unsigned int length)
{
    luaL_Buffer* p_buf = closure;
    luaL_addlstring(p_buf, (const char*)data, length);

    return CAIRO_STATUS_SUCCESS;
}

/**
 * @see https://stackoverflow.com/questions/24988164/c-fast-screenshots-in-linux-for-use-with-opencv
 * @see https://gist.github.com/bozdag/9909679
 * @param L
 * @return
 */
int atd_lua_screenshot(lua_State *L)
{
    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    Display* display = XOpenDisplay(NULL);
    int screen = DefaultScreen(display);
    Window root = DefaultRootWindow(display);

    cairo_surface_t* surface = cairo_xlib_surface_create(
        display, root, DefaultVisual(display, screen),
        DisplayWidth(display, screen),
        DisplayHeight(display, screen));

    cairo_surface_write_to_png_stream(surface, _on_write_png, &buf);
    cairo_surface_destroy(surface);
    XCloseDisplay(display);

    luaL_pushresult(&buf);

    return 1;
}

#endif
