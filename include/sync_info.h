/**
 * @file sync_info.h
 * @brief Synchronization information structure for the File Synchronization System
 *
 * This header defines the core data structure for tracking synchronization
 * information between source and target directories. Each sync_info_t instance
 * represents a directory pair being monitored and synchronized.
 */

 #ifndef SYNC_INFO
 #define SYNC_INFO
 
 #include <stdbool.h>
 #include <time.h>
 #include <linux/limits.h>
 
 /**
  * @struct sync_info
  * @brief Structure containing synchronization information for a directory pair
  *
  * Tracks the status, history, and configuration for a synchronized directory pair.
  * This is the primary data structure for the FSS system and is stored in the hashmap.
  */
 typedef struct sync_info {
     char source_dir[PATH_MAX];   /**< Path to the source directory being monitored */
     char target_dir[PATH_MAX];   /**< Path to the target directory for synchronization */
     int active;                  /**< Flag indicating if monitoring is active (1) or stopped (0) */
     time_t last_sync_time;       /**< Timestamp of the most recent synchronization */
     int error_count;             /**< Number of errors encountered during synchronization */
     struct sync_info* next;      /**< Pointer to next item (for linked list implementation) */
     bool syncing;                /**< Flag indicating if synchronization is currently in progress */
 } sync_info_t;
 
 /**
  * @struct hashmap_t
  * @brief Structure representing a hashmap for storing sync_info_t objects
  *
  * Fixed-size hashmap with 128 buckets, each containing a linked list of sync_info_t objects.
  * This is an alternative implementation that can be used instead of the more generic hashmap.h.
  */
 typedef struct {
     sync_info_t* buckets[128];   /**< Array of bucket head pointers */
 } hashmap_t;
 
 #endif /* SYNC_INFO */