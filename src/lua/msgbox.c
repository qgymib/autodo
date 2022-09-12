#include "runtime.h"
#include "msgbox.h"
#include <stdlib.h>

typedef struct msgbox_context
{
    atd_runtime_t*  rt; /**< Global runtime */
} msgbox_context_t;

int atd_lua_msgbox(lua_State *L)
{
    msgbox_context_t* msg_ctx = malloc(sizeof(msgbox_context_t));
    msg_ctx->rt = atd_get_runtime(L);

    return 0;
}
