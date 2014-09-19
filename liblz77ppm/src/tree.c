/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

#include <assert.h>
#include <stdlib.h>

#include <tree.h>
#include <ustream_internal.h>

void lz77_tree_init(lz77_ustream *ustream)
{
    lz77_tree *root = &ustream->tree[ustream->window_maxsize];
    root->parent = UNUSED;
    root->smaller = UNUSED;
    root->larger = UNUSED;
}

uint16_t lz77_find_and_add(lz77_ustream *ustream, int curr, uint16_t *offset)
{
    assert(ustream != NULL);
    assert(0 <= curr && curr < ustream->window_maxsize);
    assert(offset != NULL);

    // Start searching from the right child of the root.
    int test = ustream->tree[ustream->window_maxsize].larger;

    // The position inside the tree array which corresponds to the beginning
    // of the window.
    int begin = (ustream->window - ustream->cdata) % ustream->window_maxsize;

    uint16_t longest = 0;
    while (1) {
        int k = test - begin;
        if (k < 0) {
            k += ustream->window_maxsize;
        }

        uint16_t i = 0;
        int delta = 0;
        while (i < ustream->lookahead_currsize) {
            delta = ustream->lookahead[i] - ustream->window[k + i];
            if (delta != 0)
                break;
            i++;
        }

        if (i > longest) {
            *offset = k;
            longest = i;
            if (longest == ustream->lookahead_currsize) {
                // We found a match for the whole look-ahead buffer. Since
                // duplicated nodes in the tree are not permitted, we just
                // replace the old node (test) with the new one (curr).
                if (test != curr) {
                    lz77_tree_delete_node(ustream->tree, curr);
                    lz77_tree_replace_node(ustream->tree, test, curr);
                }
                break;
            }
        }
        assert(delta != 0);

        uint16_t *child;
        if (delta > 0) {
            child = &ustream->tree[test].larger;
        }
        else {
            child = &ustream->tree[test].smaller;
        }
        if (*child == UNUSED) {
            // We reached the end of our path in the tree. Add the new node
            // (curr) as a child of the test node.
            if (test == curr) {
                // lz77_tree_replace_node(ustream->tree, test, curr);
                break;
            }
            else {
                if (ustream->tree[curr].parent != UNUSED) {
                    lz77_tree_delete_node(ustream->tree, curr);
                }
                if (*child == UNUSED) {
                    // Function lz77_tree_delete_node() may have changed the
                    // node we had selected (*child). If this is the case, just
                    // continue navigating the tree, otherwise set the node and
                    // break.
                    *child = curr;
                    ustream->tree[curr].parent = test;
                    ustream->tree[curr].larger = UNUSED;
                    ustream->tree[curr].smaller = UNUSED;
                    break;
                }
            }
        }
        test = *child;
    }
    return longest;
}

void lz77_tree_contract_node(lz77_tree *tree, int old, int new)
{
    assert(old != UNUSED);
    assert(new != old);

    int parent = tree[old].parent;
    assert(tree[parent].larger == old || tree[parent].smaller == old);
    assert(tree[old].larger == new || tree[old].smaller == new);
    assert(tree[old].larger == UNUSED || tree[old].smaller == UNUSED);
    assert(new == UNUSED || tree[new].parent == old);

    if (new != UNUSED) {
        tree[new].parent = parent;
    }
    if (tree[parent].larger == old) {
        tree[parent].larger = new;
    }
    else {
        tree[parent].smaller = new;
    }
    tree[old].parent = UNUSED;
}

void lz77_tree_replace_node(lz77_tree *tree, int old, int new)
{
    assert(old != UNUSED);
    assert(new != UNUSED);
    assert(new != old);
    assert(tree[new].parent == UNUSED);

    int parent = tree[old].parent;
    if (parent == UNUSED) {
    } else if (tree[parent].smaller == old) {
        tree[parent].smaller = new;
    } else {
        tree[parent].larger = new;
    }
    tree[new] = tree[old];
    lz77_tree *new_node = &tree[new];
    if (new_node->smaller != UNUSED) {
        tree[new_node->smaller].parent = new;
    }
    if (new_node->larger != UNUSED) {
        tree[new_node->larger].parent = new;
    }
    tree[old].parent = UNUSED;
}

static int lz77_tree_find_next_node(lz77_tree *tree, int index)
{
    assert(index != UNUSED);

    int next = tree[index].smaller;
    while (tree[next].larger != UNUSED)
        next = tree[next].larger;
    return next;
}

void lz77_tree_delete_node(lz77_tree *tree, int index)
{
    assert(index != UNUSED);

    if (tree[index].parent == UNUSED) {
        return;
    }
    if (tree[index].smaller != UNUSED && tree[index].larger != UNUSED) {
        int replacement = lz77_tree_find_next_node(tree, index);
        lz77_tree_delete_node(tree, replacement);
        lz77_tree_replace_node(tree, index, replacement);
    }
    else if (tree[index].smaller != UNUSED) {
        lz77_tree_contract_node(tree, index, tree[index].smaller);
    }
    else {
        lz77_tree_contract_node(tree, index, tree[index].larger);
    }
}
