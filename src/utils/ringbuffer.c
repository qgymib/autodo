#include "ringbuffer.h"
#include "utils.h"

#define EV_RB_BEG_POS(rb) \
    sizeof((rb)->basis)

#define EV_RB_BEG_NODE(rb)  \
    EV_RB_NODE(rb, EV_RB_BEG_POS(rb))

#define EV_RB_NODE(rb, pos)   \
    ((ring_buffer_node_t*)((uintptr_t)&((rb)->basis) + (pos)))

#define EV_RB_BASIS_DIFF(rb, ptr)   \
    ((uintptr_t)(ptr) - (uintptr_t)&((rb)->basis))

static void _ring_buffer_reinit(ring_buffer_t* rb)
{
    rb->counter.writing = 0;
    rb->counter.committed = 0;
    rb->counter.reading = 0;

    rb->node.off_reserve = 0;
    rb->node.off_HEAD = 0;
    rb->node.off_TAIL = 0;
}

/**
 * @brief Create the first node in empty ring buffer.
 * @param rb         ring buffer
 * @param data_len   length of user data
 * @param node_size  size of node
 * @return           token
 */
static ring_buffer_token_t* _ring_buffer_reserve_empty(ring_buffer_t* rb,
    size_t data_len, size_t node_size)
{
    /* check capacity */
    if (node_size > rb->config.capacity)
    {
        return NULL;
    }

    /* The value of ring_buffer_t::basis MUST be 0 */
    rb->node.off_HEAD = EV_RB_BEG_POS(rb);

    /* Initialize node */
    {
        ring_buffer_node_t* head = EV_RB_NODE(rb, rb->node.off_HEAD);
        head->state = RINGBUFFER_STAT_WRITING;
        head->token.size.size = data_len;

        /* Initialize position chain */
        head->chain_pos.off_next = rb->node.off_HEAD;
        head->chain_pos.off_prev = rb->node.off_HEAD;

        /* Initialize time chain */
        head->chain_time.off_newer = 0;
        head->chain_time.off_older = 0;
    }

    /* Initialize other resources */
    rb->node.off_TAIL = rb->node.off_HEAD;
    rb->node.off_reserve = rb->node.off_HEAD;
    rb->counter.writing++;

    ring_buffer_node_t* rb_oldest_reserve = EV_RB_NODE(rb, rb->node.off_reserve);
    return &rb_oldest_reserve->token;
}

/**
 * @brief Update time chain for \p new_node
 * @param rb         RingBuffer
 * @param new_node   New node
 */
static void _ring_buffer_update_time_for_new_node(ring_buffer_t* rb,
    ring_buffer_node_t* new_node)
{
    /* update chain_time */
    new_node->chain_time.off_newer = 0;
    new_node->chain_time.off_older = rb->node.off_HEAD;
    EV_RB_NODE(rb, new_node->chain_time.off_older)->chain_time.off_newer = EV_RB_BASIS_DIFF(rb, new_node);

    /* update HEAD */
    rb->node.off_HEAD = EV_RB_BASIS_DIFF(rb, new_node);
}

/**
 * @brief Perform overwrite
 */
