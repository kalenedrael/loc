#ifndef _FILE_H_
#define _FILE_H_

void *file_read(const char *filename, ssize_t *out);
void file_free(void *file_data);

#endif /* _FILE_H */
