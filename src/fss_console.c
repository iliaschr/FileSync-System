/**
 * @file fss_console.c
 * @brief User interface for managing and querying the FSS system
 *
 * This file implements the console interface that users interact with to
 * control the File Synchronization System. It communicates with the fss_manager
 * process through named pipes (FIFOs) to send commands and receive responses.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <sys/select.h>
 #include <time.h>
 #include <errno.h>
 
 #define FSS_IN   "fss_in"    /**< Name of the FIFO for sending commands to the manager */
 #define FSS_OUT  "fss_out"   /**< Name of the FIFO for receiving responses from the manager */
 #define BUFSIZE  1024        /**< Buffer size for commands and responses */
 
 /**
  * @brief Log a command to the console log file
  *
  * Records each command issued by the user with a timestamp to maintain
  * a history of console operations.
  *
  * @param log_file File pointer to the console log file
  * @param command The command string entered by the user
  */
 void log_command(FILE *log_file, const char *command) {
     /* Generate timestamp */
     time_t now = time(NULL);
     struct tm *tm_info = localtime(&now);
     char timestamp[32];
     strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", tm_info);
 
     /* Write to log file */
     fprintf(log_file, "%s Command %s\n", timestamp, command);
     fflush(log_file);
 }
 
 /**
  * @brief Wait for a FIFO to be created by the manager
  *
  * Polls the filesystem until the named pipe appears or a timeout occurs.
  * This ensures the console doesn't start until the manager is ready.
  *
  * @param path Path to the FIFO to wait for
  */
 void wait_for_fifo(const char *path) {
     struct stat st;
     int tries = 50;  /* Try up to 50 times (5 seconds total) */
     
     while (tries-- > 0) {
         /* Check if FIFO exists and is actually a FIFO */
         if (stat(path, &st) == 0 && S_ISFIFO(st.st_mode)) {
             return;  /* FIFO exists, we can proceed */
         }
         usleep(100 * 1000);  /* Sleep 100ms between attempts */
     }
     
     /* Timeout occurred */
     fprintf(stderr, "Timeout waiting for FIFO: %s\n", path);
     exit(EXIT_FAILURE);
 }
 
 /**
  * @brief Main function for the FSS console application
  *
  * Parses command-line arguments, initializes the console, handles the command loop,
  * and manages the communication with the FSS manager process.
  *
  * @param argc Argument count
  * @param argv Argument vector
  * @return EXIT_SUCCESS on normal exit, EXIT_FAILURE on error
  */
 int main(int argc, char *argv[]) {
     /* Validate command-line arguments */
     if (argc != 3 || strcmp(argv[1], "-l") != 0) {
         fprintf(stderr, "Usage: %s -l <console-logfile>\n", argv[0]);
         return EXIT_FAILURE;
     }
 
     /* Open log file */
     const char *log_filename = argv[2];
     FILE *log_file = fopen(log_filename, "a+");
     if (!log_file) {
         perror("Failed to open log file");
         return EXIT_FAILURE;
     }
 
     /* Wait for manager to create the communication pipes */
     wait_for_fifo(FSS_IN);
     wait_for_fifo(FSS_OUT);
 
     /* Open pipe for sending commands to manager */
     int fd_in = open(FSS_IN, O_WRONLY);
     if (fd_in < 0) {
         perror("Failed to open fss_in pipe");
         fprintf(stderr, "Make sure fss_manager is running\n");
         fclose(log_file);
         return EXIT_FAILURE;
     }
 
     /* Open pipe for receiving responses from manager */
     int fd_out = open(FSS_OUT, O_RDONLY | O_NONBLOCK);
     if (fd_out < 0) {
         perror("Failed to open fss_out pipe");
         close(fd_in);
         fclose(log_file);
         return EXIT_FAILURE;
     }
 
     /* Buffers for user commands and manager responses */
     char command[BUFSIZE];
     char response[BUFSIZE * 4];  /* Response buffer is 4x command buffer */
 
     /* Display welcome message */
     printf("FSS Console. Type 'help' for available commands.\n");
 
     /* Main command loop */
     while (1) {
         /* Display prompt and read command */
         printf("> ");
         if (!fgets(command, sizeof(command), stdin)) {
             break;  /* EOF or error */
         }
 
         /* Remove trailing newline from command */
         command[strcspn(command, "\n")] = 0;
 
         /* Handle built-in 'exit' command */
         if (strcmp(command, "exit") == 0) {
             break;
         }
 
         /* Handle built-in 'help' command */
         if (strcmp(command, "help") == 0) {
             printf("Available commands:\n");
             printf("  add <source> <target>  - Add a directory for monitoring\n");
             printf("  status <source>        - Show status of a monitored directory\n");
             printf("  cancel <source>        - Stop monitoring a directory\n");
             printf("  sync <source>          - Synchronize a directory\n");
             printf("  shutdown               - Shutdown the manager\n");
             printf("  exit                   - Exit the console\n");
             continue;
         }
 
         /* Log the command to console log file */
         log_command(log_file, command);
 
         /* Send command to manager */
         ssize_t w = write(fd_in, command, strlen(command));
         if (w < 0) {
             perror("Failed to write to fss_in pipe");
             continue;
         }
         write(fd_in, "\n", 1);  /* Send newline to terminate command */
 
         /* Wait for response with timeout */
         fd_set read_fds;
         struct timeval tv;
 
         FD_ZERO(&read_fds);
         FD_SET(fd_out, &read_fds);
         tv.tv_sec  = 5;  /* 5 second timeout */
         tv.tv_usec = 0;
 
         int sel = select(fd_out + 1, &read_fds, NULL, NULL, &tv);
         if (sel < 0) {
             perror("select");
             continue;
         } else if (sel == 0) {
             printf("Timeout waiting for response from manager\n");
             continue;
         }
 
         /* Read and display the response */
         ssize_t n = read(fd_out, response, sizeof(response) - 1);
         if (n < 0) {
             perror("Failed to read from fss_out pipe");
             continue;
         }
         response[n] = '\0';  /* Null-terminate the response */
         printf("%s", response);
 
         /* Exit if 'shutdown' command was issued */
         if (strncmp(command, "shutdown", 8) == 0) {
             break;
         }
     }
 
     /* Clean up resources */
     close(fd_in);
     close(fd_out);
     fclose(log_file);
     return EXIT_SUCCESS;
 }