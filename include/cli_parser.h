/**
 * @file cli_parser.h
 * @brief Header file for command-line argument parsing functionality
 * 
 * This header defines structures and functions for parsing command-line
 * arguments for the File Synchronization System (FSS) manager application.
 */

 #ifndef CLI_PARSER_H
 #define CLI_PARSER_H
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <unistd.h>
 #include <string.h>
 
 /**
  * @struct args
  * @brief Structure to store parsed command-line arguments
  * 
  * This structure holds the values of command-line options parsed from
  * the arguments provided to the FSS manager application.
  */
 struct args {
     char* logfile;     /**< Path to the log file (-l option) */
     char* config_file; /**< Path to the configuration file (-c option) */
     int worker_limit;  /**< Maximum number of worker processes (-n option, default: 5) */
 };
 
 /**
  * @brief Parse command-line arguments for the FSS manager
  * 
  * Parses the command-line arguments and returns a filled args structure.
  * Expected format: ./fss_manager -l <logfile> -c <config_file> [-n <worker_limit>]
  * 
  * @param argc Argument count from main()
  * @param argv Argument vector from main()
  * @return Struct args containing parsed arguments
  */
 struct args parseArgsManager(int argc, char* argv[]);
 
 #endif /* CLI_PARSER_H */