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

#include <string.h>
#include "vpdecc_support.h"


/******************************************************************************/
/* seepromGetEcc                                                              */
/*                                                                            */
/* Calculates the 7 bit ECC code of a 32 bit data word and returns it         */
/*                                                                            */
/******************************************************************************/
inline unsigned char seepromGetEcc(const unsigned char* data)
{
   static const unsigned char aSyndromMask[] = {   0x07,0xFF,0x80,0xC0,0xFF,0x00,0xA0,0xB4,
                                                   0x39,0x07,0x54,0x6A,0x4A,0x19,0x4A,0x19,
                                                   0x54,0x6A,0x39,0x07,0xA0,0xB4,0xFF,0x00,
                                                   0x80,0xC0,0x07,0xFF
                                                };
   unsigned char vWork[4];
   unsigned char vBuffer = 0x00;
   unsigned char vResult = 0x00;
   unsigned int i;

   vResult = 0;
   for (i=0; i<7; i++)
   {
      vWork[0] = data[0] & aSyndromMask[i*4];
      vWork[1] = data[1] & aSyndromMask[i*4+1];
      vWork[2] = data[2] & aSyndromMask[i*4+2];
      vWork[3] = data[3] & aSyndromMask[i*4+3];

      /* shift by 16 */
      vWork[3] = vWork[3] ^ vWork[1];
      vWork[2] = vWork[2] ^ vWork[0];
      vWork[1] = vWork[1] ^ 0x00;
      vWork[0] = vWork[0] ^ 0x00;

      /* shift by 8 */
      vWork[3] = vWork[3] ^ vWork[2];
      vWork[2] = vWork[2] ^ vWork[1];
      vWork[1] = vWork[1] ^ vWork[0];
      vWork[0] = vWork[0] ^ 0x00;

      /* shift by 4 */
      vWork[3] = vWork[3] ^ ((vWork[2]<<4) | (vWork[3]>>4));
      vWork[2] = vWork[2] ^ ((vWork[1]<<4) | (vWork[2]>>4));
      vWork[1] = vWork[1] ^ ((vWork[0]<<4) | (vWork[1]>>4));
      vWork[0] = vWork[0] ^ (                (vWork[0]>>4));

      /* shift by 2 */
      vWork[3] = vWork[3] ^ ((vWork[2]<<6) | (vWork[3]>>2));
      vWork[2] = vWork[2] ^ ((vWork[1]<<6) | (vWork[2]>>2));
      vWork[1] = vWork[1] ^ ((vWork[0]<<6) | (vWork[1]>>2));
      vWork[0] = vWork[0] ^ (                (vWork[0]>>2));

      /* shift by 1 */
      vWork[3] = vWork[3] ^ ((vWork[2]<<7) | (vWork[3]>>1));
      vWork[2] = vWork[2] ^ ((vWork[1]<<7) | (vWork[2]>>1));
      vWork[1] = vWork[1] ^ ((vWork[0]<<7) | (vWork[1]>>1));
      vWork[0] = vWork[0] ^ (                (vWork[0]>>1));

      
      vBuffer = vWork[3] & 0x01;
      
      vResult = vResult | (vBuffer <<(6-i));
   } /* endfor i */
   vResult = vResult ^ 0x7F;
   return vResult;
}


