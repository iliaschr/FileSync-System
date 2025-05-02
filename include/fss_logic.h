/**
 * @file fss_logic.h
 * @brief Core logic for the File Synchronization System (FSS)
 *
 * This header declares the core functionality for the FSS system, including
 * directory monitoring, worker process management, and command handling.
 * It provides the API for synchronizing files between source and target
 * directories in real-time.
 */

 #ifndef FSS_LOGIC_H
 #define FSS_LOGIC_H
 
 #define BUFSIZE 1024  /**< Default buffer size for I/O operations */
 
 #include <stdio.h>
 
 #ifndef DEBUG
 # define DEBUG 0     /**< Debug mode flag (1=enabled, 0=disabled) */
 #endif
 
 /**
  * @brief Debug macro for conditional debug output
  *
  * Prints debug messages to stderr when DEBUG is enabled
  */
 #define DBG(fmt, ...) \
 do { if (DEBUG) fprintf(stderr, "[DEBUG] " fmt, ##__VA_ARGS__); } while(0)
 
 /* Global variables */
 extern int inotify_fd;             /**< File descriptor for inotify instance */
 extern int* watch_descriptors;     /**< Array of inotify watch descriptors */
 extern int watch_count;            /**< Count of active watch descriptors */
 extern volatile int running;       /**< Flag indicating if main loop should continue */
 extern int global_fd_out;          /**< File descriptor for console output pipe */
 extern FILE* global_log_file;      /**< File pointer for manager log file */
 
 /* Function declarations */
 
 /**
  * @brief Initialize global variables
  *
  * @param log_file Pointer to log file
  * @param fd_out File descriptor for console output pipe
  * @param worker_limit Maximum number of concurrent worker processes
  */
 void init_globals(FILE* log_file, int fd_out, int worker_limit);
 
 /**
  * @brief Read configuration file and start monitoring directories
  *
  * Parses each line of the config file for source-target directory pairs,
  * initializes monitoring, and starts initial synchronization for each pair.
  *
  * @param config_file Path to configuration file
  * @param worker_limit Maximum number of concurrent worker processes
  * @param log_file Pointer to log file
  */
 void readConfig(const char* config_file, int worker_limit, FILE* log_file);
 
 /**
  * @brief Set up inotify for directory monitoring
  *
  * Initializes the inotify instance for monitoring file system events
  */
 void setup_inotify();
 
 /**
  * @brief Handle inotify events
  *
  * Processes file system events (create, modify, delete) and
  * spawns worker processes to synchronize changes.
  *
  * @param log_file Pointer to log file
  */
 void handle_inotify_events(FILE* log_file);
 
 /**
  * @brief Process commands from the console
  *
  * Parses and dispatches commands received from the fss_console
  * to the appropriate handler functions.
  *
  * @param cmdline Command line string to process
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command(const char* cmdline, int fd_out, FILE* log_file);
 
 /**
  * @brief Handle 'add' command
  *
  * Adds a new directory pair for monitoring and synchronization.
  *
  * @param source Source directory path
  * @param target Target directory path
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command_add(const char* source, const char* target, int fd_out, FILE* log_file);
 
 /**
  * @brief Handle 'cancel' command
  *
  * Stops monitoring a source directory.
  *
  * @param source Source directory path to stop monitoring
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command_cancel(const char* source, int fd_out, FILE* log_file);
 
 /**
  * @brief Handle 'status' command
  *
  * Reports status information for a monitored directory.
  *
  * @param source Source directory path to check status
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command_status(const char* source, int fd_out, FILE* log_file);
 
 /**
  * @brief Handle 'sync' command
  *
  * Performs manual synchronization of a directory pair.
  *
  * @param source Source directory path to synchronize
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command_sync(const char* source, int fd_out, FILE* log_file);
 
 /**
  * @brief Handle 'shutdown' command
  *
  * Performs graceful shutdown of the FSS manager.
  *
  * @param fd_out File descriptor for console output
  * @param log_file Pointer to log file
  */
 void handle_command_shutdown(int fd_out, FILE* log_file);
 
 /**
  * @brief SIGCHLD signal handler
  *
  * Handles the termination of worker processes.
  *
  * @param signum Signal number (SIGCHLD)
  */
 void sigchld_handler(int signum);
 
 /**
  * @brief Start a worker process for synchronization
  *
  * Creates a new worker process to perform a synchronization operation.
  * If at worker limit, the task is queued for later execution.
  *
  * @param source_dir Source directory path
  * @param target_dir Target directory path
  * @param filename File to synchronize (or "ALL" for full synchronization)
  * @param operation Operation type ("FULL", "ADDED", "MODIFIED", "DELETED")
  * @param log_file Pointer to log file
  */
 void start_worker(const char* source_dir, const char* target_dir, const char* filename,
                   const char* operation, FILE* log_file);
                   
 #endif /* FSS_LOGIC_H */