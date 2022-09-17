#include "map.h"
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>

/*
* red-black trees properties:  http://en.wikipedia.org/wiki/Rbtree
*
*  1) A node is either red or black
*  2) The root is black
*  3) All leaves (NULL) are black
*  4) Both children of every red node are black
*  5) Every simple path from root to leaves contains the same number
*     of black nodes.
*
*  4 and 5 give the O(log n) guarantee, since 4 implies you cannot have two
*  consecutive red nodes in a path and every red node is therefore followed by
*  a black. So if B is the number of black nodes on every simple path (as per
*  5), then the longest possible path due to 4 is 2B.
*
*  We shall indicate color with case, where black nodes are uppercase and red
*  nodes will be lowercase. Unknown color nodes shall be drawn as red within
*  parentheses and have some accompanying text comment.
*/

#define RB_RED      0
#define RB_BLACK    1

#define __rb_color(pc)     ((uintptr_t)(pc) & 1)
#define __rb_is_black(pc)  __rb_color(pc)
#define __rb_is_red(pc)    (!__rb_color(pc))
#define __rb_parent(pc)    ((atd_map_node_t*)(pc & ~3))
#define rb_color(rb)       __rb_color((rb)->__rb_parent_color)
#define rb_is_red(rb)      __rb_is_red((rb)->__rb_parent_color)
#define rb_is_black(rb)    __rb_is_black((rb)->__rb_parent_color)
#define rb_parent(r)   ((atd_map_node_t*)((uintptr_t)((r)->__rb_parent_color) & ~3))

/* 'empty' nodes are nodes that are known not to be inserted in an rbtree */
#define RB_EMPTY_NODE(node)  \
    ((node)->__rb_parent_color == (struct atd_map_node*)(node))

static void rb_set_black(atd_map_node_t* rb)
{
    rb->__rb_parent_color =
        (atd_map_node_t*)((uintptr_t)(rb->__rb_parent_color) | RB_BLACK);
}

static atd_map_node_t* rb_red_parent(atd_map_node_t* red)
{
    return (atd_map_node_t*)red->__rb_parent_color;
}

static void rb_set_parent_color(atd_map_node_t* rb, atd_map_node_t* p, int color)
{
    rb->__rb_parent_color = (struct atd_map_node*)((uintptr_t)p | color);
}

static void __rb_change_child(atd_map_node_t* old_node, atd_map_node_t* new_node,
                              atd_map_node_t* parent, atd_map_t* root)
{
    if (parent)
    {
        if (parent->rb_left == old_node)
        {
            parent->rb_left = new_node;
        }
        else
        {
            parent->rb_right = new_node;
        }
    }
    else
    {
        root->rb_root = new_node;
    }
}

/*
* Helper function for rotations:
* - old's parent and color get assigned to new
* - old gets assigned new as a parent and 'color' as a color.
*/
static void __rb_rotate_set_parents(atd_map_node_t* old, atd_map_node_t* new_node,
    atd_map_t* root, int color)
{
    atd_map_node_t* parent = rb_parent(old);
    new_node->__rb_parent_color = old->__rb_parent_color;
    rb_set_parent_color(old, new_node, color);
    __rb_change_child(old, new_node, parent, root);
}

