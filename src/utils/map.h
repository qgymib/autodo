#ifndef __EV_MAP_H__
#define __EV_MAP_H__

#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#include <autodo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Static initializer for #ev_map_low_t
 * @see eaf_map_low_t
 */
#define EV_MAP_LOW_INIT             { NULL }

/**
 * @brief Static initializer for #atd_map_node_t
 * @see eaf_map_low_node_t
 */
#define EV_MAP_LOW_NODE_INIT        { NULL, NULL, NULL }

/**
 * @brief find helper
 * @param ret           result
 * @param p_table       eaf_map_low_t*
 * @param USER_TYPE     user type
 * @param user          user data
 * @param user_vs_orig  compare rule
 */
#define EV_MAP_LOW_FIND_HELPER(ret, p_table, USER_TYPE, user, user_vs_orig) \
    do {\
        int flag_success = 0;\
        ev_map_low_t* __table = p_table;\
        ev_map_low_node_t* node = __table->rb_root;\
        ret = NULL;\
        while (node) {\
            USER_TYPE* orig = EAF_CONTAINER_OF(node, USER_TYPE, node);\
            int cmp_ret = user_vs_orig;\
            if (cmp_ret < 0) {\
                node = node->rb_left;\
            } else if (cmp_ret > 0) {\
                node = node->rb_right;\
            } else {\
                flag_success = 1;\
                ret = orig;\
                break;\
            }\
        }\
        if (flag_success) {\
            break;\
        }\
    } while (0)

/**
 * @brief insert helper
 * @param ret               result
 * @param p_table           eaf_map_low_t*
 * @param USER_TYPE         user type
 * @param user              address
 * @param user_vs_orig      compare rule
 */
#define EV_MAP_LOW_INSERT_HELPER(ret, p_table, USER_TYPE, user, user_vs_orig)   \
    do {\
        int flag_failed = 0;\
        ev_map_low_t* __table = p_table;\
        ev_map_low_node_t **new_node = &(__table->rb_root), *parent = NULL;\
        ret = eaf_errno_success;\
        while (*new_node) {\
            USER_TYPE* orig = EAF_CONTAINER_OF(*new_node, USER_TYPE, node);\
            int cmp_ret = user_vs_orig;\
            parent = *new_node;\
            if (cmp_ret < 0) {\
                new_node = &((*new_node)->rb_left);\
            } else if (cmp_ret > 0) {\
                new_node = &((*new_node)->rb_right);\
            } else {\
                flag_failed = 1;\
                ret = eaf_errno_duplicate;\
                break;\
            }\
        }\
        if (flag_failed) {\
            break;\
        }\
        ev_map_low_link_node(&(user)->node, parent, new_node);\
        ev_map_low_insert_color(&(user)->node, __table);\
    } while (0)

/**
 * @brief Inserting data into the tree.
 *
 * The insert instead must be implemented
 * in two steps: First, the code must insert the element in order as a red leaf
 * in the tree, and then the support library function #ev_map_low_insert_color
 * must be called.
 *
 * @param node      The node you want to insert
 * @param parent    The position you want to insert
 * @param rb_link   Will be set to `node`
 * @see ev_map_low_insert_color
 */
AUTO_LOCAL void ev_map_low_link_node(atd_map_node_t* node,
    atd_map_node_t* parent, atd_map_node_t** rb_link);

/**
 * @brief re-balancing ("recoloring") the tree.
 * @param node      The node just linked
 * @param root      The map
 * @see eaf_map_low_link_node
 */
AUTO_LOCAL void ev_map_low_insert_color(atd_map_node_t* node, atd_map_t* root);

/**
 * @defgroup EV_UTILS_MAP Map
 * @ingroup EV_UTILS
 * @{
 */

/**
 * @brief Initialize the map referenced by handler.
 * @param handler   The pointer to the map
 * @param cmp       The compare function. Must not NULL
 * @param arg       User defined argument. Can be anything
 */
AUTO_LOCAL void ev_map_init(atd_map_t* handler, atd_map_cmp_fn cmp, void* arg);

/**
 * @brief Insert the node into map.
 * @warning the node must not exist in any map.
 * @param handler   The pointer to the map
 * @param node      The node
 * @return          NULL if success, otherwise return the original node.
 */
AUTO_LOCAL atd_map_node_t* ev_map_insert(atd_map_t* handler, atd_map_node_t* node);

AUTO_LOCAL atd_map_node_t* ev_map_replace(atd_map_t* handler, atd_map_node_t* node);

/**
 * @brief Delete the node from the map.
 * @warning The node must already in the map.
 * @param self   The pointer to the map
 * @param node      The node
 */
AUTO_LOCAL void ev_map_erase(atd_map_t* self, atd_map_node_t* node);

/**
 * @brief Get the number of nodes in the map.
 * @param handler   The pointer to the map
 * @return          The number of nodes
 */
AUTO_LOCAL size_t ev_map_size(const atd_map_t* handler);

/**
 * @brief Finds element with specific key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
AUTO_LOCAL atd_map_node_t* ev_map_find(const atd_map_t* handler,
    const atd_map_node_t* key);

/**
 * @brief Returns an iterator to the first element not less than the given key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
AUTO_LOCAL atd_map_node_t* ev_map_find_lower(const atd_map_t* handler,
    const atd_map_node_t* key);

/**
 * @brief Returns an iterator to the first element greater than the given key
 * @param handler   The pointer to the map
 * @param key       The key
 * @return          An iterator point to the found node
 */
AUTO_LOCAL atd_map_node_t* ev_map_find_upper(const atd_map_t* handler,
    const atd_map_node_t* key);

/**
 * @brief Returns an iterator to the beginning
 * @param handler   The pointer to the map
 * @return          An iterator
 */
AUTO_LOCAL atd_map_node_t* ev_map_begin(const atd_map_t* handler);

/**
 * @brief Returns an iterator to the end
 * @param handler   The pointer to the map
 * @return          An iterator
 */
AUTO_LOCAL atd_map_node_t* ev_map_end(const atd_map_t* handler);

/**
 * @brief Get an iterator next to the given one.
 * @param node      Current iterator
 * @return          Next iterator
 */
AUTO_LOCAL atd_map_node_t* ev_map_next(const atd_map_node_t* node);

/**
 * @brief Get an iterator before the given one.
 * @param node      Current iterator
 * @return          Previous iterator
 */
AUTO_LOCAL atd_map_node_t* ev_map_prev(const atd_map_node_t* node);

/**
 * @} EV_UTILS/EV_UTILS_MAP
 */

#ifdef __cplusplus
}
#endif
#endif
