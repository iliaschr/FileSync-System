/**
 * @file hashmap.c
 * @brief Implementation of a hashmap for storing directory synchronization information
 *
 * This file implements a hash table with separate chaining for collision resolution.
 * The hashmap is used to efficiently store and retrieve sync_info_t structures
 * based on source directory paths as keys. It provides the core data storage
 * mechanism for the File Synchronization System.
 * 
 * For this implementation I use the "Algoritms in C" book by Sedgewick.
 */

 #include "../include/hashmap.h"

 /* Static variables for the hashmap implementation */
 static int N, M;         /**< N = current number of items, M = number of buckets */
 static link_h *heads, z; /**< Array of bucket heads and sentinel node */
 
 /**
  * @brief Create a new node for the hashmap's linked lists
  *
  * Allocates and initializes a new node to store an item in the hashmap.
  *
  * @param item The sync_info_t item to store
  * @param next Pointer to the next node in the linked list
  * @return Pointer to the newly created node
  */
 link_h newNode(Item item, link_h next) {
     link_h x = malloc(sizeof(*x));
     x->item = item;
     x->next = next;
     return x;
 }
 
 /**
  * @brief Recursive search for an item in a linked list
  *
  * Traverses a linked list recursively to find an item with the given key.
  *
  * @param t Current node in the linked list
  * @param v Key to search for (source directory path)
  * @return The item if found, NULL otherwise
  */
 static Item searchR(link_h t, Key v) {
     if (t == z) return NULLitem;  // Reached end of chain without finding
     if (strcmp(v, t->item->source_dir) == 0) return t->item;  // Found
     return searchR(t->next, v);  // Recurse to next node
 }
 
 /* Macro for creating new nodes */
 #define NEW(item, next) newNode(item, next)
 
 /**
  * @brief Hash function for string keys
  *
  * Computes a hash value for a string key using the horner method.
  * This converts a variable-length string key into an integer
  * in the range [0, M-1] for use as an index in the hash table.
  *
  * @param v String key to hash (source directory path)
  * @param M Number of buckets in the hashmap
  * @return Hash value in the range [0, M-1]
  */
 int hash(char* v, int M) {
     int h = 0, a = 127;  /* a is a prime multiplier */
     for(; *v != '\0'; v++) h = (a*h + *v) % M;
     return h;
 }
 
 /**
  * @brief Initialize the hashmap
  *
  * Allocates and initializes a new hashmap with the specified capacity.
  * The actual number of buckets is max/5 to balance space usage and
  * collision avoidance.
  *
  * @param max Maximum expected number of items
  */
 void hashInit(int max) {
     int i;
     N = 0;              /* No items yet */
     M = max / 5;        /* Number of buckets */
     heads = malloc(M * sizeof(link_h));  /* Allocate bucket array */
     z = NEW(NULLitem, NULL);            /* Create sentinel node */
     
     /* Initialize all buckets to the sentinel */
     for (i = 0; i < M; i++) heads[i] = z;
 }
 
 /**
  * @brief Search for an item by key
  *
  * Computes the hash value for the key, then searches the
  * appropriate linked list for an item with a matching key.
  *
  * @param v Key to search for (source directory path)
  * @return The item if found, NULL otherwise
  */
 Item hashSearch(Key v) { 
     return searchR(heads[hash(v, M)], v); 
 }
 
 /**
  * @brief Insert an item into the hashmap
  *
  * Computes the hash value for the item's key, then inserts
  * the item at the front of the appropriate linked list.
  *
  * @param item Item to insert (sync_info_t struct)
  */
 void hashInsert(Item item) {
     int i = hash(item->source_dir, M);
     heads[i] = NEW(item, heads[i]);  /* Insert at front of list */
     N++;  /* Increment item count */
 }
 
 /**
  * @brief Delete an item from the hashmap
  *
  * Finds and removes the specified item from the hashmap,
  * freeing its memory.
  *
  * @param item Item to delete
  */
 void hashDelete(Item item) {
     int i = hash(item->source_dir, M);  /* Find bucket */
     link_h t = heads[i], prev = NULL;
     
     /* Search for the item in the linked list */
     while (t != z) {
         if (strcmp(t->item->source_dir, item->source_dir) == 0) {
             /* Remove from list */
             if (prev == NULL) {
                 heads[i] = t->next;  /* Item was at head of list */
             } else {
                 prev->next = t->next;  /* Item was in middle/end of list */
             }
             
             /* Free memory */
             if (t->item != NULLitem) {
                 free(t->item);
             }
             free(t);
             N--;  /* Decrement item count */
             return;
         }
         prev = t;
         t = t->next;
     }
 }
 
 /**
  * @brief Destroy the hashmap and free all memory
  *
  * Frees all memory associated with the hashmap, including
  * all nodes, items, and the bucket array.
  */
 void hashDestroy() {
     /* For each bucket */
     for (int i = 0; i < M; i++) {
         link_h t = heads[i];
         /* Free all nodes in this bucket's linked list */
         while (t != z) {
             link_h next = t->next;
             if (t->item != NULLitem) {
                 free(t->item);  /* Free the item */
             }
             free(t);  /* Free the node */
             t = next;
         }
     }
     free(heads);  /* Free the bucket array */
     free(z);      /* Free the sentinel node */
 }
 
 /**
  * @brief Get the number of buckets in the hashmap
  *
  * @return Number of buckets (M)
  */
 int hashGetSize() {
     return M;
 }
 
 /**
  * @brief Get the array of bucket heads
  *
  * @return Pointer to the array of bucket head pointers
  */
 link_h* hashGetBuckets() {
     return heads;
 }
 
 /**
  * @brief Get the sentinel node
  *
  * @return Pointer to the sentinel node
  */
 link_h hashGetZ() {
     return z;
 }
 
 /**
  * @brief Create an iterator for traversing all items in the hashmap
  *
  * Initializes an iterator that can be used with hashNext() to
  * iterate through all items in the hashmap, regardless of which
  * bucket they're in.
  *
  * @return Initialized HashIterator
  */
 HashIterator hashGetIterator() {
     HashIterator iter;
     iter.current_bucket = 0;           /* Start at first bucket */
     iter.current_node = heads[0];      /* Start at head of first bucket */
     iter.max_buckets = M;              /* Total number of buckets */
     iter.buckets = heads;              /* Bucket array */
     iter.z = z;                        /* Sentinel node */
     return iter;
 }
 
 /**
  * @brief Get the next item from the iterator
  *
  * Advances the iterator and returns the next item in the hashmap.
  * Returns NULL when there are no more items.
  *
  * @param iter Pointer to the iterator
  * @return Next item, or NULL if no more items
  */
 Item hashNext(HashIterator* iter) {
     /* While there are still buckets to examine */
     while (iter->current_bucket < iter->max_buckets) {
         /* If current node is not the sentinel */
         if (iter->current_node != iter->z) {
             /* Get item from current node */
             Item item = iter->current_node->item;
             /* Advance to next node */
             iter->current_node = iter->current_node->next;
             return item;
         }
         
         /* Current list exhausted, move to next bucket */
         iter->current_bucket++;
         if (iter->current_bucket < iter->max_buckets) {
             iter->current_node = iter->buckets[iter->current_bucket];
         }
     }
     
     return NULL;  /* No more items */
 }