static ring_buffer_token_t* _ring_buffer_perform_overwrite(
    ring_buffer_t* rb, uint8_t* start_point, ring_buffer_node_t* node_start,
    ring_buffer_node_t* node_end, size_t counter_lost_nodes,
    size_t data_len)
{
    ring_buffer_node_t* rb_tail = EV_RB_NODE(rb, rb->node.off_TAIL);
    ring_buffer_node_t* newer_node = EV_RB_NODE(rb, node_end->chain_time.off_newer);

    /*
     * here [node_start, node_end] will be overwrite,
     * so off_reserve need to move forward.
     * if TAIL was overwrite, then move TAIL too.
     */
    if (rb_tail == EV_RB_NODE(rb, rb->node.off_reserve))
    {
        rb->node.off_TAIL = EV_RB_BASIS_DIFF(rb, newer_node);
    }
    rb->node.off_reserve = EV_RB_BASIS_DIFF(rb, newer_node);

    /* generate new node */
    ring_buffer_node_t* new_node = (ring_buffer_node_t*)start_point;

    /* Update position chain */
    new_node->chain_pos.off_next = node_end->chain_pos.off_next;
    EV_RB_NODE(rb, new_node->chain_pos.off_next)->chain_pos.off_prev = EV_RB_BASIS_DIFF(rb, new_node);
    new_node->chain_pos.off_prev = node_start->chain_pos.off_prev;
    EV_RB_NODE(rb, new_node->chain_pos.off_prev)->chain_pos.off_next = EV_RB_BASIS_DIFF(rb, new_node);

    /* Update time chain */
    if (node_start->chain_time.off_older != 0)
    {
        EV_RB_NODE(rb, node_start->chain_time.off_older)->chain_time.off_newer =
            node_end->chain_time.off_newer;
    }
    if (node_end->chain_time.off_newer != 0)
    {
        EV_RB_NODE(rb, node_end->chain_time.off_newer)->chain_time.off_older =
            node_start->chain_time.off_older;
    }
    _ring_buffer_update_time_for_new_node(rb, new_node);

    /* Update counter */
    rb->counter.committed -= counter_lost_nodes;
    rb->counter.writing += 1;

    /* Update data length */
    new_node->token.size.size = data_len;

    /* Update node status */
    new_node->state = RINGBUFFER_STAT_WRITING;

    return &new_node->token;
}

/**
 * @brief Try to overwrite
 */
static ring_buffer_token_t* _ring_buffer_reserve_try_overwrite(
    ring_buffer_t* rb, size_t data_len, size_t node_size)
{
    ring_buffer_node_t* rb_oldest_reserve = EV_RB_NODE(rb, rb->node.off_reserve);
    /* Overwrite only works for committed nodes */
    if (rb->node.off_reserve == 0
        || rb_oldest_reserve->state != RINGBUFFER_STAT_COMMITTED)
    {
        return NULL;
    }

    /* Short cut: if only exists one node, check whether whole ring buffer can hold this data */
    if (rb_oldest_reserve->chain_pos.off_next == rb->node.off_reserve)
    {
        if (rb->config.capacity < node_size)
        {
            return NULL;
        }

        /* If we can, re-initialize and add this node */
        _ring_buffer_reinit(rb);
        return _ring_buffer_reserve_empty(rb, data_len, node_size);
    }

    /* Step 1. Calculate where overwrite start */
    const ring_buffer_node_t* backward_node = EV_RB_NODE(rb, rb_oldest_reserve->chain_pos.off_prev);
    uint8_t* start_point = (backward_node < rb_oldest_reserve) ?
        ((uint8_t*)backward_node + ring_buffer_node_cost(backward_node->token.size.size)) :
        (uint8_t*)EV_RB_BEG_NODE(rb);

    /* Step 2. Calculate whether continuous committed nodes could hold needed data */
    size_t sum_size = 0;
    size_t counter_lost_nodes = 1;
    ring_buffer_node_t* node_end = rb_oldest_reserve;

    while (1)
    {
        sum_size = (uint8_t*)node_end + ring_buffer_node_cost(node_end->token.size.size) - (uint8_t*)start_point;

        ring_buffer_node_t* forward_of_node_end = EV_RB_NODE(rb, node_end->chain_pos.off_next);
        if (!(sum_size < node_size /* overwrite minimum nodes */
            && forward_of_node_end->state == RINGBUFFER_STAT_COMMITTED /* only overwrite committed node */
            && node_end->chain_pos.off_next == node_end->chain_time.off_newer /* node must both physical and time continuous */
            && forward_of_node_end > node_end /* cannot interrupt by array boundary */
            ))
        {
            break;
        }
        node_end = EV_RB_NODE(rb, node_end->chain_pos.off_next);
        counter_lost_nodes++;
    }

    /* Step 3. check if condition allow to overwrite */
    if (sum_size < node_size)
    {
        return NULL;
    }

    /* Step 4. perform overwrite */
    return _ring_buffer_perform_overwrite(rb, start_point, rb_oldest_reserve,
        node_end, counter_lost_nodes, data_len);
}