/******************************************************************************/
/*                                                                            */
/******************************************************************************/
int seepromScramble(const int   bitOffset,
                     const unsigned char* cleanData,
                     size_t cleanSize,
                     unsigned char* scrambledData,
                     size_t scrambledSize)
{
   int   vRet = 0;
/*
   int   i,j;
   int   byteNumber, bitPosition;
   long  scrambledPosition, numOfSteps, numOfBits;   
   char  bitBuffer;

   numOfSteps  = cleanSize * 8 / bitOffset;

   if(cleanSize > scrambledSize)
   {
      return VPD_ECC_WRONG_BUFFER_SIZE;
   }
         
   memset(scrambledData,0x00,scrambledSize);
   for (i=0; i<bitOffset ;i++)
   {
      for (j=0; j<numOfSteps ;j++)
      {                    
         byteNumber =  (j*bitOffset+i)/ 8;
         bitPosition = (j*bitOffset+i)% 8;
         
         scrambledPosition = i*numOfSteps+j;

         bitBuffer = cleanData[byteNumber] & (0x80>>bitPosition);
         if(bitBuffer != 0)
         {
            scrambledData[scrambledPosition/8] = scrambledData[scrambledPosition/8] | (0x80>>(scrambledPosition%8));
         }
      }
   }
*/
   size_t i;
   int j;

   int vBitNum, vStartBit, vMaxBits;
   int vByteNum, vBitInByte;
   unsigned char vInitialBitMask,vBitMask;
   unsigned char vByteBitMask;

   vBitNum   = 0;
   vStartBit = 0;
   vInitialBitMask = 0x80;
   vMaxBits = scrambledSize * 8;

   if(cleanSize > scrambledSize)
   {
      return VPD_ECC_WRONG_BUFFER_SIZE;
   }


   memset(scrambledData, 0x00, scrambledSize );

   for (i=0; i < scrambledSize; i++)
   {
      vBitMask = vInitialBitMask;
      for (j=0; j < 8; j++)
      {
         vByteNum = vBitNum / 8;
         vBitInByte = vBitNum %8 ;

         vByteBitMask = (unsigned char) (0x80 >> vBitInByte);

         if ((cleanData[vByteNum] & vByteBitMask) != 0x00)
         {
            scrambledData[i] = scrambledData[i] | vBitMask;
         } /* endif */

         vBitMask = vBitMask >> 1;
         vBitNum = vBitNum + bitOffset;
         if (vBitNum >= vMaxBits)
         {
            vStartBit++;
            vBitNum = vStartBit;
         } 
      } 
   } 

   return vRet;
}


/******************************************************************************/
/*                                                                            */
/******************************************************************************/
int seepromUnscramble(const int   bitOffset,
                        const unsigned char* scrambledData,
                        size_t scrambledSize,
                        unsigned char* cleanData,
                        size_t cleanSize)
{
   int   vRet = 0;
/*
   int   i,j;
   int   byteNumber, bitPosition;
   long  scrambledPosition, numOfSteps, numOfBits, numOfBytes;
   char  bitBuffer;

   numOfBits  = cleanSize * 8;
   numOfBytes = cleanSize;
   numOfSteps = numOfBits / bitOffset;

   if(cleanSize < scrambledSize)
   {
      return VPD_ECC_WRONG_BUFFER_SIZE;
   }
      
   memset(cleanData,0x00,cleanSize);
   for (i=0; i<bitOffset ;i++)
   {
      for (j=0; j<numOfSteps ;j++)
      {                    
         scrambledPosition = i*numOfSteps+j;
         bitBuffer = scrambledData[scrambledPosition/8] & (0x80>>(scrambledPosition%8));
         
         if(bitBuffer != 0)
         {
            byteNumber =  (j*bitOffset+i)/ 8;
            bitPosition = (j*bitOffset+i)% 8;
         

            cleanData[byteNumber] = cleanData[byteNumber] | (0x80>>bitPosition);
         }
      }
   }
*/

   size_t i;
   long j;
   long vBitNum, vStartBit, vMaxBits;
   long vByteNum, vBitInByte;
   unsigned char vInitialWordBitMask, vWordBitMask;
   unsigned char vByteBitMask;

   vBitNum   = 0;
   vStartBit = 0;
   vInitialWordBitMask = 0x80;
   vMaxBits = scrambledSize * 8;

   if(cleanSize < scrambledSize)
   {
      return VPD_ECC_WRONG_BUFFER_SIZE;
   }

   memset(cleanData, 0x00, cleanSize);

   for (i=0; i < scrambledSize; i++)
   {
      vWordBitMask = vInitialWordBitMask;
      for (j=0; j < 8; j++)
      {
         vByteNum = vBitNum /8;
         vBitInByte = vBitNum %8;

         vByteBitMask = (unsigned char) (0x80 >> vBitInByte);
         if ( (scrambledData[i] & vWordBitMask) != 0)
         {
            cleanData[vByteNum] = (cleanData[vByteNum] | vByteBitMask);
         } /* endif */

         vWordBitMask = vWordBitMask >> 1;
         vBitNum = vBitNum + bitOffset;
         if (vBitNum >= vMaxBits)
         {
            vStartBit++;
            vBitNum = vStartBit;
         }
      } 
   } 

   return vRet;

}

