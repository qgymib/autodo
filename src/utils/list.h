/** @file
 * Double Linked List.
 * Nodes organize as:
 * ```
 * |--------|--------|--------|--------|--------|--------|--------|
 *   HEAD ------------------------------------------------> TAIL
 *     front -------------------------------------------> back
 *       before -------------------------------------> after
 * ```
 */
#ifndef __EV_LIST_H__
#define __EV_LIST_H__

#include <autodo.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup EV_UTILS_LIST List
 * @ingroup EV_UTILS
 * @{
 */

/**
 * @brief Static initializer for #atd_list_t
 * @see atd_list_t
 */
#define EV_LIST_INIT            { NULL, NULL, 0 }

/**
 * @brief Static initializer for #std_list_node_t
 * @see std_list_node_t
 */
#define EV_LIST_NODE_INIT       { NULL, NULL }

/**
 * @brief Initialize Double Linked List.
 * @note It is guarantee that memset() to zero have the same affect.
 * @param[out] handler  Pointer to list
 */
AUTO_LOCAL void ev_list_init(atd_list_t* handler);

/**
 * @brief Insert a node to the head of the list.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      Pointer to a new node
 */
AUTO_LOCAL void ev_list_push_front(atd_list_t* handler, std_list_node_t* node);

/**
 * @brief Insert a node to the tail of the list.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      Pointer to a new node
 */
AUTO_LOCAL void ev_list_push_back(atd_list_t* handler, std_list_node_t* node);

/**
 * @brief Insert a node in front of a given node.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] pos       Pointer to a exist node
 * @param[in,out] node      Pointer to a new node
 */
AUTO_LOCAL void ev_list_insert_before(atd_list_t* handler, std_list_node_t* pos, std_list_node_t* node);

/**
 * @brief Insert a node right after a given node.
 * @warning the node must not exist in any list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] pos       Pointer to a exist node
 * @param[in,out] node      Pointer to a new node
 */
AUTO_LOCAL void ev_list_insert_after(atd_list_t* handler, std_list_node_t* pos, std_list_node_t* node);

/**
 * @brief Delete a exist node
 * @warning The node must already in the list.
 * @param[in,out] handler   Pointer to list
 * @param[in,out] node      The node you want to delete
 */
AUTO_LOCAL void ev_list_erase(atd_list_t* handler, std_list_node_t* node);

/**
 * @brief Get the number of nodes in the list.
 * @param[in] handler   Pointer to list
 * @return              The number of nodes
 */
AUTO_LOCAL size_t ev_list_size(const atd_list_t* handler);

/**
 * @brief Get the first node and remove it from the list.
 * @param[in,out] handler   Pointer to list
 * @return                  The first node
 */
AUTO_LOCAL std_list_node_t* ev_list_pop_front(atd_list_t* handler);

/**
 * @brief Get the last node and remove it from the list.
 * @param[in,out] handler   Pointer to list
 * @return                  The last node
 */
AUTO_LOCAL std_list_node_t* ev_list_pop_back(atd_list_t* handler);

/**
 * @brief Get the first node.
 * @param[in] handler   Pointer to list
 * @return              The first node
 */
AUTO_LOCAL std_list_node_t* ev_list_begin(const atd_list_t* handler);

/**
 * @brief Get the last node.
 * @param[in] handler   The handler of list
 * @return              The last node
 */
AUTO_LOCAL std_list_node_t* ev_list_end(const atd_list_t* handler);

/**
* @brief Get next node.
* @param[in] node   Current node
* @return           The next node
*/
AUTO_LOCAL std_list_node_t* ev_list_next(const std_list_node_t* node);

/**
 * @brief Get previous node.
 * @param[in] node  current node
 * @return          previous node
 */
AUTO_LOCAL std_list_node_t* ev_list_prev(const std_list_node_t* node);

/**
 * @brief Move all elements from \p src into the end of \p dst.
 * @param[in] dst   Destination list.
 * @param[in] src   Source list.
 */
AUTO_LOCAL void ev_list_migrate(atd_list_t* dst, atd_list_t* src);

/**
 * @} EV_UTILS/EV_UTILS_LIST
 */

#ifdef __cplusplus
}
#endif
#endif  /* __EV_LIST_H__ */