static void __rb_insert(atd_map_node_t* node, atd_map_t* root)
{
    atd_map_node_t* parent = rb_red_parent(node), * gparent, * tmp;

    for (;;) {
        /*
        * Loop invariant: node is red
        *
        * If there is a black parent, we are done.
        * Otherwise, take some corrective action as we don't
        * want a red root or two consecutive red nodes.
        */
        if (!parent) {
            rb_set_parent_color(node, NULL, RB_BLACK);
            break;
        }
        else if (rb_is_black(parent))
            break;

        gparent = rb_red_parent(parent);

        tmp = gparent->rb_right;
        if (parent != tmp) {    /* parent == gparent->rb_left */
            if (tmp && rb_is_red(tmp)) {
                /*
                * Case 1 - color flips
                *
                *       G            g
                *      / \          / \
                *     p   u  -->   P   U
                *    /            /
                *   n            n
                *
                * However, since g's parent might be red, and
                * 4) does not allow this, we need to recurse
                * at g.
                */
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_right;
            if (node == tmp) {
                /*
                * Case 2 - left rotate at parent
                *
                *      G             G
                *     / \           / \
                *    p   U  -->    n   U
                *     \           /
                *      n         p
                *
                * This still leaves us in violation of 4), the
                * continuation into Case 3 will fix that.
                */
                parent->rb_right = tmp = node->rb_left;
                node->rb_left = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent,
                        RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->rb_right;
            }

            /*
            * Case 3 - right rotate at gparent
            *
            *        G           P
            *       / \         / \
            *      p   U  -->  n   g
            *     /                 \
            *    n                   U
            */
            gparent->rb_left = tmp;  /* == parent->rb_right */
            parent->rb_right = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            break;
        }
        else {
            tmp = gparent->rb_left;
            if (tmp && rb_is_red(tmp)) {
                /* Case 1 - color flips */
                rb_set_parent_color(tmp, gparent, RB_BLACK);
                rb_set_parent_color(parent, gparent, RB_BLACK);
                node = gparent;
                parent = rb_parent(node);
                rb_set_parent_color(node, parent, RB_RED);
                continue;
            }

            tmp = parent->rb_left;
            if (node == tmp) {
                /* Case 2 - right rotate at parent */
                parent->rb_left = tmp = node->rb_right;
                node->rb_right = parent;
                if (tmp)
                    rb_set_parent_color(tmp, parent,
                        RB_BLACK);
                rb_set_parent_color(parent, node, RB_RED);
                parent = node;
                tmp = node->rb_left;
            }

            /* Case 3 - left rotate at gparent */
            gparent->rb_right = tmp;  /* == parent->rb_left */
            parent->rb_left = gparent;
            if (tmp)
                rb_set_parent_color(tmp, gparent, RB_BLACK);
            __rb_rotate_set_parents(gparent, parent, root, RB_RED);
            break;
        }
    }
}

static void rb_set_parent(atd_map_node_t* rb, atd_map_node_t* p)
{
    rb->__rb_parent_color = (struct atd_map_node*)(rb_color(rb) | (uintptr_t)p);
}

static atd_map_node_t* __rb_erase_augmented(atd_map_node_t* node, atd_map_t* root)
{
    atd_map_node_t* child = node->rb_right, * tmp = node->rb_left;
    atd_map_node_t* parent, * rebalance;
    uintptr_t pc;

    if (!tmp) {
        /*
        * Case 1: node to erase has no more than 1 child (easy!)
        *
        * Note that if there is one child it must be red due to 5)
        * and node must be black due to 4). We adjust colors locally
        * so as to bypass __rb_erase_color() later on.
        */
        pc = (uintptr_t)(node->__rb_parent_color);
        parent = __rb_parent(pc);
        __rb_change_child(node, child, parent, root);
        if (child) {
            child->__rb_parent_color = (struct atd_map_node*)pc;
            rebalance = NULL;
        }
        else
            rebalance = __rb_is_black(pc) ? parent : NULL;
        tmp = parent;
    }
    else if (!child) {
        /* Still case 1, but this time the child is node->rb_left */
        pc = (uintptr_t)(node->__rb_parent_color);
        tmp->__rb_parent_color = (atd_map_node_t*)pc;
        parent = __rb_parent(pc);
        __rb_change_child(node, tmp, parent, root);
        rebalance = NULL;
        tmp = parent;
    }
    else {
        atd_map_node_t* successor = child, * child2;
        tmp = child->rb_left;
        if (!tmp) {
            /*
            * Case 2: node's successor is its right child
            *
            *    (n)          (s)
            *    / \          / \
            *  (x) (s)  ->  (x) (c)
            *        \
            *        (c)
            */
            parent = successor;
            child2 = successor->rb_right;
        }
        else {
            /*
            * Case 3: node's successor is leftmost under
            * node's right child subtree
            *
            *    (n)          (s)
            *    / \          / \
            *  (x) (y)  ->  (x) (y)
            *      /            /
            *    (p)          (p)
            *    /            /
            *  (s)          (c)
            *    \
            *    (c)
            */
            do {
                parent = successor;
                successor = tmp;
                tmp = tmp->rb_left;
            } while (tmp);
            parent->rb_left = child2 = successor->rb_right;
            successor->rb_right = child;
            rb_set_parent(child, successor);
        }

        successor->rb_left = tmp = node->rb_left;
        rb_set_parent(tmp, successor);

        pc = (uintptr_t)(node->__rb_parent_color);
        tmp = __rb_parent(pc);
        __rb_change_child(node, successor, tmp, root);
        if (child2) {
            successor->__rb_parent_color = (struct atd_map_node*)pc;
            rb_set_parent_color(child2, parent, RB_BLACK);
            rebalance = NULL;
        }
        else {
            uintptr_t pc2 = (uintptr_t)(successor->__rb_parent_color);
            successor->__rb_parent_color = (struct atd_map_node*)pc;
            rebalance = __rb_is_black(pc2) ? parent : NULL;
        }
        tmp = successor;
    }

    return rebalance;
}

/*
* Inline version for rb_erase() use - we want to be able to inline
* and eliminate the dummy_rotate callback there
*/
static void ____rb_erase_color(atd_map_node_t* parent, atd_map_t* root)
{
    atd_map_node_t* node = NULL, * sibling, * tmp1, * tmp2;

    for (;;) {
        /*
        * Loop invariants:
        * - node is black (or NULL on first iteration)
        * - node is not the root (parent is not NULL)
        * - All leaf paths going through parent and node have a
        *   black node count that is 1 lower than other leaf paths.
        */
        sibling = parent->rb_right;
        if (node != sibling) {  /* node == parent->rb_left */
            if (rb_is_red(sibling)) {
                /*
                * Case 1 - left rotate at parent
                *
                *     P               S
                *    / \             / \
                *   N   s    -->    p   Sr
                *      / \         / \
                *     Sl  Sr      N   Sl
                */
                parent->rb_right = tmp1 = sibling->rb_left;
                sibling->rb_left = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                __rb_rotate_set_parents(parent, sibling, root,
                    RB_RED);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_right;
            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_left;
                if (!tmp2 || rb_is_black(tmp2)) {
                    /*
                    * Case 2 - sibling color flip
                    * (p could be either color here)
                    *
                    *    (p)           (p)
                    *    / \           / \
                    *   N   S    -->  N   s
                    *      / \           / \
                    *     Sl  Sr        Sl  Sr
                    *
                    * This leaves us violating 5) which
                    * can be fixed by flipping p to black
                    * if it was red, or by recursing at p.
                    * p is red when coming from Case 1.
                    */
                    rb_set_parent_color(sibling, parent,
                        RB_RED);
                    if (rb_is_red(parent))
                        rb_set_black(parent);
                    else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                /*
                * Case 3 - right rotate at sibling
                * (p could be either color here)
                *
                *   (p)           (p)
                *   / \           / \
                *  N   S    -->  N   Sl
                *     / \             \
                *    sl  Sr            s
                *                       \
                *                        Sr
                */
                sibling->rb_left = tmp1 = tmp2->rb_right;
                tmp2->rb_right = sibling;
                parent->rb_right = tmp2;
                if (tmp1)
                    rb_set_parent_color(tmp1, sibling,
                        RB_BLACK);
                tmp1 = sibling;
                sibling = tmp2;
            }
            /*
            * Case 4 - left rotate at parent + color flips
            * (p and sl could be either color here.
            *  After rotation, p becomes black, s acquires
            *  p's color, and sl keeps its color)
            *
            *      (p)             (s)
            *      / \             / \
            *     N   S     -->   P   Sr
            *        / \         / \
            *      (sl) sr      N  (sl)
            */
            parent->rb_right = tmp2 = sibling->rb_left;
            sibling->rb_left = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2)
                rb_set_parent(tmp2, parent);
            __rb_rotate_set_parents(parent, sibling, root,
                RB_BLACK);
            break;
        }
        else {
            sibling = parent->rb_left;
            if (rb_is_red(sibling)) {
                /* Case 1 - right rotate at parent */
                parent->rb_left = tmp1 = sibling->rb_right;
                sibling->rb_right = parent;
                rb_set_parent_color(tmp1, parent, RB_BLACK);
                __rb_rotate_set_parents(parent, sibling, root,
                    RB_RED);
                sibling = tmp1;
            }
            tmp1 = sibling->rb_left;
            if (!tmp1 || rb_is_black(tmp1)) {
                tmp2 = sibling->rb_right;
                if (!tmp2 || rb_is_black(tmp2)) {
                    /* Case 2 - sibling color flip */
                    rb_set_parent_color(sibling, parent,
                        RB_RED);
                    if (rb_is_red(parent))
                        rb_set_black(parent);
                    else {
                        node = parent;
                        parent = rb_parent(node);
                        if (parent)
                            continue;
                    }
                    break;
                }
                /* Case 3 - right rotate at sibling */
                sibling->rb_right = tmp1 = tmp2->rb_left;
                tmp2->rb_left = sibling;
                parent->rb_left = tmp2;
                if (tmp1)
                    rb_set_parent_color(tmp1, sibling,
                        RB_BLACK);
                tmp1 = sibling;
                sibling = tmp2;
            }
            /* Case 4 - left rotate at parent + color flips */
            parent->rb_left = tmp2 = sibling->rb_right;
            sibling->rb_right = parent;
            rb_set_parent_color(tmp1, sibling, RB_BLACK);
            if (tmp2)
                rb_set_parent(tmp2, parent);
            __rb_rotate_set_parents(parent, sibling, root,
                RB_BLACK);
            break;
        }
    }
}

void ev_map_low_link_node(atd_map_node_t* node,
                          atd_map_node_t* parent, atd_map_node_t** rb_link)
{
    node->__rb_parent_color = parent;
    node->rb_left = node->rb_right = NULL;

    *rb_link = node;
    return;
}

void ev_map_low_insert_color(atd_map_node_t* node, atd_map_t* root)
{
    __rb_insert(node, root);
}

void ev_map_low_erase(atd_map_t* root, atd_map_node_t* node)
{
    atd_map_node_t* rebalance;
    rebalance = __rb_erase_augmented(node, root);
    if (rebalance)
    {
        ____rb_erase_color(rebalance, root);
    }
    node->rb_left = NULL;
    node->rb_right = NULL;
    node->__rb_parent_color = NULL;
}

/*
* This function returns the first node (in sort order) of the tree.
*/
atd_map_node_t* ev_map_low_first(const atd_map_t* root)
{
    atd_map_node_t* n = root->rb_root;

    if (!n)
        return NULL;
    while (n->rb_left)
        n = n->rb_left;
    return n;
}

atd_map_node_t* ev_map_low_last(const atd_map_t* root)
{
    atd_map_node_t* n = root->rb_root;

    if (!n)
        return NULL;
    while (n->rb_right)
        n = n->rb_right;
    return n;
}

atd_map_node_t* ev_map_low_next(const atd_map_node_t* node)
{
    atd_map_node_t* parent;

    if (RB_EMPTY_NODE(node))
        return NULL;

    /*
    * If we have a right-hand child, go down and then left as far
    * as we can.
    */
    if (node->rb_right) {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;
        return (atd_map_node_t*)node;
    }

    /*
    * No right-hand children. Everything down and left is smaller than us,
    * so any 'next' node must be in the general direction of our parent.
    * Go up the tree; any time the ancestor is a right-hand child of its
    * parent, keep going up. First time it's a left-hand child of its
    * parent, said parent is our 'next' node.
    */
    while ((parent = rb_parent(node)) != NULL && node == parent->rb_right)
        node = parent;

    return parent;
}

atd_map_node_t* ev_map_low_prev(const atd_map_node_t* node)
{
    atd_map_node_t* parent;

    if (RB_EMPTY_NODE(node))
        return NULL;

    /*
    * If we have a left-hand child, go down and then right as far
    * as we can.
    */
    if (node->rb_left) {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;
        return (atd_map_node_t*)node;
    }

    /*
    * No left-hand children. Go up till we find an ancestor which
    * is a right-hand child of its parent.
    */
    while ((parent = rb_parent(node)) != NULL && node == parent->rb_left)
        node = parent;

    return parent;
}

void ev_map_init(atd_map_t* handler, atd_map_cmp_fn cmp, void* arg)
{
    handler->rb_root = NULL;
    handler->cmp.cmp = cmp;
    handler->cmp.arg = arg;
    handler->size = 0;
}

atd_map_node_t* ev_map_insert(atd_map_t* handler, atd_map_node_t* node)
{
    atd_map_node_t **new_node = &(handler->rb_root), *parent = NULL;

    /* Figure out where to put new node */
    while (*new_node)
    {
        int result = handler->cmp.cmp(node, *new_node, handler->cmp.arg);

        parent = *new_node;
        if (result < 0)
        {
            new_node = &((*new_node)->rb_left);
        }
        else if (result > 0)
        {
            new_node = &((*new_node)->rb_right);
        }
        else
        {
            return *new_node;
        }
    }

    handler->size++;
    ev_map_low_link_node(node, parent, new_node);
    ev_map_low_insert_color(node, handler);

    return 0;
}

atd_map_node_t* ev_map_replace(atd_map_t* handler, atd_map_node_t* node)
{
    atd_map_node_t** new_node = &(handler->rb_root), * parent = NULL;
    handler->size++;

    /* Figure out where to put new node */
    while (*new_node)
    {
        int result = handler->cmp.cmp(node, *new_node, handler->cmp.arg);

        parent = *new_node;
        if (result < 0)
        {
            new_node = &((*new_node)->rb_left);
        }
        else if (result > 0)
        {
            new_node = &((*new_node)->rb_right);
        }
        else
        {/* replace new_node with node */
            node->rb_left = (*new_node)->rb_left;
            node->rb_left->__rb_parent_color = (atd_map_node_t*)((uintptr_t)node | rb_color(node->rb_left));

            node->rb_right = (*new_node)->rb_right;
            node->rb_right->__rb_parent_color = (atd_map_node_t*)((uintptr_t)node | rb_color(node->rb_right));

            node->__rb_parent_color = (*new_node)->__rb_parent_color;
            if (rb_parent(node)->rb_left == *new_node)
            {
                rb_parent(node)->rb_left = node;
            }
            else
            {
                rb_parent(node)->rb_right = node;
            }

            (*new_node)->rb_left = NULL;
            (*new_node)->rb_right = NULL;
            (*new_node)->__rb_parent_color = 0;

            return *new_node;
        }
    }

    ev_map_low_link_node(node, parent, new_node);
    ev_map_low_insert_color(node, handler);

    return NULL;
}

void ev_map_erase(atd_map_t* handler, atd_map_node_t* node)
{
    handler->size--;
    ev_map_low_erase(handler, node);
}

size_t ev_map_size(const atd_map_t* handler)
{
    return handler->size;
}

atd_map_node_t* ev_map_find(const atd_map_t* handler, const atd_map_node_t* key)
{
    atd_map_node_t* node = handler->rb_root;

    while (node)
    {
        int result = handler->cmp.cmp(key, node, handler->cmp.arg);

        if (result < 0)
        {
            node = node->rb_left;
        }
        else if (result > 0)
        {
            node = node->rb_right;
        }
        else
        {
            return node;
        }
    }

    return NULL;
}

atd_map_node_t* ev_map_find_lower(const atd_map_t* handler, const atd_map_node_t* key)
{
    atd_map_node_t* lower_node = NULL;
    atd_map_node_t* node = handler->rb_root;
    while (node)
    {
        int result = handler->cmp.cmp(key, node, handler->cmp.arg);
        if (result < 0)
        {
            node = node->rb_left;
        }
        else if (result > 0)
        {
            lower_node = node;
            node = node->rb_right;
        }
        else
        {
            return node;
        }
    }

    return lower_node;
}

atd_map_node_t* ev_map_find_upper(const atd_map_t* handler, const atd_map_node_t* key)
{
    atd_map_node_t* upper_node = NULL;
    atd_map_node_t* node = handler->rb_root;

    while (node)
    {
        int result = handler->cmp.cmp(key, node, handler->cmp.arg);

        if (result < 0)
        {
            upper_node = node;
            node = node->rb_left;
        }
        else if (result > 0)
        {
            node = node->rb_right;
        }
        else
        {
            if (upper_node == NULL)
            {
                upper_node = node->rb_right;
            }
            break;
        }
    }

    return upper_node;
}

atd_map_node_t* ev_map_begin(const atd_map_t* handler)
{
    return ev_map_low_first(handler);
}

atd_map_node_t* ev_map_end(const atd_map_t* handler)
{
    return ev_map_low_last(handler);
}

atd_map_node_t* ev_map_next(const atd_map_node_t* node)
{
    return ev_map_low_next(node);
}

atd_map_node_t* ev_map_prev(const atd_map_node_t* node)
{
    return ev_map_low_prev(node);
}