inline static void _ring_buffer_insert_new_node(ring_buffer_t* rb,
    ring_buffer_node_t* new_node, size_t data_len)
{
    ring_buffer_node_t* rb_head = EV_RB_NODE(rb, rb->node.off_HEAD);

    /* initialize token */
    new_node->state = RINGBUFFER_STAT_WRITING;
    new_node->token.size.size = data_len;

    /* update chain_pos */
    new_node->chain_pos.off_next = rb_head->chain_pos.off_next;
    new_node->chain_pos.off_prev = rb->node.off_HEAD;
    EV_RB_NODE(rb, new_node->chain_pos.off_next)->chain_pos.off_prev = EV_RB_BASIS_DIFF(rb, new_node);
    EV_RB_NODE(rb, new_node->chain_pos.off_prev)->chain_pos.off_next = EV_RB_BASIS_DIFF(rb, new_node);

    _ring_buffer_update_time_for_new_node(rb, new_node);
}

/**
 * @brief Update off_reserve
 */
static void _ring_buffer_reserve_update_oldest_reserve(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    if (rb->node.off_reserve == 0)
    {
        rb->node.off_reserve = EV_RB_BASIS_DIFF(rb, node);
    }
}

static ring_buffer_token_t* _ring_buffer_reserve_none_empty(
    ring_buffer_t* rb, size_t data_len, size_t node_size, int flags)
{
    ring_buffer_node_t* rb_head = EV_RB_NODE(rb, rb->node.off_HEAD);

    /**
     * Get next possible node in right side of HEAD
     * If there is a node exists, the address of that node is larger or equal to calculated address.
     */
    size_t next_possible_pos = rb->node.off_HEAD + ring_buffer_node_cost(rb_head->token.size.size);
    ring_buffer_node_t* next_possible_node = EV_RB_NODE(rb, next_possible_pos);

    /* If have a existing node on right side of HEAD, we can use that space */
    if (rb_head->chain_pos.off_next > rb->node.off_HEAD)
    {
        /* If space is enough, generate token */
        if (rb_head->chain_pos.off_next - next_possible_pos >= node_size)
        {
            rb->counter.writing++;
            _ring_buffer_insert_new_node(rb, next_possible_node, data_len);
            _ring_buffer_reserve_update_oldest_reserve(rb, next_possible_node);
            return &next_possible_node->token;
        }

        /* Otherwise overwrite */
        return (flags & RINGBUFFER_FLAG_OVERWRITE) ?
            _ring_buffer_reserve_try_overwrite(rb, data_len, node_size) :
            NULL;
    }

    /* If no existing node on right side, try to create token */
    if ((rb->config.capacity - (next_possible_pos - EV_RB_BEG_POS(rb))) >= node_size)
    {
        rb->counter.writing++;
        _ring_buffer_insert_new_node(rb, next_possible_node, data_len);
        _ring_buffer_reserve_update_oldest_reserve(rb, next_possible_node);
        return &next_possible_node->token;
    }

    /* if area on the most left cache is enough, make token */
    if (rb_head->chain_pos.off_next - EV_RB_BEG_POS(rb) >= node_size)
    {
        next_possible_node = EV_RB_BEG_NODE(rb);
        rb->counter.writing++;
        _ring_buffer_insert_new_node(rb, next_possible_node, data_len);
        _ring_buffer_reserve_update_oldest_reserve(rb, next_possible_node);
        return &next_possible_node->token;
    }

    /* in other condition, overwrite if needed */
    return (flags & RINGBUFFER_FLAG_OVERWRITE) ?
        _ring_buffer_reserve_try_overwrite(rb, data_len, node_size) : NULL;
}

inline static int _ring_buffer_commit_for_write_confirm(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    (void)rb;

    /* Update counter */
    rb->counter.writing--;
    rb->counter.committed++;

    /* Update node status */
    node->state = RINGBUFFER_STAT_COMMITTED;

    return 0;
}

