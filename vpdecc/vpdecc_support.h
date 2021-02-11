/******************************************************************************
 *
 * IBM Confidential
 *
 * Licensed Internal Code Source Materials
 *
 * IBM Flexible Support Processor Licensed Internal Code
 *
 * (c) Copyright IBM Corp. 2004
 *
 * The source code is for this program is not published or otherwise divested
 * of its trade secrets, irrespective of what has been deposited with the
 * U.S. Copyright Office.
 *
 *****************************************************************************/


#ifndef _VPDECC_SUPPORT_H_
#define _VPDECC_SUPPORT_H_

#include "vpdecc.h"

#define VPD_ECC_DATA_BIT_OFFSET  11
#define VPD_ECC_ECC_BIT_OFFSET   11

#ifdef __cplusplus
   extern "C" {
#endif

/******************************************************************************/
/* seepromGetEcc                                                              */
/*                                                                            */
/* Calculates the 7 bit ECC code of a 32 bit data word and returns it         */
/* @param data Data Byte                                                      */
/* @return 7 Bit ECC for the Data Byte                                        */
/*                                                                            */
/******************************************************************************/
unsigned char seepromGetEcc(const unsigned char* data);


/******************************************************************************/
/* seepromScramble                                                            */
/*                                                                            */
/* scrambles each Bit of the buffer "dataByte" into the Buffer "dataWord" .   */
/* The neighbour Bits of "dataWord" have had the distance "bitOffset"         */
/* before they were scrambled. Both buffers are seen as an continous stream   */
/* of Bits. The scramble routine is done for the n = bitsInWord most right    */
/* Bits of each "numOfWord" "dataWord".                                       */
/* @param bitOffset Distance of scrambled Bits before scambling               */
/* @param bitsInWord Number of used Bits of each dataWord                     */
/* @param dataByte  In-buffer of Byte organized Bits to be scrambled          */
/* @param numOfByte Size of in-buffer                                         */
/* @param dataWord  Out-Buffer of Word organized Bits after scrambling        */
/* @param numOfWord Size of out-buffer                                        */
/* @return none                                                               */
/******************************************************************************/

int seepromScramble(const int   bitOffset,
                     const unsigned char* cleanData,
                     size_t cleanSize,
                     unsigned char* scrambledData,
                     size_t scrambledSize);

/******************************************************************************/
/* seepromUnscramble                                                          */
/*                                                                            */
/* scrambles each Bit of the buffer "dataWord" into the Buffer "dataBytes".   */
/* The neighbour Bits of "dataWord" will have the discance "bitOffset"        */
/* when they are scrambled back to the out-buffer "dataByte"                  */
/* Both buffers are seen as an continous stream of Bits                       */
/* The scramble routine is done for the n = bitsInWord most right Bits        */
/* of each "numOfWord" "dataWord".                                            */
/* @param bitOffset Distance of scrambled Bits after scrambling               */
/* @param bitsInWord Number of used Bits of each dataWord                     */
/* @param dataWord  In-buffer of Word organized Bits to be scrambled          */
/* @param numOfByte Size of in-buffer                                         */
/* @param dataWord  Out-Buffer of Byte organized Bits after scrambling        */
/* @param numOfWord Size of out-buffer                                        */
/* @return none                                                               */
/******************************************************************************/
int seepromUnscramble(const int   bitOffset,
                        const unsigned char* scrambledData,
                        size_t scrambledSize,
                        unsigned char* cleanData,
                        size_t cleanSize);

/******************************************************************************/
/* seepromGenCsDecode                                                         */
/*                                                                            */
/* genrates Position of Bit to be corrected based on syndorm and array of     */
/* valid syndroms.                                                            */
/* @param numBits Number of compares done for syndrom in the array of         */
/*                csdSyndroms                                                 */
/* @param syndrom Bitpattern describing the positon of Bit to be corrected    */
/* @param csdSyndrom Array of Bitpatterns each representing a correctable Bit */
/*                   error. The position in the Array is pointing to the Bit  */
/*                   possition in the code word                               */
/* @return Bit position of the Bit to be corrected                            */
/*         if 0x0 syndrom not found in csdSyndroms => uncorrectable           */
/******************************************************************************/
void seepromGenCsDecode(const unsigned char  numBits,
                                 const unsigned char  syndrom,
                                 const unsigned char* csdSyndroms,
                                       unsigned char* vResult);


/******************************************************************************/
/* seepromGenerateCheckSyndromDecode                                          */
/*                                                                            */
/*                                                                            */
/* check if "checkSyndrom" is a valid syndrom for the data of ECC word.       */
/* @param checkSyndrom syndrom to be checked                                  */
/* @param csdData Bit position of Bit to be corrected if valid data syndrom   */
/* @param csdEcc  Bit position of Bit to be corrected if valid ECC syndrom    */
/* @return none                                                               */
/******************************************************************************/
void seepromGenerateCheckSyndromDecode(const unsigned char  checkSyndrom,
                                             unsigned char* csdData,
                                             unsigned char* csdEcc);


/******************************************************************************/
/* seepromEccCheck                                                            */
/*                                                                            */
/* Checks the data integrety and correct it if possible                       */
/* @param userWord In-Buffer containing the user data                         */
/* @param eccWord  In-Buffer containing the ecc data for the related userWord */
/* @param numOfWord Length of both In-Buffers                                 */
/* @return Error returncode                                                   */
/******************************************************************************/
int seepromEccCheck(unsigned char* vData, unsigned char* vEcc, size_t numOfDataBytes);


/******************************************************************************/
/* seepromCheckData  (for 2K SEEPROMs only)                                   */
/*                                                                            */
/* Checks a page (256 bytes) of a 2K SEEPROM and correct it if possible       */
/* @param seepromData In-Buffer containing the raw data of the SEEPROM        */
/*                   Out-Buffer containing the corrected user data if possible*/
/* @return Error returncode                                                   */
/******************************************************************************/
/*int seepromCheckData(unsigned char* seepromData);*/


/******************************************************************************/
/* seepromCreateEcc (for 2K SEEPROMs only)                                    */
/*                                                                            */
/* create for the first 208 bytes user data, 48 ECC bytes and append it       */
/* seepromData is a 256 byte buffer, which contains in the first 208 bytes    */
/* the user data. The last 48 bytes contains the ECC after returning the      */
/* function                                                                   */
/* @param seepromData In-Buffer containing the user data                      */
/*                   Out-Buffer containing the user data with the appended ECC*/
/* @return Error returncode                                                   */
/******************************************************************************/
/*int seepromCreateEcc(unsigned char* seepromData);*/




#ifdef __cplusplus
   } /* end extern "C" */
#endif

#endif  /* endif _PXESCSEEPROMSUPPORT_H_ */
