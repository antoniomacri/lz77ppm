/*
 * This file is part of lz77ppm, a simple implementation of the LZ77
 * compression algorithm.
 *
 * This is free and unencumbered software released into the public domain.
 * For more information, see the included UNLICENSE file.
 */

/**
 * @file tree.h
 *
 * Construction and management of a binary search tree built upon the sliding
 * window.
 */

#ifndef _LZ77_TREE_H_
#define _LZ77_TREE_H_

#include <stdint.h>

// Forward-declare lz77_tree since it is used by ustream.h.
struct _lz77_tree;
typedef struct _lz77_tree lz77_tree;

#include <lz77ppm/ustream.h>

/**
 * Basic data structure used to construct the binary search tree on top of the
 * sliding window.
 */
struct _lz77_tree {
    /** The index of the parent node (or <tt>(uint16_t)-1</tt> for the root). */
    uint16_t parent;
    /** The index of the child node starting the left subtree, or <tt>
     *  (uint16_t)-1</tt> if unused. */
    uint16_t smaller;
    /** The index of the child node starting the right subtree, or <tt>
     *  (uint16_t)-1</tt> if unused. */
    uint16_t larger;
};

// Just for convenience.
static const uint16_t UNUSED = -1;

/**
 * Initializes the binary search tree. It just sets all indices of the root
 * node to 'unused'.
 */
void lz77_tree_init(lz77_ustream *ustream);

/**
 * Finds a match in the tree.
 */
uint16_t lz77_find_and_add(lz77_ustream *ustream, int index, uint16_t *offset);

/**
 * Removes a node, replacing it with one of its children.
 *
 * @param old The node to be removed.
 * @param new The node that will replace the removed one. It can be an 'unused'
 *        node.
 *
 * The @c new node must be one of the children of @c old. Its other child must
 * be unused.
 */
void lz77_tree_contract_node(lz77_tree *tree, int old, int new);

/**
 * Replaces a node.
 *
 * @param old The node to be replaced.
 * @param new The node that will replace the removed one.
 */
void lz77_tree_replace_node(lz77_tree *tree, int old, int new);

/**
 * Deletes a node.
 *
 * @param index The node to be deleted.
 */
void lz77_tree_delete_node(lz77_tree *tree, int index);

#endif