/******************************************************************************/
/* seepromGenCsDecode                                                         */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
void seepromGenCsDecode(const unsigned char  numBits,
                          const unsigned char  syndrom,
                          const unsigned char* csdSyndroms,
                                     unsigned char* vResult)
{
   unsigned char vBitMask[4];
   unsigned char i;
   int bitpos;
   
   i        = 0;

   bitpos = 32- numBits;
   while ( (i == 0) && (bitpos < numBits) )
   {
      vResult[0] = vBitMask[0]=0x00;
      vResult[1] = vBitMask[1]=0x00;
      vResult[2] = vBitMask[2]=0x00;
      vResult[3] = vBitMask[3]=0x00;
      vBitMask[bitpos/8] = (0x80 >> (bitpos%8));
      if (syndrom == csdSyndroms[bitpos])
      {
         vResult[0] = vBitMask[0];
         vResult[1] = vBitMask[1];
         vResult[2] = vBitMask[2];
         vResult[3] = vBitMask[3];
         i++;
      }  /* endif */
      bitpos++;
   } /* endwhile */
}


/******************************************************************************/
/* seepromGenerateCheckSyndromDecode                                          */
/*                                                                            */
/*                                                                            */
/******************************************************************************/
void seepromGenerateCheckSyndromDecode(const unsigned char  checkSyndrom,
                                             unsigned char* csdData,
                                             unsigned char* csdEcc)
{
   unsigned char vEccbytes[4];
   
   static unsigned char aCsdDataSyndroms[] = { 0x23, 0x2C, 0x32, 0x34, 0x38, 0x64,
                                        0x68, 0x70, 0x43, 0x45, 0x46, 0x4A,
                                        0x4C, 0x52, 0x54, 0x58, 0x62, 0x1A,
                                        0x26, 0x16, 0x0E, 0x13, 0x0B, 0x07,
                                        0x61, 0x51, 0x31, 0x29, 0x19, 0x25,
                                        0x15, 0x0D
                                      };

   static unsigned char aCsdEccSyndroms[]  = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20,
                                        0x40
                                      };

   vEccbytes[0]=0x00;
   vEccbytes[1]=0x00;
   vEccbytes[2]=0x00;
   vEccbytes[3]=0x00;
   
   seepromGenCsDecode( 32, checkSyndrom, aCsdDataSyndroms,csdData);
   seepromGenCsDecode( 7, checkSyndrom, aCsdEccSyndroms,vEccbytes);
   *csdEcc = vEccbytes[3];

}






/******************************************************************************/
/* seepromEccCheck                                                            */
/*                                                                            */
/* Checks the data integrety and correct it if possible                       */
/*                                                                            */
/******************************************************************************/

int seepromEccCheck(unsigned char* vData, unsigned char* vEcc, size_t numOfWords)
{
   int vPosition;
   int vFoundPosCounter;
   int vRet = 0;
   unsigned char vCheckSyndrom;
   unsigned char vCheckSyndromDecodeData[4];
   unsigned char vCheckSyndromDecodeEcc[1];
   size_t i;
   int j;

   for (i=0; i < numOfWords; i++)
   {

      vCheckSyndrom = vEcc[i] ^ seepromGetEcc((const unsigned char*)&vData[i*4]);
      if (vCheckSyndrom != 0)  // if error occurs
      {
         seepromGenerateCheckSyndromDecode(vCheckSyndrom,
                                    vCheckSyndromDecodeData,
                                    vCheckSyndromDecodeEcc);
         vPosition = 0;
         vFoundPosCounter = 0;     
         for(j=0;j<4;j++)
         {
            if(vCheckSyndromDecodeData[j] != 0x00)
            {
                  vPosition = j;
                  vFoundPosCounter++;  
            }
         }
         if ((vFoundPosCounter == 0) && (vCheckSyndromDecodeEcc[0] == 0))
         {
            vRet = VPD_ECC_UNCORRECTABLE_DATA;
            break;
         }
         else
         {
            vRet = VPD_ECC_CORRECTABLE_DATA;
            vData[i*4+vPosition]   = (vData[i*4+vPosition] ^ vCheckSyndromDecodeData[vPosition]);
         }
      }
   }
   return vRet;
}