/**
 * @brief Update position chain and time chain after delete node.
 * @param node   The node to delete
 */
static void _ring_buffer_delete_node_update_chain(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    /* Update position chain */
    EV_RB_NODE(rb, node->chain_pos.off_prev)->chain_pos.off_next = node->chain_pos.off_next;
    EV_RB_NODE(rb, node->chain_pos.off_next)->chain_pos.off_prev = node->chain_pos.off_prev;

    /* Update time chain */
    if (node->chain_time.off_older != 0)
    {
        EV_RB_NODE(rb, node->chain_time.off_older)->chain_time.off_newer = node->chain_time.off_newer;
    }
    if (node->chain_time.off_newer != 0)
    {
        EV_RB_NODE(rb, node->chain_time.off_newer)->chain_time.off_older = node->chain_time.off_older;
    }
}

/**
 * @brief Completely remove a node from ring buffer
 * @param rb    ring buffer
 * @param node  node to be delete
 */
static void _ring_buffer_delete_node(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    /**
     * Short cut: If only one node in ring buffer, re-initialize.
     * Here use `p_forward` to check if it meets the condition:
     * 1. `chain_pos.off_next` always point to next node. If it point to self, there is only one node.
     * 2. Access to `node` means `node` is in CPU cache line. By access `p_forward` can avoid cache miss.
     */
    if (node->chain_pos.off_next == EV_RB_BASIS_DIFF(rb, node))
    {
        _ring_buffer_reinit(rb);
        return;
    }

    /* Delete node and update chains */
    _ring_buffer_delete_node_update_chain(rb, node);

    /* Update off_reserve */
    if (rb->node.off_reserve == EV_RB_BASIS_DIFF(rb, node))
    {
        rb->node.off_reserve = node->chain_time.off_newer;
    }

    /* Update TAIL if necessary */
    if (node->chain_time.off_older == 0)
    {
        rb->node.off_TAIL = node->chain_time.off_newer;
        return;
    }

     /* Update HEAD if necessary */
    if (node->chain_time.off_newer == 0)
    {
        rb->node.off_HEAD = node->chain_time.off_older;
        return;
    }

    return;
}

static int _ring_buffer_commit_for_write_discard(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    rb->counter.writing--;
    _ring_buffer_delete_node(rb, node);
    return 0;
}

static int _ring_buffer_commit_for_write(ring_buffer_t* rb,
    ring_buffer_node_t* node, int flags)
{
    return (flags & RINGBUFFER_FLAG_DISCARD) ?
        _ring_buffer_commit_for_write_discard(rb, node) :
        _ring_buffer_commit_for_write_confirm(rb, node);
}

static int _ring_buffer_commit_for_consume_confirm(ring_buffer_t* rb,
    ring_buffer_node_t* node)
{
    rb->counter.reading--;
    _ring_buffer_delete_node(rb, node);
    return 0;
}

/**
 * @brief Discard a consumed token.
 * the only condition a consumed token can be discard is no one consume newer token
 */
static int _ring_buffer_commit_for_consume_discard(ring_buffer_t* rb,
    ring_buffer_node_t* node, int flags)
{
    /* If existing a newer consumer, should fail. */
    if (node->chain_time.off_newer != 0
        && EV_RB_NODE(rb, node->chain_time.off_newer)->state == RINGBUFFER_STAT_READING)
    {
        return (flags & RINGBUFFER_FLAG_ABANDON) ?
            _ring_buffer_commit_for_consume_confirm(rb, node) : -1;
    }

    /* Update counter and status */
    rb->counter.reading--;
    rb->counter.committed++;
    node->state = RINGBUFFER_STAT_COMMITTED;

    /* if no newer node, then off_reserve should point to this node */
    if (node->chain_time.off_newer == 0)
    {
        rb->node.off_reserve = EV_RB_BASIS_DIFF(rb, node);
        return 0;
    }

    /* if node is just older than off_reserve, then off_reserve should move back */
    if (rb->node.off_reserve != 0
        && EV_RB_NODE(rb, rb->node.off_reserve)->chain_time.off_older == EV_RB_BASIS_DIFF(rb, node))
    {
        rb->node.off_reserve = EV_RB_BASIS_DIFF(rb, node);
        return 0;
    }

    return 0;
}

