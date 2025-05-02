/**
 * @file cli_parser.c
 * @brief Implementation of command-line argument parsing for FSS manager
 *
 * This file implements functions to parse command-line arguments for the 
 * File Synchronization System (FSS) manager application.
 */

 #include "../include/cli_parser.h"

 /**
  * @brief Parse command-line arguments for the FSS manager
  *
  * Processes the command-line arguments provided to the FSS manager application
  * and returns a structure containing the parsed values. Supports the following
  * options:
  *   -l <logfile>      : Path to the log file (required)
  *   -c <config_file>  : Path to the configuration file (required)
  *   -n <worker_limit> : Maximum number of concurrent worker processes (optional, default: 5)
  *
  * If required options are missing, the function prints usage information and exits.
  *
  * @param argc Argument count from main()
  * @param argv Argument vector from main()
  * @return Struct args containing the parsed arguments
  */
 struct args parseArgsManager(int argc, char* argv[]) {
     /* Initialize return struct with default values */
     struct args ret = { .logfile = NULL, .config_file = NULL, .worker_limit = 5 };
     
     /* Skip program name */
     argv++; 
     argc--;
     
     /* Process all arguments */
     while (argc > 0) {
         if ((*argv)[0] == '-') {
             /* Process -l option (logfile) */
             if (strcmp(*argv, "-l") == 0 && argc > 1) {
                 argv++; 
                 argc--;
                 ret.logfile = *argv;
             } 
             /* Process -c option (config file) */
             else if (strcmp(*argv, "-c") == 0 && argc > 1) {
                 argv++; 
                 argc--;
                 ret.config_file = *argv;
             } 
             /* Process -n option (worker limit) */
             else if (strcmp(*argv, "-n") == 0 && argc > 1) {
                 argv++; 
                 argc--;
                 char* endptr;
                 ret.worker_limit = strtol(*argv, &endptr, 10);
                 
                 /* Validate worker limit is a positive integer */
                 if (*endptr != '\0' || ret.worker_limit <= 0) {
                     fprintf(stderr, "Invalid worker limit: %s\n", *argv);
                     exit(EXIT_FAILURE);
                 }
             } 
             /* Handle unrecognized or incomplete options */
             else {
                 fprintf(stderr, "Unrecognized or incomplete argument: %s\n", *argv);
                 exit(EXIT_FAILURE);
             }
         }
         argv++; 
         argc--;
     }
     
     /* Ensure required arguments are provided */
     if (!ret.logfile || !ret.config_file) {
         fprintf(stderr, "Usage: ./fss_manager -l <logfile> -c <config_file> [-n <worker_limit>]\n");
         exit(EXIT_FAILURE);
     }
     
     return ret;
 }