#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <zlib.h>

#include "arvik.h"

void show_help(void);
int create_archive(const char * archive_name, char ** members, int member_count, int verbose);
int extract_archive(const char * archive_name, int verbose, int validate);
int list_archive(const char * archive_name, int verbose, int validate);
int check_archive_tag(int fd);
int write_header(int archive_fd, const char * filename);
int write_footer(int archive_fd, uLong crc);
int extract_file(int archive_fd, arvik_header_t header, int verbose, int validate);
void process_archive(int archive_fd, int verbose, int validate, void (*process_func)(int, arvik_header_t, int, int));
void print_file_info (arvik_header_t header, int verbose);


int main(int argc, char * argv[]) 
{
    int opt; //Option char for getop
    int cflag = 0; //Flag for the create archive option
    int tflag = 0; //Flag for the table of contents option
    int xflag = 0; //Flag for extract option
    int vflag = 0; //Flag for verbose output option
    int Vflag = 0; //Flag for validation
    char * archive_name = NULL; //Name of the archive file

    //Process the command line options using getopt
    while ((opt = getopt(argc, argv, "ctxf:hvV")) != -1)
    {
        switch(opt)
        {
            case 'c': //Create archive
                cflag = 1;
                break;
            case 't': //Table of contents
                tflag = 1;
                break;
            case 'x': // Extract Files
                xflag = 1;
                break;
            case 'f': // Specify archive file
                archive_name = optarg;
                break;
            case 'h': // Show help
                show_help();
                exit(0);
                break;
            case 'v': // Verbose output
                vflag = 1;
                break;
            case 'V': // Validate CRC
                Vflag = 1;
                break;
            default: // Invalid option
                fprintf(stderr, "Invalid command line option\n");
                exit(INVALID_CMD_OPTION);
        }
    }


    return EXIT_SUCCESS;
}