static int _ring_buffer_commit_for_consume(ring_buffer_t* rb,
    ring_buffer_node_t* node, int flags)
{
    return (flags & RINGBUFFER_FLAG_DISCARD) ?
        _ring_buffer_commit_for_consume_discard(rb, node, flags) :
        _ring_buffer_commit_for_consume_confirm(rb, node);
}

size_t ring_buffer_heap_cost(void)
{
    /* need to align with machine size */
    return ALIGN_WITH(sizeof(struct ring_buffer), sizeof(void*));
}

size_t ring_buffer_node_cost(size_t size)
{
    return ALIGN_WITH(sizeof(ring_buffer_node_t) + size, sizeof(void*));
}

ring_buffer_t* ring_buffer_init(void* buffer, size_t size)
{
    /* Calculate start address */
    ring_buffer_t* handler = buffer;

    /* Check space size */
    if (ring_buffer_heap_cost() + ring_buffer_node_cost(0) >= size)
    {
        return NULL;
    }

    /* setup necessary field */
    handler->config.capacity = size - ring_buffer_heap_cost();
    handler->basis = NULL;

    /* initialize */
    _ring_buffer_reinit(handler);

    return handler;
}

ring_buffer_token_t* ring_buffer_reserve(ring_buffer_t* handler, size_t len,
    int flags)
{
    /* node must aligned */
    const size_t node_size = ring_buffer_node_cost(len);

    /* empty ring buffer */
    if (handler->node.off_TAIL == 0)
    {
        return _ring_buffer_reserve_empty(handler, len, node_size);
    }

    /* non empty ring buffer */
    return _ring_buffer_reserve_none_empty(handler, len, node_size, flags);
}

ring_buffer_token_t* ring_buffer_consume(ring_buffer_t* handler)
{
    ring_buffer_node_t* rb_oldest_reserve = EV_RB_NODE(handler, handler->node.off_reserve);
    if (handler->node.off_reserve == 0
        || rb_oldest_reserve->state != RINGBUFFER_STAT_COMMITTED)
    {
        return NULL;
    }

    handler->counter.committed--;
    handler->counter.reading++;

    ring_buffer_node_t* token_node = rb_oldest_reserve;
    handler->node.off_reserve = rb_oldest_reserve->chain_time.off_newer;
    token_node->state = RINGBUFFER_STAT_READING;

    return &token_node->token;
}

int ring_buffer_commit(ring_buffer_t* handler, ring_buffer_token_t* token, int flags)
{
    ring_buffer_node_t* node = container_of(token, ring_buffer_node_t, token);

    return node->state == RINGBUFFER_STAT_WRITING ?
        _ring_buffer_commit_for_write(handler, node, flags) :
        _ring_buffer_commit_for_consume(handler, node, flags);
}

size_t ring_buffer_count(ring_buffer_t* handler, ring_buffer_counter_t* counter)
{
    if (counter != NULL)
    {
        *counter = handler->counter;
    }

    return handler->counter.committed + handler->counter.reading + handler->counter.writing;
}

ring_buffer_token_t* ring_buffer_begin(const ring_buffer_t* handler)
{
    ring_buffer_node_t* rb_tail = EV_RB_NODE(handler, handler->node.off_TAIL);
    return &(rb_tail->token);
}

ring_buffer_token_t* ring_buffer_next(const ring_buffer_t* handler, const ring_buffer_token_t* token)
{
    ring_buffer_node_t* node = container_of(token, ring_buffer_node_t, token);
    if (node->chain_time.off_newer == 0)
    {
        return NULL;
    }

    node = EV_RB_NODE(handler, node->chain_time.off_newer);
    return &(node->token);
}
