#pragma once

#include "avl.h"
#include "hashtable.h"


struct ZSet {
    AVLNode *root = NULL;   // index by (score, name) Keeps all your data perfectly sorted. It sorts primarily by score. If two items have the same score, it sorts them alphabetically by name. This is what allows you to grab the "top 10" easily
    HMap hmap;              // index by name, ignores the score completely and just indexes the items by their name. This allows you to instantly look up an item without scanning the tree.
};
/*Instead of the tree or hash map holding pointers to the data, the data holds the tree and hash map nodes inside itself. Because ZNode contains both an AVLNode and an HNode, a single ZNode can be physically wired into both the AVL Tree and the Hash Table simultaneously.*/
struct ZNode {
    AVLNode tree;
    HNode   hmap;
    double  score = 0;
    size_t  len = 0;
    char    name[0];        // flexible array ,This is a classic C memory trick. By declaring an array of size 0 at the very end of the struct, it acts as a placeholder. When you allocate memory for a ZNode, you will ask the computer for sizeof(ZNode) + name_length. This lets you store the string directly adjacent to the struct in memory, avoiding the need for an extra pointer and an extra malloc call!
};

bool   zset_insert(ZSet *zset, const char *name, size_t len, double score);  //When a user adds a new key-score pair, this function will create a ZNode and insert it into both the hmap and the root tree.
ZNode *zset_lookup(ZSet *zset, const char *name, size_t len);  //This skips the tree entirely and asks the hmap to find the ZNode by its name. It's lightning fast ($O(1)$).
void   zset_delete(ZSet *zset, ZNode *node);  //Finds the ZNode, detaches it from the AVL tree, detaches it from the hash table, and then finally frees the memory.
ZNode *zset_seekge(ZSet *zset, double score, const char *name, size_t len);  //Seek Greater or Equal". This uses the AVL tree to find the very first node whose score is $\ge$ the requested score. This is the starting point for commands like ZRANGEBYSCORE.
void   zset_clear(ZSet *zset);   //Safely destroys both data structures to prevent memory leaks.
ZNode *znode_offset(ZNode *node, int64_t offset);  //his is the wrapper for that avl_offset function,It takes a ZNode, reaches inside it to grab the AVLNode, passes it to avl_offset to jump through the tree mathematically, and then returns the new ZNode.