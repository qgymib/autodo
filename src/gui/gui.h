#ifndef __AUTO_GUI_H__
#define __AUTO_GUI_H__
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GUI event.
 */
typedef enum auto_gui_event
{
    AUTO_GUI_READY,             /**< Gui is ready */
    AUTO_GUI_QUIT,              /**< GUI is abort to quit */
} auto_gui_event_t;

typedef struct auto_gui_msg
{
    auto_gui_event_t    type;    /**< Event type */
    union
    {
        int             reserve;
    } as;
} auto_gui_msg_t;

typedef struct auto_gui_startup_info
{
    /**
     * @brief The number of arguments passed to the program.
     */
    int     argc;

    /**
     * @brief The arguments passed to the program.
     */
    char**  argv;

    /**
     * @brief (Windows only) Controls how the window is to be shown.
     */
    int     nShowCmd;

    /**
     * @brief User data.
     */
    void*   udata;

    /**
     * @brief Gui event callback
     * @param[in] msg   Event data.
     * @param[in] udata User data.
     */
    void (*on_event)(auto_gui_msg_t* msg, void* udata);
} auto_gui_startup_info_t;

/**
 * @brief Gui entrypoint
 * @node Before #AUTO_GUI_READY happen, no other gui functions can be called.
 * @param[in] info  Startup information.
 * @return          Exit code.
 */
int auto_gui(auto_gui_startup_info_t* info);

void auto_gui_exit(void);

#ifdef __cplusplus
}
#endif
#endif
