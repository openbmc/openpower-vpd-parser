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

static const char copyright [] __attribute__((unused)) =
	"Licensed Internal Code - Property of IBM\n"
	"IBM Flexible Support Processor Licensed Internal Code\n"
	"(c) Copyright IBM Corp 2004 All Rights Reserved\n"
	"US Government Users Restricted Rights - Use, duplication\n"
	"or disclosure restricted by GSA ADP Schedule Contract\n"
	"with IBM Corp.";


#include <string.h>
#include "vpdecc.h"
#include "vpdecc_support.h"

int vpdecc_create_ecc(const unsigned char* data, size_t data_length,
                      unsigned char* ecc, size_t* ecc_buffersize)
{
   size_t i;
   int vPosition,vRet = 0;
   /********************************************/
   /* to make sure that it is accessable       */
   /* with a block size of 44 bytes            */
   /* 4Bytes ECC <-> 11 bit scrambling         */
   /********************************************/
   size_t vBlocks       = (data_length+3)/4;
   size_t vBufferLength = vBlocks*4;
   size_t vEccLength    = vBlocks;

   
   unsigned char* pRawDataBuffer  = NULL;
   unsigned char* pDataBuffer  = NULL;
   unsigned char* pEccBuffer  = NULL;
   
   /********************************************/
   /* check if the buffer delivered by the     */
   /* invoking procedure is large enough       */
   /* to store the ecc                         */
   /********************************************/

   if(vEccLength > *ecc_buffersize)
   {
      return VPD_ECC_NOT_ENOUGH_BUFFER;
   }
   
   pRawDataBuffer = malloc(vBufferLength);
   pDataBuffer = malloc(vBufferLength);
   pEccBuffer = malloc(vEccLength);
   
   /***********************************************************************************/
   /* first of all the data will be scrabled for a higher probability to be able to   */
   /* correct the errors. The bits used for one ecc-word are ditributed over the      */
   /* whole memory. Therefore it does not matter if a specific byte is not readable   */
   /***********************************************************************************/

   memset(pRawDataBuffer,0x00,vBufferLength);
   memset(pDataBuffer,0x00,vBufferLength);

   memcpy(pRawDataBuffer,data,data_length);
   
   seepromScramble(VPD_ECC_DATA_BIT_OFFSET, pRawDataBuffer, vBufferLength, 
                     pDataBuffer, vBufferLength);

   for (i=0; i < vEccLength; i++)
   {
      vPosition = i*4;
      pEccBuffer[i] = seepromGetEcc(&pDataBuffer[vPosition]);
   }
   seepromUnscramble(VPD_ECC_ECC_BIT_OFFSET, pEccBuffer, vEccLength, ecc, vEccLength);
   
   *ecc_buffersize = vEccLength;
    
   free (pRawDataBuffer);
   free (pDataBuffer);
   free (pEccBuffer);
   return vRet;
}

int vpdecc_check_data(unsigned char* data, size_t data_length,
                      const unsigned char* ecc, size_t ecc_length)
{
   int vRet = 0;

   /********************************************/
   /* to make sure that it is accessable       */
   /* with a block size of 44 bytes            */
   /* 4Bytes ECC <-> 11 bit scrambling         */
   /********************************************/
   size_t vBlocks       = (data_length+3)/4;
   size_t vBufferLength = vBlocks*4;
   size_t vEccLength    = vBlocks;

   
   unsigned char* pRawDataBuffer  = NULL;
   unsigned char* pDataBuffer  = NULL;
   unsigned char* pEccBuffer  = NULL;

   if(vEccLength > ecc_length)
   {
      return VPD_ECC_WRONG_ECC_SIZE;
   }
   
   
   pRawDataBuffer = (unsigned char*)malloc(vBufferLength);
   pDataBuffer = (unsigned char*)malloc(vBufferLength);
   pEccBuffer  = (unsigned char*)malloc(vEccLength);

   memset(pRawDataBuffer,0x00,vBufferLength);
   memset(pDataBuffer,0x00,vBufferLength);

   memcpy(pRawDataBuffer,data,data_length);
         
   seepromScramble(VPD_ECC_DATA_BIT_OFFSET, pRawDataBuffer, vBufferLength,
                     pDataBuffer, vBufferLength);


   seepromScramble(VPD_ECC_ECC_BIT_OFFSET, ecc, vEccLength,
                     pEccBuffer, vEccLength);


   vRet = seepromEccCheck(pDataBuffer, pEccBuffer, vEccLength);

   if(vRet == VPD_ECC_CORRECTABLE_DATA)
   {
      seepromUnscramble(VPD_ECC_DATA_BIT_OFFSET, pDataBuffer, vBufferLength,
                     pRawDataBuffer,vBufferLength);
   }
   memcpy(data,pRawDataBuffer,data_length);

   free (pRawDataBuffer);
   free (pDataBuffer);
   free (pEccBuffer);

   return vRet;
}

