
#include "vpdecc_support.h"

#include <string.h>

/******************************************************************************/
/* seepromGetEcc                                                              */
/*                                                                            */
/* Calculates the 7 bit ECC code of a 32 bit data word and returns it         */
/*                                                                            */
/******************************************************************************/
inline unsigned char seepromGetEcc(const unsigned char* data)
{
    unsigned char vResult = 0x00;
    return vResult;
}

/******************************************************************************/
/*                                                                            */
/******************************************************************************/
int seepromScramble(const int bitOffset, const unsigned char* cleanData,
                    size_t cleanSize, unsigned char* scrambledData,
                    size_t scrambledSize)
{
    int vRet = 0;
    return vRet;
}

/******************************************************************************/
/*                                                                            */
/******************************************************************************/
int seepromUnscramble(const int bitOffset, const unsigned char* scrambledData,
                      size_t scrambledSize, unsigned char* cleanData,
                      size_t cleanSize)
{
    int vRet = 0;
    return vRet;
}

/******************************************************************************/
/* seepromGenCsDecode                                                         */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
void seepromGenCsDecode(const unsigned char numBits,
                        const unsigned char syndrom,
                        const unsigned char* csdSyndroms,
                        unsigned char* vResult)
{
}

/******************************************************************************/
/* seepromGenerateCheckSyndromDecode                                          */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
void seepromGenerateCheckSyndromDecode(const unsigned char checkSyndrom,
                                       unsigned char* csdData,
                                       unsigned char* csdEcc)
{
}

/******************************************************************************/
/* seepromEccCheck                                                            */
/*                                                                            */
/* Checks the data integrety and correct it if possible                       */
/*                                                                            */
/******************************************************************************/

int seepromEccCheck(unsigned char* vData, unsigned char* vEcc,
                    size_t numOfWords)
{
    int vRet = 0;
    return vRet;
}
