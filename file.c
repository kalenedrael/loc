/** @file file.c
 *  @brief File reading stuff
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"

/** @brief Reads a file into memory
 *  @param filename Name of file to read
 *  @param size_out Output; number of bytes read
 *  @return Pointer to file data, or NULL on failure
 */
void *file_read(const char *filename, ssize_t *size_out)
{
	void *file_data = NULL;

	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "%s: error opening file: %s\n", filename, strerror(errno));
		return NULL;
	}

	struct stat stats;
	if (fstat(fd, &stats) == -1) {
		fprintf(stderr, "%s: cannot stat file: %s\n", filename, strerror(errno));
		goto out;
	}

	ssize_t size = *size_out = stats.st_size;
	file_data = malloc(size);
	if (file_data == NULL) {
		fprintf(stderr, "%s: cannot allocate enough space to read\n", filename);
		goto out;
	}

	ssize_t ret = read(fd, file_data, size);
	if (ret != size) {
		fprintf(stderr, "%s: cannot read entire file\n", filename);
		free(file_data);
		file_data = NULL;
	}

out:
	close(fd);
	return file_data;
}

/** @brief Frees file data returned by `file_read`
 *  @param file_data File data to free
 */
void file_free(void *file_data)
{
	free(file_data);
}
