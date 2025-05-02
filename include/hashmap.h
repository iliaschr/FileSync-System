/**
 * @file hashmap.h
 * @brief Hashmap implementation for sync_info storage and retrieval
 *
 * This header defines a hashmap data structure optimized for storing and
 * retrieving sync_info_t structures by source directory path. It provides
 * fast lookups used by the FSS system to track monitored directories.
 */

 #ifndef HASHMAP_H
 #define HASHMAP_H
 
 #include "sync_info.h"
 #include <string.h>
 #include <stdlib.h>
 
 #define NULLitem NULL  /**< Sentinel value for null items */
 
 typedef sync_info_t* Item;  /**< Item type stored in the hashmap */
 typedef struct node* link_h;  /**< Pointer to a hashmap node */
 
 /**
  * @struct node
  * @brief Node in the hashmap's linked list
  *
  * Each bucket in the hashmap contains a linked list of these nodes.
  */
 struct node {
     Item item;   /**< The sync_info_t item stored in this node */
     link_h next; /**< Pointer to the next node in the linked list */
 };
 
 typedef char* Key;  /**< Key type used to index the hashmap (string) */
 
 /**
  * @brief Calculate hash value for a string key
  *
  * @param v String key to hash
  * @param M Size of the hashmap
  * @return Integer hash value in range [0, M-1]
  */
 int hash(char* v, int M);
 
 /**
  * @brief Initialize the hashmap
  *
  * Creates and initializes a new hashmap with the specified maximum size.
  *
  * @param max Maximum number of items expected (actual size will be max/5)
  */
 void hashInit(int max);
 
 /**
  * @brief Search for an item by key
  *
  * Searches the hashmap for an item with the specified key.
  *
  * @param v Key to search for (source directory path)
  * @return Pointer to the item if found, NULL otherwise
  */
 Item hashSearch(Key v);
 
 /**
  * @brief Insert an item into the hashmap
  *
  * Inserts a new item into the hashmap using the item's source_dir as the key.
  *
  * @param item Pointer to the item to insert
  */
 void hashInsert(Item item);
 
 /**
  * @brief Delete an item from the hashmap
  *
  * Removes and frees the specified item from the hashmap.
  *
  * @param item Pointer to the item to delete
  */
 void hashDelete(Item item);
 
 /**
  * @brief Destroy the hashmap
  *
  * Frees all memory associated with the hashmap and its items.
  */
 void hashDestroy();
 
 /**
  * @brief Get the size of the hashmap
  *
  * @return Number of buckets in the hashmap
  */
 int hashGetSize();
 
 /**
  * @brief Get the array of bucket heads
  *
  * @return Pointer to the array of bucket head pointers
  */
 link_h* hashGetBuckets();
 
 /**
  * @brief Get the sentinel node
  *
  * @return Pointer to the sentinel node used to mark the end of lists
  */
 link_h hashGetZ();
 
 /**
  * @struct HashIterator
  * @brief Iterator for traversing all items in the hashmap
  *
  * Provides a way to iterate through all items in the hashmap
  * regardless of which bucket they're in.
  */
 typedef struct {
     int current_bucket;  /**< Current bucket being examined */
     link_h current_node; /**< Current node in the current bucket */
     int max_buckets;     /**< Total number of buckets */
     link_h* buckets;     /**< Array of bucket head pointers */
     link_h z;            /**< Sentinel node pointer */
 } HashIterator;
 
 /**
  * @brief Create a new hashmap iterator
  *
  * @return Initialized HashIterator for the hashmap
  */
 HashIterator hashGetIterator();
 
 /**
  * @brief Get the next item from the iterator
  *
  * Advances the iterator and returns the next item.
  *
  * @param iter Pointer to the HashIterator
  * @return Next item in the hashmap, or NULL if no more items
  */
 Item hashNext(HashIterator* iter);
 
 #endif /* HASHMAP_H */