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

#ifndef _VPDECC_H_
#define _VPDECC_H_

#include <stdlib.h>

#define VPD_ECC_OK 0
#define VPD_ECC_NOT_ENOUGH_BUFFER 1
#define VPD_ECC_WRONG_ECC_SIZE 2
#define VPD_ECC_WRONG_BUFFER_SIZE 9
#define VPD_ECC_UNCORRECTABLE_DATA 90
#define VPD_ECC_CORRECTABLE_DATA 91

#ifdef __cplusplus
extern "C" {
#endif

/* TODO  doxygen !!!!!!!! */

/******************************************************************************/
/* vpdecc_create_ecc                                                          */
/*                                                                            */
/* For a given data block (together with the length of the data block         */
/* this function creates the ECC                                              */
/*                                                                            */
/* @param     pData           In-Buffer containing the raw VPD data           */
/*                                            (wont't be changed)             */
/*                                                                            */
/* @param     vDataLength     In        should contain the length of the Data */
/*                                      in the buffer given to vData          */
/*                                                                            */
/* @param     pEcc            Out-Buffer after execution this will be the     */
/*                                      buffer for the calculated Ecc         */
/*                                                                            */
/* @param     pEccLenght      In/Out    In : size of buffer                   */
/*                                      Out: contains the length of the Ecc   */
/*                                                                            */
/* @return Error returncode                                                   */
/******************************************************************************/
int vpdecc_create_ecc(const unsigned char* data, size_t datalength,
                      unsigned char* ecc, size_t* ecc_buffersize);

/******************************************************************************/
/* vpdecc_check_data                                                          */
/*                                                                            */
/* For a given data block (together with the ecc)                             */
/* this function checks the data for validness                                */
/*                                                                            */
/* @param     pData           In-Buffer containing the raw VPD data           */
/*                            Out-Buffer containing the raw VPD data          */
/*                                      No error           : data unchanged   */
/*                                      Correctable error  : data corrected   */
/*                                      Uncorrectable error: data unchanged   */
/*                                                                            */
/* @param     vDataLength     In        should contain the length of the Data */
/*                                      in the buffer given to vData          */
/*                                                                            */
/* @param     pEcc            In-Buffer should contain the Ecc for the data   */
/*                                                                            */
/*                                                                            */
/* @param     vEccLenght      In        should contain the length of the Ecc  */
/*                                                                            */
/* @return Error returncode                                                   */
/******************************************************************************/
int vpdecc_check_data(unsigned char* data, size_t data_length,
                      const unsigned char* ecc, size_t ecc_length);

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif /* endif _VPDECC_H_ */
