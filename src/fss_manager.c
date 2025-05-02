/**
 * @file fss_manager.c
 * @brief Main application manager for the File Synchronization System (FSS)
 *
 * This file implements the main entry point and control loop for the FSS system.
 * The manager is responsible for:
 * - Initializing the synchronization system
 * - Creating communication channels with the console (named pipes)
 * - Monitoring directories for changes using inotify
 * - Managing worker processes that perform actual synchronization
 * - Processing commands from the user console
 * - Coordinating synchronization activities between source and target directories
 */

 #include "../include/cli_parser.h"
 #include "../include/fss_logic.h"
 #include "../include/hashmap.h"
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <stdio.h>
 #include <errno.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 #include <sys/select.h>
 #include <signal.h>
 
 #define BUFSIZE 1024  /**< Buffer size for reading commands */
 
 /**
  * @brief Main entry point for the FSS manager
  *
  * Initializes the system, sets up communication channels, and runs the
  * main event loop processing commands and file system events.
  *
  * @param argc Number of command-line arguments
  * @param argv Command-line argument array
  * @return 0 on successful shutdown, non-zero on error
  */
 int main(int argc, char *argv[]) {
     /* Ignore SIGPIPE to prevent termination when writing to closed pipes */
     signal(SIGPIPE, SIG_IGN);
     
     /* Parse command-line arguments */
     struct args input = parseArgsManager(argc, argv);
     
     /* Open log file */
     FILE *log_file = fopen(input.logfile, "a+");
     if (!log_file) { 
         perror("open log"); 
         exit(EXIT_FAILURE); 
     }
     
     /* Create named pipes (FIFOs) for communication with console */
     unlink("fss_in"); /* Remove existing pipes if any */
     unlink("fss_out");
     if (mkfifo("fss_in", 0666) || mkfifo("fss_out", 0666)) {
         perror("mkfifo"); 
         exit(EXIT_FAILURE);
     }
     
     /* Initialize data structures */
     hashInit(127);       /* Initialize the hashmap for storing sync info */
     setup_inotify();     /* Set up inotify for directory monitoring */
     
     /* Open input pipe in non-blocking mode */
     int fd_in = open("fss_in", O_RDONLY | O_NONBLOCK);
     if (fd_in < 0) { 
         perror("open in"); 
         exit(EXIT_FAILURE); 
     }
     
     /* Output pipe will be opened later when console connects */
     int fd_out = -1;
     
     /* Initialize global variables needed by worker processes and handlers */
     init_globals(log_file, fd_out, input.worker_limit);
     
     /* Read configuration file and start initial synchronization */
     readConfig(input.config_file, input.worker_limit, log_file);
     
     /* Install signal handler for SIGCHLD (child process termination) */
     struct sigaction sa = { .sa_handler = sigchld_handler };
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
     if (sigaction(SIGCHLD, &sa, NULL)) { 
         perror("sigaction"); 
     }
     
     /* Main event loop - process inotify events and console commands */
     fd_set rfds;
     while (running) {
         /* Try opening output pipe if not already connected */
         if (global_fd_out < 0) {
             global_fd_out = open("fss_out", O_WRONLY | O_NONBLOCK);
         }
         
         /* Set up file descriptor set for select() */
         FD_ZERO(&rfds);
         FD_SET(fd_in, &rfds);
         FD_SET(inotify_fd, &rfds);
         int maxfd = (fd_in > inotify_fd ? fd_in : inotify_fd) + 1;
         
         /* Set timeout for select() */
         struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
         
         /* Wait for events with timeout */
         int r = select(maxfd, &rfds, NULL, NULL, &tv);
         if (r < 0) { 
             if (errno == EINTR) continue; /* Interrupted by signal, retry */
             perror("select"); 
             break; 
         }
         
         /* Process commands from console */
         if (FD_ISSET(fd_in, &rfds)) {
             char buf[BUFSIZE];
             ssize_t n = read(fd_in, buf, BUFSIZE-1);
             if (n > 0) {
                 buf[n] = 0; /* Null-terminate the buffer */
                 
                 /* Process each command (may be multiple commands separated by newlines) */
                 char *cmd = strtok(buf, "\n");
                 while (cmd) {
                     handle_command(cmd, global_fd_out, log_file);
                     cmd = strtok(NULL, "\n");
                 }
             }
         }
         
         /* Process inotify events (file system changes) */
         if (FD_ISSET(inotify_fd, &rfds)) {
             handle_inotify_events(log_file);
         }
     }
     
     /* Clean up resources before exit */
     close(fd_in);
     if (global_fd_out >= 0) close(global_fd_out);
     close(inotify_fd);
     fclose(log_file);
     unlink("fss_in");
     unlink("fss_out");
     hashDestroy();
     
     return 0;
 }