#include "../../include/includes.h"
#include "../../include/utils.h"

#include "dynamic_files.h"

#define MAX 5

struct filename_array* init_filename_array(void)
{
    struct filename_array* ret = calloc(1, sizeof *ret);
    if (ret) {
        ret->filenames = calloc(MAX, sizeof(void*));
        ret->max_alloced = MAX;
        ret->count = 0;
    }

    return ret;
}

void insert_in_array(struct filename_array* arr, char* filename, size_t size)
{
    if (!arr || !arr->filenames)
        return;

    size_t* idx = &arr->count;
    arr->filenames[(*idx)++] = strndup(filename, size);
    
    if (*idx == arr->max_alloced) {
        arr->max_alloced += MAX;
        arr->filenames = realloc(arr->filenames, arr->max_alloced);
    }
}

void dealloc_filename_array(struct filename_array* ptr)
{
    if (!ptr) return;

    if (ptr->filenames) {
        for (size_t i = 0; i < ptr->count; i++)
            free(ptr->filenames[i]);
        free(ptr->filenames);
    }

    free(ptr);
}

