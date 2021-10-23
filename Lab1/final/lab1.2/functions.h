#if !defined(FUNCTIONS_H)
#define FUNCTIONS_H

// Writes buffer data into outfile
// int fd: file to write buffer into
// const char * buff: data to write into the file
// int len: size of buffer
void doWrite(int fd, const char *buff, int len);

// Opens infile and writes it's data into outfile using doWrite()
// int fd: file to write infile's data into
// const char * infile: filename
void write_file(int fd, const char *infile);

// Checks if a file exists.
// const char *infile: filename
int check_file(const char *infile);

#endif  // FUNCTIONS_H
