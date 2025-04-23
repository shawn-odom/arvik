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
void create_archive(char * archive_name, char ** members, int member_count, int verbose);
void extract_archive(char * archive_name, int verbose, int validate);
void list_archive( char * archive_name, int verbose, int validate);
int check_archive_tag(int fd);
void write_header(int archive_fd, char * filename);
void write_footer(int archive_fd, uLong crc);
void extract_file(int archive_fd, arvik_header_t header, int verbose, int validate);
void process_archive(int archive_fd, int verbose, int validate, void (*process_func)(int, arvik_header_t, int, int));
void print_file_info (int archive_fd, arvik_header_t header, int verbose, int validate);
void process_file(int archive_fd, arvik_header_t header, int verbose, int validate, int extract);


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

    if (cflag)
    {
        // Creat Archive. Get file members from command line arguements
        char ** members = &argv[optind];
        int member_count = argc - optind;
        create_archive(archive_name, members, member_count, vflag);
    }
    else if (tflag)
    {
        // List Table of Contents
        list_archive(archive_name, vflag, Vflag);
    }
    else if (xflag)
    {
        extract_archive(archive_name, vflag, Vflag);
    }

    return EXIT_SUCCESS;
}

// Display help for the program
void show_help(void)
{
    printf("Usage: arvik <options> [-f archive-file] [member [...]]\n");
    printf("Options:\n");
    printf("  -c        Create an arvik style archive file\n");
    printf("  -t        Table of contents\n");
    printf("  -x        Extract members from arvik file\n");
    printf("  -f file   Specify the name of the arvik file on which to operate\n");
    printf("  -h        Show this help text and exit\n");
    printf("  -v        Verbose processing\n");
    printf("  -V        Validate the crc value for the data for each archive member\n");
}

