#include "api.h"
#include "api/coroutine.h"
#include "api/list.h"
#include "api/lua.h"
#include "api/map.h"
#include "api/memory.h"
#include "api/misc.h"
#include "api/notify.h"
#include "api/regex.h"
#include "api/sem.h"
#include "api/thread.h"
#include "api/timer.h"

const auto_api_t api = {
    &api_lua,
    &api_memory,
    &api_list,
    &api_map,
    &api_sem,
    &api_thread,
    &api_timer,
    &api_notify,
    &api_coroutine,
    &api_misc,
    &api_regex,
};

const auto_api_t* auto_api(void)
{
    return &api;
}
