/******************************************************************************
 * File Name:   main.c
 *
 * Description: This is the source code for the USB FS CDC Echo Example
 *              for ModusToolbox.
 *
 * Related Document: See README.md
 *
 *
*******************************************************************************
* Copyright 2021-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/


/*******************************************************************************
 * Include header files
 ******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_usb_dev.h"
#include "cy_usb_dev_cdc.h"
#include "cycfg_usbdev.h"
#include "stdio.h"
#include <inttypes.h>

/*******************************************************************************
 * Macros
 ********************************************************************************/
#define USB_BUFFER_SIZE     (64u)
#define USB_COM_PORT        (0u)

/* CY Assert failure */
#define CY_ASSERT_FAILED    (0u)

/* Debug print macro to enable UART print */
#define DEBUG_PRINT         (0u)

/*******************************************************************************
 * Function Prototypes
 ********************************************************************************/
static void usb_high_isr(void);
static void usb_medium_isr(void);
static void usb_low_isr(void);

/*******************************************************************************
 * Global Variables
 ********************************************************************************/
/* Stores number of data bytes received from host */
uint32_t count;

/* Array containing the data bytes received from host */
uint8_t buffer[USB_BUFFER_SIZE];

/* USB Interrupt Configuration */
const cy_stc_sysint_t usb_high_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_hi_IRQn,
        .intrPriority = 0U,
};
const cy_stc_sysint_t usb_medium_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_med_IRQn,
        .intrPriority = 1U,
};
const cy_stc_sysint_t usb_low_interrupt_cfg =
{
        .intrSrc = (IRQn_Type) usb_interrupt_lo_IRQn,
        .intrPriority = 2U,
};

/* USBDEV context variables */
cy_stc_usbfs_dev_drv_context_t usb_drvContext;
cy_stc_usb_dev_context_t usb_devContext;
cy_stc_usb_dev_cdc_context_t usb_cdcContext;

#if DEBUG_PRINT
cy_stc_scb_uart_context_t UART_context; /* UART context */

/* Variable used for tracking the print status */
volatile bool ENTER_LOOP = true;

/*******************************************************************************
* Function Name: check_status
********************************************************************************
* Summary:
*  Prints the error message.
*
* Parameters:
*  error_msg - message to print if any error encountered.
*  status - status obtained after evaluation.
*
* Return:
*  void
*
*******************************************************************************/
void check_status(char *message, cy_rslt_t status)
{
    char error_msg[50];

    sprintf(error_msg, "Error Code: 0x%08" PRIX32 "\n", status);

    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\nFAIL: ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, message);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, error_msg);
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\r\n=====================================================\r\n");
}
#endif

/*******************************************************************************
 * Function Name: main
 ********************************************************************************
 * Summary:
 *  This is the main function. It initializes the USB Device block
 *  and enumerates as a CDC device. It is constantly checking for data
 *  received from host and echos it back .
 *
 * Parameters:
 *  void
 *
 * Return:
 *  int
 *
 *******************************************************************************/
int main(void)
{
    /* Stores result following specific operations */
    cy_rslt_t result;
    cy_en_sysint_status_t intr_result;
    cy_en_usb_dev_status_t usb_result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable global interrupts */
    __enable_irq();

#if DEBUG_PRINT
    /* Configure and enable the UART peripheral */
    Cy_SCB_UART_Init(CYBSP_UART_HW, &CYBSP_UART_config, &UART_context);
    Cy_SCB_UART_Enable(CYBSP_UART_HW);

    /* Sequence to clear screen */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "\x1b[2J\x1b[;H");

    /* Print "USB FS CDC Echo" */
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** ");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "PMG1 MCU: USB FS CDC Echo");
    Cy_SCB_UART_PutString(CYBSP_UART_HW, "****************** \r\n\n");
