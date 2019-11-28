#include "vpdecc.h"

#include <string.h>

int vpdecc_create_ecc(const unsigned char* data, size_t data_length,
                      unsigned char* ecc, size_t* ecc_buffersize)
{
    int i, vRet = 0;

    memset(ecc, 0, *ecc_buffersize);

    return vRet;
}

int vpdecc_check_data(unsigned char* data, size_t data_length,
                      const unsigned char* ecc, size_t ecc_length)
{
    int vRet = 0;

    return vRet;
}
