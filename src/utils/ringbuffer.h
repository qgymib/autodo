#ifndef __EV_RINGBUFFER_INTERNAL_H__
#define __EV_RINGBUFFER_INTERNAL_H__

#include "lua/api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ring buffer flags
 * @see ring_buffer_reserve
 * @see ring_buffer_commit
 */
typedef enum ring_buffer_flag
{
    RINGBUFFER_FLAG_OVERWRITE   = 0x01 << 0x00,         /**< Overwrite data if no enough free space */
    RINGBUFFER_FLAG_DISCARD     = 0x01 << 0x01,         /**< Discard operation */
    RINGBUFFER_FLAG_ABANDON     = 0x01 << 0x02,         /**< Abandon token */
}ring_buffer_flag_t;

typedef enum ring_buffer_node_state
{
    RINGBUFFER_STAT_WRITING,                            /** Writing */
    RINGBUFFER_STAT_COMMITTED,                          /** Committed */
    RINGBUFFER_STAT_READING,                            /** Reading */
}ring_buffer_node_state_t;

/**
 * @brief Ring buffer data token
 */
typedef struct ring_buffer_token
{
    union
    {
        size_t                      size;               /**< Data length */
        void*                       _align;             /**< Padding field used for make sure address align */
    }size;

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable : 4200)
#endif
    uint8_t                         data[];             /**< Data body */
#if defined(_MSC_VER)
#   pragma warning(pop)
#endif
}ring_buffer_token_t;

typedef struct ring_buffer_node
{
    struct
    {
        size_t                      off_next;           /**< Next position in chain. NOT `NULL` in any condition. */
        size_t                      off_prev;           /**< Previous position in chain. NOT `NULL` in any condition. */
    }chain_pos;

    struct
    {
        size_t                      off_newer;          /**< Newer node. `0` if not exists. */
        size_t                      off_older;          /**< Older node. `NULL` if not exists. */
    }chain_time;

    ring_buffer_node_state_t        state;              /**< Node status */
    ring_buffer_token_t             token;              /**< User data */
}ring_buffer_node_t;

/**
 * @brief Ring buffer counter
 */
typedef struct ring_buffer_counter
{
    size_t                          committed;          /**< The number of committed nodes */
    size_t                          writing;            /**< The number of writing nodes */
    size_t                          reading;            /**< The number of reading nodes */
}ring_buffer_counter_t;

/**
 * @brief Ring buffer
 */
typedef struct ring_buffer
{
    struct
    {
        size_t                      capacity;           /**< Available length. */
    }config;

    struct
    {
        size_t                      off_HEAD;           /**< The newest node which is in READING/WRITING/COMMITTED status. */
        size_t                      off_TAIL;           /**< The oldest node which is in READING/WRITING/COMMITTED status. */
        size_t                      off_reserve;        /**< The oldest node which is in WRITING/COMMITTED status. */
    }node;

    ring_buffer_counter_t           counter;            /** Counter */

    /**
     * @brief Ring buffer base.
     * Data store start at the address of basis.
     * However first `sizeof(void*)` bytes is always zero, by which design all
     * the node position will not be 0, and access to 0 offset data will cause crash.
     */
    void*                           basis;
}ring_buffer_t;

/**
 * @brief Initialize ring buffer on the give memory.
 * @param[in,out] buffer    Memory area
 * @param[in] size          The size of memory area
 * @return                  Initialized ring buffer, the address is equal it \p buffer
 */
API_LOCAL ring_buffer_t* ring_buffer_init(void* buffer, size_t size);

/**
 * @brief Acquire a token for write.
 * @see ring_buffer_commit
 * @param[in,out] handler   The pointer to the ring buffer
 * @param[in] size          The size of area you want
 * @param[in] flags         #ring_buffer_flag_t
 * @return                  A token for write. This token must be committed by #ring_buffer_commit
 */
API_LOCAL ring_buffer_token_t* ring_buffer_reserve(ring_buffer_t* handler, size_t size, int flags);

/**
 * @brief Acquire a token for read.
 * @see ring_buffer_commit
 * @param[in,out] handler   The pointer to the ring buffer
 * @return                  A token for read. This token must be committed by #ring_buffer_commit
 */
API_LOCAL ring_buffer_token_t* ring_buffer_consume(ring_buffer_t* handler);

/**
 * @brief Commit a token.
 *
 * If the token was created by #ring_buffer_reserve, then this token can be
 * consumed by #ring_buffer_consume. If flag contains
 * #RINGBUFFER_FLAG_DISCARD, then this token will be
 * destroyed.
 *
 * If the token was created by #ring_buffer_consume, then this token will be
 * destroyed. If flag contains
 * #RINGBUFFER_FLAG_DISCARD, then this token will be
 * marked as readable, and #ring_buffer_consume will be able to get this
 * token.
 *
 * If there are two or more consumers, discard a reading token may be failed.
 * Consider the following condition:
 * 1. `CONSUMER_A` acquire a reading token `READ_A` (success)
 * 2. `CONSUMER_B` acquire a reading token `READ_B` (success)
 * 3. `CONSUMER_A` discard `READ_A` (failure)
 * > This happens because ring buffer must guarantee data order is FIFO. If
 * > `CONSUMER_A` is able to discard `READ_A`, then next consumer will get
 * > `READ_A` which is older than `READ_B`. This condition must not happen.
 *
 * @pre In the precondition, parameter `token' must be returned from either
 *   #ring_buffer_consume or #ring_buffer_consume.
 * @post In the postcondition, parameter `token' will be invalid to user if
 *   return code is zero.
 * @param[in,out] handler   The pointer to the ring buffer
 * @param[in,out] token     The token going to be committed
 * @param[in] flags         #ring_buffer_flag_t
 * @return                  0 if success, otherwise failure
 */
API_LOCAL int ring_buffer_commit(ring_buffer_t* handler, ring_buffer_token_t* token, int flags);

/**
 * @brief Get counter
 * @param[in] handler   The ring buffer
 * @param[out] counter  The pointer to counter
 * @return              The sum of all counter
 */
API_LOCAL size_t ring_buffer_count(ring_buffer_t* handler, ring_buffer_counter_t* counter);

/**
 * @brief Get how much bytes the ring buffer structure cost
 * @return          size of ring_buffer_t
 */
API_LOCAL size_t ring_buffer_heap_cost(void);

/**
 * @brief Calculate the how much space of given size data will take place.
 * @param[in] size      The size of data
 * @return              The space of data will take place
 */
API_LOCAL size_t ring_buffer_node_cost(size_t size);

/**
 * @brief Get the begin node of ring buffer.
 * @param[in] handler   The ring buffer
 * @return              The iterator
 */
API_LOCAL ring_buffer_token_t* ring_buffer_begin(const ring_buffer_t* handler);

/**
 * @brief Get next token.
 * @param[in] handler   The ring buffer
 * @param[in] token     The ring buffer token
 * @return              Next token
 */
API_LOCAL ring_buffer_token_t* ring_buffer_next(const ring_buffer_t* handler,
    const ring_buffer_token_t* token);

#ifdef __cplusplus
}
#endif
#endif
