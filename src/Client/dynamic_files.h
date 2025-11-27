#ifndef _DYNAMIC_FILES_H_
#define _DYNAMIC_FILES_H_

#include <stdlib.h>

struct filename_array {
    char** filenames;
    size_t max_alloced;
    size_t count;
};

struct filename_array* init_filename_array(void);
void dealloc_filename_array(struct filename_array* ptr);
void insert_in_array(struct filename_array* arr, char* filename, size_t size);

#endif
