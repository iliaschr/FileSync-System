/**
 * @file worker.c
 * @brief Worker process implementation for file synchronization operations
 *
 * This file implements the worker process responsible for performing actual
 * file synchronization tasks in the FSS (File Synchronization System). Each
 * worker handles a specific synchronization operation between source and
 * target directories, such as copying a new file, updating a modified file,
 * deleting a file, or performing a full directory synchronization.
 *
 * Workers are spawned by the fss_manager process and communicate their results
 * back to the manager through standard output in a structured format.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <fcntl.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <errno.h>
 #include <time.h>
 #include <dirent.h>
 #include <linux/limits.h>
 
 #define BUFFER_SIZE 4096  /**< Buffer size for file I/O operations */
 
 /**
  * @brief Copy a file from source to target using low-level I/O syscalls
  *
  * Implements file copying using open(), read(), write(), and close()
  * system calls as required by the assignment. Handles error conditions
  * and reports success or failure to stdout.
  *
  * @param source_path Path to the source file
  * @param target_path Path to the target file (will be created or overwritten)
  */
 void copy_file(const char *source_path, const char *target_path) {
     int source_fd, target_fd;
     char buffer[BUFFER_SIZE];
     ssize_t bytes_read, bytes_written;
     int errors = 0;
     
     /* Open source file */
     source_fd = open(source_path, O_RDONLY);
     if (source_fd < 0) {
         fprintf(stderr, "Error opening source file %s: %s\n", source_path, strerror(errno));
         printf("ERROR: Cannot open source file %s: %s\n", source_path, strerror(errno));
         return;
     }
     
     /* Create or overwrite target file with permissions rw-r--r-- */
     target_fd = open(target_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
     if (target_fd < 0) {
         fprintf(stderr, "Error creating target file %s: %s\n", target_path, strerror(errno));
         printf("ERROR: Cannot create target file %s: %s\n", target_path, strerror(errno));
         close(source_fd);
         return;
     }
     
     /* Copy data in chunks */
     while ((bytes_read = read(source_fd, buffer, BUFFER_SIZE)) > 0) {
         bytes_written = write(target_fd, buffer, bytes_read);
         if (bytes_written != bytes_read) {
             fprintf(stderr, "Error writing to target file %s: %s\n", target_path, strerror(errno));
             printf("ERROR: Write error for %s: %s\n", target_path, strerror(errno));
             errors++;
             break;
         }
     }
     
     /* Check for read error */
     if (bytes_read < 0) {
         fprintf(stderr, "Error reading from source file %s: %s\n", source_path, strerror(errno));
         printf("ERROR: Read error for %s: %s\n", source_path, strerror(errno));
         errors++;
     }
     
     /* Close file descriptors */
     close(source_fd);
     close(target_fd);
     
     /* Report success if no errors occurred */
     if (errors == 0) {
         printf("SUCCESS: Copied %s to %s\n", source_path, target_path);
     }
 }
 
 /**
  * @brief Delete a file from the target directory
  *
  * Uses the unlink() system call to remove a file, handling errors
  * and reporting success or failure to stdout.
  *
  * @param target_path Path to the file to delete
  */
 void delete_file(const char *target_path) {
     if (unlink(target_path) < 0) {
         fprintf(stderr, "Error deleting file %s: %s\n", target_path, strerror(errno));
         printf("ERROR: Cannot delete %s: %s\n", target_path, strerror(errno));
     } else {
         printf("SUCCESS: Deleted %s\n", target_path);
     }
 }
 
 /**
  * @brief Perform a full synchronization between source and target directories
  *
  * Copies all files from the source directory to the target directory,
  * handling errors and reporting overall status. Creates the target
  * directory if it doesn't exist.
  *
  * @param source_dir Path to the source directory
  * @param target_dir Path to the target directory
  */
 void full_sync(const char *source_dir, const char *target_dir) {
     /* Add a small delay for testing purposes */
     sleep(1);
     
     DIR *dir;
     struct dirent *entry;
     int files_processed = 0;
     int files_skipped = 0;
     int errors = 0;
     
     /* Open source directory */
     dir = opendir(source_dir);
     
     if (!dir) {
         fprintf(stderr, "Error opening directory %s: %s\n", source_dir, strerror(errno));
         printf("ERROR: Cannot open source directory %s: %s\n", source_dir, strerror(errno));
         return;
     }
     
     /* Ensure target directory exists */
     struct stat st;
     if (stat(target_dir, &st) < 0 || !S_ISDIR(st.st_mode)) {
         /* Target doesn't exist or isn't a directory, create it */
         if (mkdir(target_dir, 0755) < 0) {
             fprintf(stderr, "Error creating target directory %s: %s\n", target_dir, strerror(errno));
             printf("ERROR: Cannot create target directory %s: %s\n", target_dir, strerror(errno));
             closedir(dir);
             return;
         }
     }
     
     /* Process each file in the directory */
     while ((entry = readdir(dir)) != NULL) {
         /* Skip . and .. */
         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
             continue;
         }
         
         /* Construct full paths */
         char source_path[PATH_MAX], target_path[PATH_MAX];
         snprintf(source_path, PATH_MAX, "%s/%s", source_dir, entry->d_name);
         snprintf(target_path, PATH_MAX, "%s/%s", target_dir, entry->d_name);
         
         /* Only handle regular files, not subdirectories (per assignment specs) */
         struct stat st;
         if (stat(source_path, &st) < 0) {
             fprintf(stderr, "Error stating file %s: %s\n", source_path, strerror(errno));
             printf("ERROR: Cannot stat %s: %s\n", source_path, strerror(errno));
             files_skipped++;
             errors++;
             continue;
         }
         
         if (S_ISREG(st.st_mode)) {
             /* Regular file, copy it */
             copy_file(source_path, target_path);
             files_processed++;
         } else {
             /* Not a regular file, skip it */
             files_skipped++;
         }
     }
     
     /* Clean up */
     closedir(dir);
     
     /* Send execution report to manager */
     printf("EXEC_REPORT_START\n");
     if (errors > 0) {
         if (files_processed > 0) {
             printf("STATUS: PARTIAL\n");
             printf("DETAILS: %d files copied, %d skipped\n", files_processed, files_skipped);
         } else {
             printf("STATUS: ERROR\n");
             printf("DETAILS: Operation failed\n");
         }
     } else {
         printf("STATUS: SUCCESS\n");
         printf("DETAILS: %d files processed\n", files_processed);
     }
     printf("EXEC_REPORT_END\n");
 }
 
 /**
  * @brief Main entry point for the worker process
  *
  * Parses command-line arguments and performs the requested synchronization
  * operation based on the specified parameters:
  * - source_dir: Source directory path
  * - target_dir: Target directory path
  * - filename: File to process (or "ALL" for full sync)
  * - operation: Type of operation ("FULL", "ADDED", "MODIFIED", "DELETED")
  *
  * The worker communicates its results back to the manager by writing
  * a formatted execution report to stdout.
  *
  * @param argc Argument count
  * @param argv Argument vector
  * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
  */
 int main(int argc, char *argv[]) {
     /* Validate arguments */
     if (argc != 5) {
         fprintf(stderr, "Usage: %s <source_dir> <target_dir> <filename> <operation>\n", argv[0]);
         return EXIT_FAILURE;
     }
     
     /* Parse arguments */
     const char *source_dir = argv[1];
     const char *target_dir = argv[2];
     const char *filename = argv[3];
     const char *operation = argv[4];
     
     /* Perform the requested operation */
     if (strcmp(operation, "FULL") == 0) {
         /* Perform full directory synchronization */
         full_sync(source_dir, target_dir);
     } else if (strcmp(operation, "ADDED") == 0 || strcmp(operation, "MODIFIED") == 0) {
         /* Copy a single file (new or modified) */
         char source_path[PATH_MAX], target_path[PATH_MAX];
         snprintf(source_path, PATH_MAX, "%s/%s", source_dir, filename);
         snprintf(target_path, PATH_MAX, "%s/%s", target_dir, filename);
         
         copy_file(source_path, target_path);
         printf("EXEC_REPORT_START\n");
         printf("STATUS: SUCCESS\n");
         printf("DETAILS: File %s was copied\n", filename);
         printf("EXEC_REPORT_END\n");
     } else if (strcmp(operation, "DELETED") == 0) {
         /* Delete a file */
         char target_path[PATH_MAX];
         snprintf(target_path, PATH_MAX, "%s/%s", target_dir, filename);
         
         delete_file(target_path);
         printf("EXEC_REPORT_START\n");
         printf("STATUS: SUCCESS\n");
         printf("DETAILS: File %s was deleted\n", filename);
         printf("EXEC_REPORT_END\n");
     } else {
         /* Unknown operation */
         fprintf(stderr, "Unknown operation: %s\n", operation);
         printf("EXEC_REPORT_START\n");
         printf("STATUS: ERROR\n");
         printf("DETAILS: Unknown operation %s\n", operation);
         printf("EXEC_REPORT_END\n");
         return EXIT_FAILURE;
     }
     
     return EXIT_SUCCESS;
 }