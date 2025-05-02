/**
 * @file fss_logic.c
 * @brief Core implementation of the File Synchronization System (FSS)
 *
 * This file implements the main logic for directory monitoring, file synchronization,
 * worker process management, and command handling for the FSS system. It serves as
 * the central controller that coordinates all synchronization activities between
 * source and target directories.
 */

 #include "../include/fss_logic.h"
 #include "../include/hashmap.h"
 #include "../include/sync_info.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <errno.h>
 #include <sys/inotify.h>
 #include <linux/limits.h>
 #include <signal.h>
 #include <sys/wait.h>
 #include <sys/stat.h>
 
 
 /**
  * -----------------------------------------------------------------------------
  * Type definitions for worker queue and active workers
  * -----------------------------------------------------------------------------
  */
 
 /**
  * @struct worker_task
  * @brief Represents a pending synchronization task in the queue
  *
  * When the system reaches the worker limit, new synchronization tasks
  * are queued until a worker becomes available.
  */
 typedef struct worker_task {
     char source_dir[PATH_MAX]; /**< Source directory path */
     char target_dir[PATH_MAX]; /**< Target directory path */
     char filename[PATH_MAX];   /**< File to synchronize (or "ALL" for full sync) */
     char operation[20];        /**< Operation type: "FULL", "ADDED", "MODIFIED", "DELETED" */
     struct worker_task* next;  /**< Pointer to next task in queue */
 } worker_task_t;
 
 /**
  * @struct worker_info
  * @brief Represents an active worker process
  *
  * Tracks information about a running worker process, including its
  * communication pipe and the synchronization task it's performing.
  */
 typedef struct worker_info {
     pid_t pid;                 /**< Process ID of the worker */
     int pipe_fd;               /**< File descriptor for reading worker output */
     char source_dir[PATH_MAX]; /**< Source directory being synchronized */
     char target_dir[PATH_MAX]; /**< Target directory being synchronized */
     char filename[PATH_MAX];   /**< File being synchronized (or "ALL") */
     char operation[20];        /**< Operation being performed */
     struct worker_info* next;  /**< Pointer to next active worker in list */
 } worker_info_t;
 
 /**
  * -----------------------------------------------------------------------------
  * Watch descriptor to source directory mapping
  * -----------------------------------------------------------------------------
  */
 
 /**
  * @struct watch_map_t
  * @brief Maps inotify watch descriptors to source directories
  *
  * Enables lookup of which source directory an inotify event came from
  * based on the watch descriptor included in the event.
  */
 typedef struct {
     int wd;                   /**< inotify watch descriptor */
     char source[PATH_MAX];    /**< Source directory path being watched */
 } watch_map_t;
 
 /**
  * -----------------------------------------------------------------------------
  * Global variables (single definition)
  * -----------------------------------------------------------------------------
  */
 int inotify_fd;                  /**< File descriptor for inotify instance */
 volatile int running = 1;        /**< Flag indicating if main loop should continue */
 
 int global_fd_out = -1;          /**< File descriptor for console output pipe */
 FILE* global_log_file = NULL;    /**< File pointer for manager log file */
 
 static int worker_limit_global = 5;  /**< Maximum concurrent worker processes */
 static int active_worker_count = 0;  /**< Current number of active workers */
 
 /* Active worker list and task queue */
 static worker_info_t* active_workers = NULL;  /**< Linked list of active workers */
 static worker_task_t* task_queue = NULL;      /**< Queue of pending synchronization tasks */
 
 /* Watch descriptor mapping */
 static watch_map_t* watch_map = NULL;    /**< Array of watch descriptor mappings */
 static int watch_map_len = 0;            /**< Number of entries in watch_map */
 
 /**
  * -----------------------------------------------------------------------------
  * Helper functions
  * -----------------------------------------------------------------------------
  */
 
 /**
  * @brief Generate a timestamp string in the standard format
  *
  * Creates a timestamp in the format [YYYY-MM-DD HH:MM:SS] for logging.
  * Uses a static buffer for efficiency.
  *
  * @return Pointer to static buffer containing formatted timestamp
  */
 static char* get_timestamp() {
     static char buf[32];
     time_t now = time(NULL);
     struct tm* tm_info = localtime(&now);
     strftime(buf, sizeof(buf), "[%Y-%m-%d %H:%M:%S]", tm_info);
     return buf;
 }
 
 /**
  * @brief Add a worker to the active workers list
  *
  * Creates and initializes a new worker_info structure and adds it
  * to the front of the active workers list.
  *
  * @param pid Process ID of the worker
  * @param pipe_fd File descriptor for reading worker output
  * @param src Source directory path
  * @param dst Target directory path
  * @param op Operation type
  * @param fn Filename being processed
  */
 static void add_active_worker(pid_t pid, int pipe_fd,
                               const char* src, const char* dst,
                               const char* op, const char* fn)
 {
     /* Allocate and initialize new worker info */
     worker_info_t* w = malloc(sizeof(*w));
     w->pid = pid;
     w->pipe_fd = pipe_fd;
     strcpy(w->source_dir, src);
     strcpy(w->target_dir, dst);
     strcpy(w->operation, op);
     strcpy(w->filename, fn);
     
     /* Add to front of list and update count */
     w->next = active_workers;
     active_workers = w;
     active_worker_count++;
 }
 
 /**
  * @brief Remove and return a worker from the active workers list
  *
  * Searches for a worker with the given PID, removes it from the list,
  * and returns a pointer to it. The caller is responsible for freeing
  * the returned structure after use.
  *
  * @param pid Process ID of the worker to remove
  * @return Pointer to removed worker_info, or NULL if not found
  */
 static worker_info_t* remove_active_worker(pid_t pid) {
     worker_info_t *cur = active_workers, *prev = NULL;
     
     /* Search for worker with matching PID */
     while (cur) {
         if (cur->pid == pid) {
             /* Remove from list */
             if (prev) prev->next = cur->next;
             else active_workers = cur->next;
             
             /* Update count and return */
             active_worker_count--;
             return cur;
         }
         prev = cur; 
         cur = cur->next;
     }
     
     return NULL;  /* Worker not found */
 }
 
 /**
  * @brief Check if a worker is already active for a source directory
  *
  * Searches the active workers list for any worker processing
  * the specified source directory.
  *
  * @param src Source directory path to check
  * @return 1 if a worker is active for this source, 0 otherwise
  */
 static int is_worker_active_for_source(const char* src) {
     /* Iterate through active workers */
     for (worker_info_t* w = active_workers; w; w = w->next)
         if (!strcmp(w->source_dir, src))
             return 1;  /* Found a matching worker */
     
     return 0;  /* No matching worker found */
 }
 
 /**
  * @brief Add a task to the queue
  *
  * Creates a new worker_task structure and adds it to the
  * end of the task queue.
  *
  * @param src Source directory path
  * @param dst Target directory path
  * @param fn Filename to process
  * @param op Operation type
  */
 static void queue_task(const char* src, const char* dst,
                        const char* fn, const char* op)
 {
     /* Create and initialize task */
     worker_task_t* t = malloc(sizeof(*t));
     strcpy(t->source_dir, src);
     strcpy(t->target_dir, dst);
     strcpy(t->filename, fn);
     strcpy(t->operation, op);
     t->next = NULL;
     
     /* Add to queue (either empty or at end) */
     if (!task_queue) {
         task_queue = t;  /* First task in queue */
     } else {
         /* Find end of queue */
         worker_task_t* c = task_queue;
         while (c->next) c = c->next;
         c->next = t;     /* Add to end */
     }
 }
 
 /**
  * @brief Remove and return the first task from the queue
  *
  * @return Pointer to the task removed, or NULL if queue is empty
  */
 static worker_task_t* dequeue_task() {
     if (!task_queue) return NULL;
     
     /* Remove first task */
     worker_task_t* t = task_queue;
     task_queue = t->next;
     
     return t;
 }
 
 /**
  * -----------------------------------------------------------------------------
  * Forward declarations for internal functions
  * -----------------------------------------------------------------------------
  */
 static void process_worker_output(worker_info_t* w);
 static void start_queued_task();
 
 /**
  * -----------------------------------------------------------------------------
  * Public API Implementation
  * -----------------------------------------------------------------------------
  */
 
 /**
  * @brief Initialize global variables
  *
  * Sets up global variables with values provided by the manager.
  *
  * @param log_file File pointer for the manager's log file
  * @param fd_out File descriptor for console output pipe
  * @param worker_limit Maximum number of concurrent worker processes
  */
 void init_globals(FILE* log_file, int fd_out, int worker_limit) {
     global_log_file = log_file;
     global_fd_out = fd_out;
     worker_limit_global = worker_limit;
 }
 
 /**
  * @brief Read configuration file and initialize synchronization
  *
  * Parses the config file for source-target directory pairs,
  * initializes monitoring, and starts synchronization.
  *
  * @param config_file Path to the configuration file
  * @param unused Unused parameter (kept for API compatibility)
  * @param log_file File pointer for logging
  */
 void readConfig(const char* config_file, int unused, FILE* log_file) {
     (void)unused;  /* Suppress unused parameter warning */
     
     /* Open config file */
     FILE* fp = fopen(config_file, "r");
     if (!fp) { 
         perror("fopen config"); 
         exit(EXIT_FAILURE); 
     }
 
     /* Process each line in the config file */
     char src[PATH_MAX], dst[PATH_MAX], line[PATH_MAX*2];
     while (fgets(line, sizeof(line), fp)) {
         /* Skip empty lines and comments */
         if (line[0]=='\n' || line[0]=='#') continue;
         
         /* Parse source and target directories */
         if (sscanf(line,"%s %s",src,dst)==2) {
             /* Create sync_info structure */
             sync_info_t* info = malloc(sizeof(*info));
             strcpy(info->source_dir, src);
             strcpy(info->target_dir, dst);
             info->active = 1;
             info->last_sync_time = 0;   /* Never synchronized yet */
             info->error_count = 0;
             info->next = NULL;
             
             /* Add to hashmap */
             hashInsert(info);
 
             /* Log to file and console */
             char* ts = get_timestamp();
             fprintf(log_file, "%s Added directory: %s -> %s\n", ts, src, dst);
             fprintf(log_file, "%s Monitoring started for %s\n", ts, src);
             fflush(log_file);
             dprintf(global_fd_out, "%s Added directory: %s -> %s\n", ts, src, dst);
             dprintf(global_fd_out, "%s Monitoring started for %s\n", ts, src);
 
             /* Ensure target directory exists */
             mkdir(dst, 0777);
 
             /* Set up inotify watch */
             int wd = inotify_add_watch(inotify_fd, src, 
                                       IN_CREATE|IN_MODIFY|IN_DELETE);
             
             /* Add to watch map */
             watch_map = realloc(watch_map, (watch_map_len+1)*sizeof(*watch_map));
             watch_map[watch_map_len].wd = wd;
             strncpy(watch_map[watch_map_len].source, src, PATH_MAX);
             watch_map_len++;
 
             /* Start initial full synchronization */
             start_worker(src, dst, "ALL", "FULL", log_file);
         }
     }
     
     fclose(fp);
 }
 
 /**
  * @brief Initialize inotify for directory monitoring
  *
  * Sets up the inotify instance with non-blocking mode and
  * initializes the watch mapping data structure.
  */
 void setup_inotify() {
     /* Create non-blocking inotify instance */
     inotify_fd = inotify_init1(IN_NONBLOCK);
     if (inotify_fd < 0) { 
         perror("inotify_init"); 
         exit(EXIT_FAILURE); 
     }
     
     /* Initialize watch map */
     watch_map = NULL;
     watch_map_len = 0;
 }
 
 /**
  * @brief Process inotify events
  *
  * Reads and processes events from the inotify file descriptor,
  * mapping watch descriptors to source directories and spawning
  * workers to handle file changes.
  *
  * @param log_file File pointer for logging
  */
 void handle_inotify_events(FILE* log_file) {
     char buf[4096];
     
     /* Read events (non-blocking) */
     ssize_t len = read(inotify_fd, buf, sizeof(buf));
     if (len <= 0) return;  /* No events or error */
 
     /* Process each event in buffer */
     for (char* p = buf; p < buf+len; ) {
         struct inotify_event* ev = (void*)p;
         
         /* Find source directory for this watch descriptor */
         const char* src = NULL;
         for (int i = 0; i < watch_map_len; i++) {
             if (watch_map[i].wd == ev->wd) {
                 src = watch_map[i].source;
                 break;
             }
         }
         
         if (!src) {
             /* Unknown watch descriptor */
             fprintf(stderr, "Unknown watch descriptor %d\n", ev->wd);
         } else if (ev->len > 0) {
             /* Determine operation type */
             const char* op = (ev->mask&IN_CREATE) ? "ADDED" :
                             (ev->mask&IN_MODIFY) ? "MODIFIED" :
                             (ev->mask&IN_DELETE) ? "DELETED" : "UNKNOWN";
             
             /* Get sync_info for this source directory */
             sync_info_t* info = hashSearch((char*)src);
             
             /* Log event start */
             char* ts = get_timestamp();
             fprintf(log_file,
                    "%s [%s] [%s] [0] [%s] [STARTED] [File: %s]\n",
                    ts, src, info->target_dir, op, ev->name);
             fflush(log_file);
             
             /* Spawn worker to handle the file change */
             start_worker(src, info->target_dir, ev->name, op, log_file);
         }
         
         /* Move to next event */
         p += sizeof(*ev) + ev->len;
     }
 }
 
 /**
  * @brief Process commands from the console
  *
  * Parses command lines received from the console and
  * dispatches them to the appropriate handler functions.
  *
  * @param cmdline Command line to process
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command(const char* cmdline, int fd_out, FILE* log_file) {
     char cmd[BUFSIZE], a1[PATH_MAX], a2[PATH_MAX];
     
     /* Parse command and arguments */
     int n = sscanf(cmdline, "%s %s %s", cmd, a1, a2);
     
     /* Dispatch to appropriate handler */
     if (!strcmp(cmd, "add") && n==3) 
         handle_command_add(a1, a2, fd_out, log_file);
     else if (!strcmp(cmd, "cancel") && n==2) 
         handle_command_cancel(a1, fd_out, log_file);
     else if (!strcmp(cmd, "status") && n==2) 
         handle_command_status(a1, fd_out, log_file);
     else if (!strcmp(cmd, "sync") && n==2) 
         handle_command_sync(a1, fd_out, log_file);
     else if (!strcmp(cmd, "shutdown")) 
         handle_command_shutdown(fd_out, log_file);
     else 
         dprintf(fd_out, "Unrecognized: %s\n", cmdline);
 }
 
 /**
  * @brief Handle 'add' command
  *
  * Adds a new directory pair for monitoring and synchronization.
  * Note: This is a placeholder implementation that only handles
  * the case where the directory is already monitored.
  *
  * @param src Source directory path
  * @param dst Target directory path
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command_add(const char* src, const char* dst, int fd_out, FILE* log_file) {
     /* Check if already monitored */
     sync_info_t* e = hashSearch((char*)src);
     char* ts = get_timestamp();
     
     if (e && e->active && !strcmp(e->target_dir, dst)) {
         /* Already monitored with same target */
         dprintf(fd_out, "%s Already in queue: %s\n", ts, src);
         fprintf(log_file, "%s Already in queue: %s\n", ts, src);
         fflush(log_file);
         return;
     }
     
     /* Full implementation left as exercise */
     /* Would need to create sync_info, add inotify watch, start sync */
 }
 
 /**
  * @brief Handle 'cancel' command
  *
  * Stops monitoring a source directory and removes its inotify watch.
  *
  * @param source Source directory to stop monitoring
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command_cancel(const char* source, int fd_out, FILE* log_file) {
     /* Look up sync_info for this source */
     sync_info_t* info = hashSearch((char*)source);
     char* ts = get_timestamp();
 
     if (info && info->active) {
         /* Mark as inactive */
         info->active = 0;
         
         /* Log to file */
         fprintf(log_file, "%s Monitoring stopped for %s\n", ts, source);
     } else {
         /* Not monitored */
         fprintf(log_file, "%s Directory not monitored: %s\n", ts, source);
     }
     fflush(log_file);
 
     /* Send response to console */
     if (info && info->active == 0) {
         dprintf(fd_out, "%s Monitoring stopped for %s\n", ts, source);
     } else {
         dprintf(fd_out, "%s Directory not monitored: %s\n", ts, source);
     }
 
     /* Remove inotify watch if directory was active */
     if (info) {
         for (int i = 0; i < watch_map_len; i++) {
             if (strcmp(watch_map[i].source, source) == 0) {
                 inotify_rm_watch(inotify_fd, watch_map[i].wd);
                 break;
             }
         }
     }
 }
 
 /**
  * @brief Handle 'status' command
  *
  * Reports synchronization status for a monitored directory.
  *
  * @param source Source directory to check status
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command_status(const char* source, int fd_out, FILE* log_file) {
     /* Look up sync_info */
     sync_info_t* info = hashSearch((char*)source);
     char* ts = get_timestamp();
 
     /* Always log the status request */
     fprintf(log_file, "%s Status requested for %s\n", ts, source);
     fflush(log_file);
 
     if (info && info->active) {
         /* Directory is being monitored - format last sync time */
         char lst[32];
         strftime(lst, sizeof(lst), "%Y-%m-%d %H:%M:%S", 
                 localtime(&info->last_sync_time));
         
         /* Send status information to console */
         dprintf(fd_out,
                 "%s Status requested for %s\n"
                 "Directory: %s\n"
                 "Target: %s\n"
                 "Last Sync: %s\n"
                 "Errors: %d\n"
                 "Status: Active\n",
                 ts, source,
                 source,
                 info->target_dir,
                 lst,
                 info->error_count);
     } else {
         /* Directory not monitored */
         dprintf(fd_out, "%s Directory not monitored: %s\n", ts, source);
     }
 }
 
 /**
  * @brief Handle 'sync' command
  *
  * Initiates manual synchronization of a directory.
  *
  * @param source Source directory to synchronize
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command_sync(const char* source, int fd_out, FILE* log_file) {
     DBG("handle_command_sync(%s)\n", source);
     
     /* Look up sync_info */
     sync_info_t* info = hashSearch((char*)source);
     char* ts = get_timestamp();
 
     if (!info || !info->active) {
         /* Not being monitored */
         fprintf(log_file, "%s Directory not monitored: %s\n", ts, source);
         fflush(log_file);
         dprintf(fd_out, "%s Directory not monitored: %s\n", ts, source);
         return;
     }
 
     /* Check if sync already in progress */
     if (is_worker_active_for_source(source)) {
         fprintf(log_file, "%s Sync already in progress %s\n", ts, source);
         fflush(log_file);
         dprintf(fd_out, "%s Sync already in progress %s\n", ts, source);
         return;
     }
 
     /* Start new synchronization */
     fprintf(log_file, "%s Syncing directory: %s -> %s\n", ts, source, info->target_dir);
     fflush(log_file);
     dprintf(fd_out, "%s Syncing directory: %s -> %s\n", ts, source, info->target_dir);
 
     start_worker(source, info->target_dir, "ALL", "FULL", log_file);
 }
 
 /**
  * @brief Handle 'shutdown' command
  *
  * Performs graceful shutdown of the FSS manager, waiting for
  * active workers to complete and draining the task queue.
  *
  * @param fd_out File descriptor for console output
  * @param log_file File pointer for logging
  */
 void handle_command_shutdown(int fd_out, FILE* log_file) {
     char* ts = get_timestamp();
     
     /* Send shutdown messages to console */
     dprintf(fd_out, "%s Shutting down manager...\n", ts);
     dprintf(fd_out, "%s Waiting for all active workers to finish.\n", ts);
     dprintf(fd_out, "%s Processing remaining queued tasks.\n", ts);
 
     /* Log shutdown process */
     fprintf(log_file,
             "%s Shutting down manager...\n"
             "%s Waiting for all active workers to finish.\n"
             "%s Processing remaining queued tasks.\n",
             ts, ts, ts);
     fflush(log_file);
 
     /* Wait for all active workers to finish */
     for (worker_info_t* w = active_workers; w; w = w->next) {
         waitpid(w->pid, NULL, 0);
         process_worker_output(w);
     }
     
     /* Drain the task queue */
     while (dequeue_task()) {}
     
     /* Set flag to exit main loop */
     running = 0;
 
     /* Send completion message */
     dprintf(fd_out, "%s Manager shutdown complete.\n", ts);
     fprintf(log_file, "%s Manager shutdown complete.\n", ts);
     fflush(log_file);
     
     /* Clean up hashmap */
     hashDestroy();
 }
 
 /**
  * @brief SIGCHLD signal handler
  *
  * Handles the termination of worker processes, processes their
  * output, and starts queued tasks if worker limit allows.
  *
  * @param signum Signal number (SIGCHLD)
  */
 void sigchld_handler(int signum) {
     pid_t pid;
     int status;
 
     /* Reap all terminated child processes */
     while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
         DBG("sigchld: reaped %d\n", pid);
         
         /* Find and remove the worker from active list */
         worker_info_t* w = remove_active_worker(pid);
         
         if (w) {
             /* Process its output and start queued task if possible */
             process_worker_output(w);
             start_queued_task();
         }
     }
 }
 
 /**
  * @brief Start a worker process for synchronization
  *
  * Creates a new worker process to perform a synchronization operation.
  * If at worker limit, the task is queued for later execution.
  *
  * @param src Source directory path
  * @param dst Target directory path
  * @param fn Filename to synchronize (or "ALL" for full sync)
  * @param op Operation type ("FULL", "ADDED", "MODIFIED", "DELETED")
  * @param log_file File pointer for logging
  */
 void start_worker(const char* src, const char* dst,
                   const char* fn, const char* op,
                   FILE* log_file)
 {
     /* Check if a worker is already active for this source */
     if (is_worker_active_for_source(src)) {
         return;  /* Already being handled */
     }
 
     /* If at worker limit, queue the task */
     if (active_worker_count >= worker_limit_global) {
         queue_task(src, dst, fn, op);
         fprintf(log_file, "%s Queued task: %s -> %s (%s %s)\n",
                 get_timestamp(), src, dst, op, fn);
         fflush(log_file);
         return;
     }
     
     /* Create pipe for worker output */
     int p[2];
     if (pipe(p) < 0) { 
         perror("pipe"); 
         return; 
     }
     
     /* Fork worker process */
     pid_t pid = fork();
     if (pid < 0) { 
         perror("fork"); 
         close(p[0]); 
         close(p[1]); 
         return; 
     }
     
     if (!pid) {
         /* Child process (worker) */
         close(p[0]);  /* Close read end */
         
         /* Redirect stdout to pipe */
         dup2(p[1], STDOUT_FILENO);
         close(p[1]);
         
         /* Execute worker binary */
         execl("./worker", "worker", src, dst, fn, op, NULL);
         
         /* If exec fails */
         perror("execl"); 
         exit(1);
     }
     
     /* Parent process (manager) */
     close(p[1]);  /* Close write end */
     
     /* Add to active workers list */
     add_active_worker(pid, p[0], src, dst, op, fn);
     
     /* Log worker start */
     fprintf(log_file, "%s [%s] [%s] [%d] [%s] [STARTED] [File: %s]\n",
             get_timestamp(), src, dst, pid, op, fn);
     fflush(log_file);
 }
 
 /**
  * @brief Process worker output and log results
  *
  * Reads the worker's stdout, parses the EXEC_REPORT, updates
  * sync_info, and logs the synchronization result.
  *
  * @param w Pointer to worker_info structure
  */
 static void process_worker_output(worker_info_t* w) {
     char buf[4096] = {0}, *line;
     int inrep = 0;
     char status[16] = "UNKNOWN", details[128] = "";
 
     /* Set pipe to non-blocking mode */
     fcntl(w->pipe_fd, F_SETFL, O_NONBLOCK);
     
     /* Read all available data from pipe */
     ssize_t n;
     while ((n = read(w->pipe_fd, buf + strlen(buf), 
                      sizeof(buf) - strlen(buf) - 1)) > 0) {}
     
     close(w->pipe_fd);
 
     /* Parse the worker's output line by line */
     line = strtok(buf, "\n");
     while (line) {
         if (!strcmp(line, "EXEC_REPORT_START")) {
             inrep = 1;  /* Start of report */
         } else if (!strcmp(line, "EXEC_REPORT_END")) {
             inrep = 0;  /* End of report */
         } else if (inrep) {
             /* Extract status and details from report */
             if (!strncmp(line, "STATUS: ", 8)) 
                 strcpy(status, line + 8);
             if (!strncmp(line, "DETAILS:", 8)) 
                 strcpy(details, line + 8);
         }
         line = strtok(NULL, "\n");
     }
 
     /* Update sync_info */
     sync_info_t* i = hashSearch(w->source_dir);
     if (i) {
         i->last_sync_time = time(NULL);
         if (!strcmp(status, "ERROR")) 
             i->error_count++;
     }
 
     /* Log the completion and result */
     fprintf(global_log_file,
             "%s [%s] [%s] [%d] [%s] [%s] [%s]\n",
             get_timestamp(), w->source_dir, w->target_dir,
             w->pid, w->operation, status, details);
     fflush(global_log_file);
 
     /* Free worker info structure */
     free(w);
 }
 
 /**
  * @brief Start a queued task if under worker limit
  *
  * Checks if the system is under the worker limit, and if so,
  * dequeues and starts a task from the queue.
  */
 static void start_queued_task() {
     if (active_worker_count < worker_limit_global) {
         /* Dequeue a task */
         worker_task_t* t = dequeue_task();
         
         if (t) {
             /* Start worker for this task */
             start_worker(t->source_dir,
                         t->target_dir,
                         t->filename,
                         t->operation,
                         global_log_file);
             
             /* Free task structure */
             free(t);
         }
     }
 }