#endif

    /* Initialize the USB device */
    usb_result = Cy_USB_Dev_Init(CYBSP_USB_HW, &CYBSP_USB_config, &usb_drvContext, &usb_devices[0], &usb_devConfig, &usb_devContext);
    if (usb_result != CY_USB_DEV_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_USB_Dev_Init failed with error code", usb_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Initialize the CDC Class */
    usb_result = Cy_USB_Dev_CDC_Init(&usb_cdcConfig, &usb_cdcContext, &usb_devContext);
    if (usb_result != CY_USB_DEV_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_USB_Dev_CDC_Init failed with error code", usb_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Initialize the USB interrupts */
    intr_result = Cy_SysInt_Init(&usb_high_interrupt_cfg, &usb_high_isr);
    if (intr_result != CY_SYSINT_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_SysInt_Init failed with error code", intr_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    intr_result = Cy_SysInt_Init(&usb_medium_interrupt_cfg, &usb_medium_isr);
    if (intr_result != CY_SYSINT_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_SysInt_Init failed with error code", intr_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    intr_result = Cy_SysInt_Init(&usb_low_interrupt_cfg, &usb_low_isr);
    if (intr_result != CY_SYSINT_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_SysInt_Init failed with error code", intr_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    /* Enable the USB interrupts */
    NVIC_EnableIRQ(usb_high_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_medium_interrupt_cfg.intrSrc);
    NVIC_EnableIRQ(usb_low_interrupt_cfg.intrSrc);

    /* Make device appear on the bus. This function call is blocking,
     * it waits until the device enumerates */
    usb_result = Cy_USB_Dev_Connect(true, CY_USB_DEV_WAIT_FOREVER, &usb_devContext);
    if (usb_result != CY_USB_DEV_SUCCESS)
    {
#if DEBUG_PRINT
        check_status("API Cy_USB_Dev_Connect failed with error code", usb_result);
#endif
        CY_ASSERT(CY_ASSERT_FAILED);
    }

    for (;;)
    {
        /* Check if host sent any data */
        if (Cy_USB_Dev_CDC_IsDataReady(USB_COM_PORT, &usb_cdcContext))
        {
            /* Get number of data bytes received from USB Host and store these data bytes in to 'buffer' */
            count = Cy_USB_Dev_CDC_GetAll(USB_COM_PORT, buffer, USB_BUFFER_SIZE, &usb_cdcContext);

            if (0u != count)
            {
                /* Wait until component is ready to send data to host */
                while (0u == Cy_USB_Dev_CDC_IsReady(USB_COM_PORT, &usb_cdcContext))
                {
                }

                /* Send data back to host using data bytes from 'buffer' */
                Cy_USB_Dev_CDC_PutData(USB_COM_PORT, buffer, count, &usb_cdcContext);

                /* If the last sent packet is exactly the maximum packet
                 *  size, it is followed by a zero-length packet to assure
                 *  that the end of the segment is properly identified by
                 *  the terminal.
                 */
                if (USB_BUFFER_SIZE == count)
                {
                    /* Wait until component is ready to send data to PC. */
                    while (0u == Cy_USB_Dev_CDC_IsReady(USB_COM_PORT, &usb_cdcContext))
                    {
                    }

                    /* Send zero-length packet to PC. */
                    Cy_USB_Dev_CDC_PutData(USB_COM_PORT, NULL, 0u, &usb_cdcContext);
                }
            }
        }
#if DEBUG_PRINT
        if (ENTER_LOOP)
        {
            Cy_SCB_UART_PutString(CYBSP_UART_HW, "Entered for loop\r\n");
            ENTER_LOOP = false;
        }
#endif
    }

}

/***************************************************************************
 * Function Name: usb_high_isr
 ********************************************************************************
 * Summary:
 *  This function processes the high priority USB interrupts.
 *
 ***************************************************************************/
static void usb_high_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseHi(CYBSP_USB_HW), &usb_drvContext);
}

/***************************************************************************
 * Function Name: usb_medium_isr
 ********************************************************************************
 * Summary:
 *  This function processes the medium priority USB interrupts.
 *
 ***************************************************************************/
static void usb_medium_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseMed(CYBSP_USB_HW), &usb_drvContext);
}

/***************************************************************************
 * Function Name: usb_low_isr
 ********************************************************************************
 * Summary:
 *  This function processes the low priority USB interrupts.
 *
 **************************************************************************/
static void usb_low_isr(void)
{
    /* Call interrupt processing */
    Cy_USBFS_Dev_Drv_Interrupt(CYBSP_USB_HW, Cy_USBFS_Dev_Drv_GetInterruptCauseLo(CYBSP_USB_HW), &usb_drvContext);
}

/* [] END OF FILE */