// Create new archive file
void create_archive(char * archive_name, char ** members, int member_count, int verbose)
{
    int archive_fd;
    mode_t old_mask;

    // Open the archive file or use stdout if no file is specified
    if (archive_name == NULL)
    {
        archive_fd = STDOUT_FILENO;
    }
    else
    {
        old_mask = umask(0); // Clear umask temp
        archive_fd = open(archive_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        umask(old_mask); // Restore original umask

        if (archive_fd < 0)
        {
            perror("Error opening archive file for writing");
            exit(1);
        }
    }

    if (write(archive_fd, ARVIK_TAG, strlen(ARVIK_TAG)) != (ssize_t)strlen(ARVIK_TAG))
    {
        perror("Error writing archive tag");
        close(archive_fd);
        exit(1);
    }

    // Process each member file given
    for (int i = 0; i < member_count; ++i)
    {
        struct stat st; // File Statistics
        int member_fd; // File descriptor for the memory file
        char buffer[4096]; // Buffer for reading file data
        ssize_t bytes_read; // Number of bytes read
        uLong crc = crc32(0L, Z_NULL, 0); // Initialize CRC

        // Open member file
        member_fd = open(members[i], O_RDONLY);
        if (member_fd < 0)
        {
            fprintf(stderr, "Error opening member file %s: %s\n", members[i], strerror(errno));
            continue;
        }

        // Get file information using fstat
        if (fstat(member_fd, &st) < 0)
        {
            fprintf(stderr, "Error getting file information for %s: %s\n", members[i], strerror(errno));
            close(member_fd);
            continue;
        }

        write_header(archive_fd, members[i]);

        if (verbose)
        {
            printf("a - %s\n", members[i]);
        }

        // Copy file data and calculate CRC
        while ((bytes_read = read(member_fd, buffer, sizeof(buffer))) > 0)
        {
            // Update CRC for this chunk of data
            crc = crc32(crc, (const Bytef*) buffer, bytes_read);
            
            if (write(archive_fd, buffer, bytes_read) != bytes_read)
            {
                fprintf(stderr, "Error writing data for %s: %s\n", members[i], strerror(errno));
                break;
            }
        }

        // Write footer with CRC
        write_footer(archive_fd, crc);

        // Close member file
        close(member_fd);
    }

    // Close archive file if it's not stdout
    if (archive_fd != STDOUT_FILENO)
    {
        close(archive_fd);
    }
}

// Write file header to archive
void write_header(int archive_fd, char * filename)
{
    arvik_header_t header; // Header struct
    struct stat st; // File statistics

    // Initialize header with zeros
    memset(&header, 0, sizeof(header));

    // Get file info
    if (stat(filename, &st) < 0)
    {  
        perror("Error getting file information");
        return;
    }

    // Fill in header fields
    strncpy(header.arvik_name, filename, sizeof(header.arvik_name) -1);
    sprintf(header.arvik_date, "%ld", st.st_mtime); // Convert modification time to string
    sprintf(header.arvik_uid, "%d", st.st_uid);  // Convert UID to string
    sprintf(header.arvik_gid, "%d", st.st_gid); // Convert GID to string
    sprintf(header.arvik_mode, "%o", st.st_mode); // Convert mode to octal string
    sprintf(header.arvik_size, "%ld", st.st_size); // Convert size to string

    // Set terminator
    header.arvik_term[0] = ARVIK_TERM[0];
    header.arvik_term[1] = ARVIK_TERM[0];

    // Write the header to the archive
    if (write(archive_fd, &header, sizeof(header)) != sizeof(header))
    {
        perror("Error writing header");
    }
}

// Write file footer to archive
void write_footer(int archive_fd, uLong crc)
{
    arvik_footer_t footer;
    
    // Init footer with zeros
    memset(&footer, 0, sizeof(footer));

    // Fill in footer fields
    sprintf(footer.arvik_data_crc, "0x%0lx", crc); // Convert CRC to hex string

    // Set Terminator
    footer.arvik_term[0] = ARVIK_TERM[0];
    footer.arvik_term[1] = ARVIK_TERM[0];

    // Write footer to archive
    if (write(archive_fd, &footer, sizeof(footer)) != sizeof(footer))
    {
        perror("Error writing footer");
    }
}

// Extract files from archive
void extract_archive(char * archive_name, int verbose, int validate)
{
    int archive_fd; // File descriptor for the archive file

    if (archive_name == NULL)
    {
        archive_fd = STDIN_FILENO; // Use standard input
    }
    else
    {
        archive_fd = open(archive_name, O_RDONLY);
        if (archive_fd < 0)
        {
            perror("Error opening archive file for reading");
            exit(1);
        }
    }
    
    // Check if the file has correct tag
    if (!check_archive_tag(archive_fd))
    {
        fprintf(stderr, "Error: Not a valid arvik archive file\n");
        close(archive_fd);
        exit(BAD_TAG);
    }

    process_archive(archive_fd, verbose, validate, extract_file);

    // Close file if not stdin
    if (archive_fd != STDIN_FILENO)
    {
        close (archive_fd);
    }
}

// Extract single file from archive
void extract_file(int archive_fd, arvik_header_t header, int verbose, int validate)
{
    int file_fd; // File descriptor for the extracted file
    size_t file_size; //size of file
    char buffer[4096]; //buffer for reading file data
    size_t remaining; // remaining bytes to read
    ssize_t bytes_read; //number of bytes read in one op
    uLong crc = crc32(0L, Z_NULL, 0); //init CRC
    arvik_footer_t footer; // Footer struct
    struct timespec times[2]; // For setting file times
    mode_t mode;    // file mode

    // convert size string to num
    file_size = strtol(header.arvik_size, NULL, 10);

    // open output file
    file_fd = open(header.arvik_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0)
    {
        fprintf(stderr, "Error creating file %s: %s\n", header.arvik_name, strerror(errno));

        // skip file data
        if (lseek(archive_fd, file_size, SEEK_CUR) < 0)
        {
            perror("Error skipping file data");
            exit(1);
        }

        // skip footer
        if (lseek(archive_fd, sizeof(arvik_footer_t), SEEK_CUR) < 0)
        {
            perror("Error skipping footer");
            exit(1);
        }
        return;
    }

    // Verbose check
    if (verbose)
    {
        printf("x - %s\n", header.arvik_name);
    }

    // Copy file data
    remaining = file_size;
    while (remaining > 0)
    {
        size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        bytes_read = read(archive_fd, buffer, to_read);

        if (bytes_read <= 0)
        {
            fprintf(stderr, "Error reading file data for %s: %s\n", header.arvik_name, strerror(errno));
            break;
        }

        // update CRC
        crc = crc32(crc, (const Bytef*) buffer, bytes_read);

        // write data to output file
        if (write(file_fd, buffer, bytes_read) != bytes_read)
        {
            fprintf(stderr, "Error writing to file %s: %s\n", header.arvik_name, strerror(errno));
            break;
        }

        remaining -= bytes_read;
    }

    // read footer
    if (read(archive_fd, &footer, sizeof(footer)) != sizeof(footer))
    {
        fprintf(stderr, "Error reading footer for %s: %s\n", header.arvik_name, strerror(errno));
        close (file_fd);
        return;
    }

    // validate CRC if req
    if (validate)
    {
        uLong stored_crc;
        sscanf(footer.arvik_data_crc, "0x%lx", &stored_crc);

        if (crc != stored_crc)
        {
            fprintf(stderr, "CRC check failed for %s\n", header.arvik_name);
        }
    }

    // set file perms
    mode = strtol(header.arvik_mode, NULL, 8); // convert octal str to num
    if (fchmod(file_fd, mode) < 0)
    {
        fprintf(stderr, "Error setting permissions for %s: %s\n", header.arvik_name, strerror(errno));
    }

    // set file times
    times[0].tv_sec = times[1].tv_sec = strtol(header.arvik_date, NULL, 10);
    times[0].tv_nsec = times[1].tv_nsec = 0;

    if (futimens(file_fd, times) < 0)
    {
        fprintf(stderr, "Error setting timestamps for %s: %s\n", header.arvik_name, strerror(errno));
    }

    // close output
    close(file_fd);
}


// List contents of an archive
void list_archive(char * archive_name, int verbose, int validate)
{
    int archive_fd;

    if (archive_name == NULL)
    {
        archive_fd = STDIN_FILENO;
    }
    else
    {
        archive_fd = open(archive_name, O_RDONLY);
        if (archive_fd < 0)
        {
            perror("Error opening archive file for reading");
            exit(1);
        }
    }

    // check if file has correct tag
    if (!check_archive_tag(archive_fd))
    {
        fprintf(stderr, "Error: Not a valid arvik archive file\n");
        close(archive_fd);
        exit(BAD_TAG);
    }

    process_archive(archive_fd, verbose, validate, print_file_info);

    // close if not stdin
    if (archive_fd != STDIN_FILENO)
    {
        close (archive_fd);
    }
}


// Print information about a file in the archive
void print_file_info(int archive_fd, arvik_header_t header, int verbose, int validate)
{
    size_t file_size;
    time_t mtime;
    struct tm *tm_info;
    char time_str[32];
    mode_t mode;
    char mode_str[11];
    
    (void) archive_fd;
    (void) validate;

    // if verbose
    if (verbose)
    {
        file_size = strtol(header.arvik_size, NULL, 10);
    
        // conert time string to time_t 
        mtime = strtol(header.arvik_date, NULL, 10);
        tm_info = localtime(&mtime);
        strftime(time_str, sizeof(time_str), "%b %e %R %Y", tm_info);

        // convert mode str to mode_t
        mode = strtol(header.arvik_mode, NULL, 8);

        // format mode string
         mode_str[0] = S_ISDIR(mode) ? 'd' : '-';
        mode_str[1] = (mode & S_IRUSR) ? 'r' : '-';
        mode_str[2] = (mode & S_IWUSR) ? 'w' : '-';
        mode_str[3] = (mode & S_IXUSR) ? 'x' : '-';
        mode_str[4] = (mode & S_IRGRP) ? 'r' : '-';
        mode_str[5] = (mode & S_IWGRP) ? 'w' : '-';
        mode_str[6] = (mode & S_IXGRP) ? 'x' : '-';
        mode_str[7] = (mode & S_IROTH) ? 'r' : '-';
        mode_str[8] = (mode & S_IWOTH) ? 'w' : '-';
        mode_str[9] = (mode & S_IXOTH) ? 'x' : '-';
        mode_str[10] = '\0';

        printf("%s %8s/%s %8ld %s %s\n", mode_str, header.arvik_uid, header.arvik_gid, file_size, time_str, header.arvik_name);
    }
    else
    {
        // non verbose
        printf("%s\n", header.arvik_name);
    }
}

// Check if file has correct arvik tag
int check_archive_tag(int fd)
{
    char tag[sizeof(ARVIK_TAG)]; // buffer to read the tag

    if (read(fd, tag, strlen(ARVIK_TAG)) != (ssize_t)strlen(ARVIK_TAG))
    {
        return 0; //error
    }

    // check if tag matches
    return (memcmp(tag, ARVIK_TAG, strlen(ARVIK_TAG)) == 0);
}

// proccess archive file and call function for each member
void process_archive(int archive_fd, int verbose, int validate, void(*process_func)(int, arvik_header_t, int, int))
{
    arvik_header_t header;
    (void) validate;

    // process each file in archive
    while (read(archive_fd, &header, sizeof(header)) == sizeof(header))
    {
        if(header.arvik_term[0] != ARVIK_TERM[0] || header.arvik_term[1] != ARVIK_TERM[0])
        {
            fprintf(stderr, "Error: Invalid header terminator\n");
            break;
        }

        process_func(archive_fd, header, verbose, validate);
    }
}

// process file in archive during extraction or listing
void process_file(int archive_fd, arvik_header_t header, int verbose, int validate, int extract)
{
    size_t file_size;
    if (extract)
    {
        extract_file(archive_fd, header, verbose, validate);
    }
    else
    {
        print_file_info(archive_fd, header, verbose, validate);
        
        file_size = strtol(header.arvik_size, NULL, 10);
        if (lseek(archive_fd, file_size + sizeof(arvik_footer_t), SEEK_CUR) < 0)
        {
            perror("Error skipping file data and footer");
            exit(1);
        }
    }
}
