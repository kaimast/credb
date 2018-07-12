#include "FakeEnclave.h"

#ifdef FAKE_ENCLAVE
int get_file_size(int32_t *out, const char *filename)
{
    *out = get_file_size(filename);
    return 0;
}

int get_num_files(size_t *out)
{
    *out = get_num_files();
    return 0;
}

int get_total_file_size(size_t *out)
{
    *out = get_total_file_size();
    return 0;
}

int read_from_disk(bool *result, const char *filename, uint8_t *data, uint32_t length)
{
    *result = read_from_disk(filename, data, length);
    return 0;
}

int write_to_disk(bool *res, const char *filename, const uint8_t *data, uint32_t length)
{
    *res = write_to_disk(filename, data, length);
    return 0;
}
#endif
