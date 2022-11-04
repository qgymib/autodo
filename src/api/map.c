#include "map.h"
#include "utils/map.h"

const auto_api_map_t api_map = {
    ev_map_init,                        /* .map.init */
    ev_map_insert,                      /* .map.insert */
    ev_map_replace,                     /* .map.replace */
    ev_map_erase,                       /* .map.erase */
    ev_map_size,                        /* .map.size */
    ev_map_find,                        /* .map.find */
    ev_map_find_lower,                  /* .map.find_lower */
    ev_map_find_upper,                  /* .map.find_upper */
    ev_map_begin,                       /* .map.begin */
    ev_map_end,                         /* .map.end */
    ev_map_next,                        /* .map.next */
    ev_map_prev,                        /* .map.prev */
};
