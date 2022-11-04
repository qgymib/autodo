#include "list.h"
#include "utils/list.h"

const auto_api_list_t api_list = {
    ev_list_init,                       /* .init */
    ev_list_push_front,                 /* .push_front */
    ev_list_push_back,                  /* .push_back */
    ev_list_insert_before,              /* .insert_before */
    ev_list_insert_after,               /* .insert_after */
    ev_list_erase,                      /* .erase */
    ev_list_size,                       /* .size */
    ev_list_pop_front,                  /* .pop_front */
    ev_list_pop_back,                   /* .pop_back */
    ev_list_begin,                      /* .begin */
    ev_list_end,                        /* .end */
    ev_list_next,                       /* .next */
    ev_list_prev,                       /* .prev */
    ev_list_migrate,                    /* .migrate */
};
