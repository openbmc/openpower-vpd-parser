#include "vpdecc.h"

/******************************************************************************/
unsigned char seepromGetEcc(const unsigned char* data);

/******************************************************************************/
/* seepromScramble                                                            */
/******************************************************************************/

int seepromScramble(const int bitOffset, const unsigned char* cleanData,
                    size_t cleanSize, unsigned char* scrambledData,
                    size_t scrambledSize);

/******************************************************************************/
/******************************************************************************/
int seepromUnscramble(const int bitOffset, const unsigned char* scrambledData,
                      size_t scrambledSize, unsigned char* cleanData,
                      size_t cleanSize);

/******************************************************************************/
/******************************************************************************/
void seepromGenCsDecode(const unsigned char numBits,
                        const unsigned char syndrom,
                        const unsigned char* csdSyndroms,
                        unsigned char* vResult);

/******************************************************************************/
/* seepromGenerateCheckSyndromDecode                                          */
/******************************************************************************/
void seepromGenerateCheckSyndromDecode(const unsigned char checkSyndrom,
                                       unsigned char* csdData,
                                       unsigned char* csdEcc);

/******************************************************************************/
/******************************************************************************/
int seepromEccCheck(unsigned char* vData, unsigned char* vEcc,
                    size_t numOfDataBytes);

/******************************************************************************/
/******************************************************************************/
/*int seepromCheckData(unsigned char* seepromData);*/

/******************************************************************************/
/******************************************************************************/
/*int seepromCreateEcc(unsigned char* seepromData);*/
