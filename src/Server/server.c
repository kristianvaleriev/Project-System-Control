#include "../../include/includes.h"
#include "../../include/utils.h"

char* program_name = 0;

int main(int argc, char** argv)
{
    get_program_name(argv[0]);

    info_msg("test");
}
