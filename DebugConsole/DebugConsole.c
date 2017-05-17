

/* Standard includes. */
#include "string.h"
#include "stdio.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

#include "main.h"
#include "DebugConsole.h"
#include "cmdinterpreter.h"
#include "usart.h"

// ������ ������ ������ � �������� �������� �� �������
#define cmdMAX_INPUT_SIZE					20
#define cmdMAX_OUTPUT_SIZE					255

// ������������ ����� ��� �������� ������� ��������
#define cmdMAX_MUTEX_WAIT					( 200 / portTICK_RATE_MS )			// ��/portTICK_RATE_MS

/*-----------------------------------------------------------*/

// ������, ������� ��������� ��������� ��������� �������.
//static void DEBUGConsoleTask( void *pvParameters );

/*-----------------------------------------------------------*/

// ������������� ������ ��� ������ ����� ������ (���� �������� �� ����������� ����������)
static xSemaphoreHandle 	xNewDataSemaphore;

// ������ ������� � ����������� USART ���� �� ������������ ����� ����� �������.
static xSemaphoreHandle 	xDEBUGbusMutex;		//xCDCMutex

extern xQueueHandle		xDebugUsartOut;			// ������� ��� �������� � ����������

/* Const messages output by the command console. */
//static const char * const pcWelcomeMessage = "����� ������� IEC61850.\r\n\r\n>";
static const char * const pcEndOfOutputMessage = "\r\nENTER\r\n>";
static const char * const pcNewLine = "\r\n";


UART_HandleTypeDef 	BOOT_UART;			//USART1

/*-----------------------------------------------------------*/

char cGetChar( void );

/************************************************************
 *
 *
 *
 ************************************************************/
void DEBUGUSARTOUTTask(void const * argument)
{
	char 	*OutputString;

	// ������� � 5-� �������� �� ����� ������ ��� ��������
	xDebugUsartOut = xQueueCreate( 5, sizeof(char*));

	/*
	 * ��� ��������� � �������.
	 * xQueueSend( xDebugUsartOut, ( void * ) &toSend, portMAX_DELAY  );
	 *
	 * ��� �������� �� �������
	 * xQueueReceive(xDebugUsartOut, &( rec ), portMAX_DELAY );
	 */
	 for( ;; ) {
		if (xQueueReceive( xDebugUsartOut,  OutputString, 0 )){
			//HAL_UART_Transmit_DMA(&BOOT_UART, OutputString, strlen(OutputString) );
			HAL_UART_Transmit(&BOOT_UART, (uint8_t *)OutputString, strlen(OutputString), 0xFFFF);
		}
	 }
}
/*-----------------------------------------------------------*/
void DEBUGConsoleTask(void const * argument)//( void *pvParameters )
{
char 		cRxedChar;
uint8_t 	ucInputIndex = 0;
char 		*pcOutputString;					// ��������� �� �������� �����
static char cInputString[ cmdMAX_INPUT_SIZE ], cLastInputString[ cmdMAX_INPUT_SIZE ];
BaseType_t 	xReturned;

//	( void ) pvParameters;

	// �������� �������� � �������� ������������ �������
	xDEBUGbusMutex = xSemaphoreCreateMutex();			// ���������� ������� � USART
	vSemaphoreCreateBinary( xNewDataSemaphore );		// ������ � �������� ����� ������
	// �������� ��������� ��������� ��� ��� ?
	configASSERT( xDEBUGbusMutex );
	configASSERT( xNewDataSemaphore );

	// ������� �������� � �������� � ������� ��� ��������� ����� (������������)
	//  API-������� vQueueAddToRegistry() �� ����� ������� ��������������, ����� ��� ��� ����� �������. ������ ��-
	// ������� � ������ ������� ������ �� �������, ��������� ������� ���������� �������� � ���� �������.
	vQueueAddToRegistry( xDEBUGbusMutex, "DBGMu" );
	vQueueAddToRegistry( xNewDataSemaphore, "DBGDat" );


	// ������� ����� ��������� ������.
	pcOutputString = DEBUG_GetOutputBuffer();

	// ���� ��������� ��������
//	BOOT_UART_Init(115200);

	// �������� ������
//	HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)"DBG->", strlen( "DBG->" ));

	for( ;; )
	{
		// ������� ������� �������� ������
		cRxedChar = 0;

		// ������ �� ������ ��������� ������� �� ���
		cRxedChar = cGetChar();
		if( xSemaphoreTake( xDEBUGbusMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )							// �������� ������� ������������ ����� cmdMAX_MUTEX_WAIT �����
		{
			// ���������� ���.
//			HAL_UART_Transmit(&BOOT_UART, (uint8_t *)&cRxedChar, 1, 0xFFFF);
			HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)&cRxedChar, 1);

			// ��� �� ��� ����� ������?
			if( cRxedChar == '\n' || cRxedChar == '\r' )
			{
				// ��������� ������� �� ��������� ������
//				HAL_UART_Transmit(&BOOT_UART, (uint8_t *)pcNewLine, strlen( pcNewLine ), 0xFFFF);
				HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)pcNewLine, strlen( pcNewLine ));

				//���� ������� �����, �� ��������� ������� ������ ���� ��������� ��������.
				if( ucInputIndex == 0 )
				{
					// ��������� ��������� ������� ������� � ������ �����.
					strcpy( cInputString, cLastInputString );
				}

				// ��������� ���������� ������� ����� �������������� ������. �������� ��� �����������, ���� �� �� ����� pdFALSE
				// �.�. �� ����� ������������ ����� ����� ������.
				do
				{
					// �������� ������ ��� ������ �� �������������� ������.
					xReturned = DEBUG_ProcessCommand( cInputString, pcOutputString, BOOT_BUF_MAX_OUTPUT_SIZE );
					vTaskDelay( 1 );
					// ��������� ����� � ����
					HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)pcOutputString, strlen( pcOutputString ));
					vTaskDelay( 10 );

				} while( xReturned != pdFALSE );

				// �������� ������ ����� ��� ������ ��������� �������. ��������
				// �������, ������� ����, �� ������ ���� ��� ������ ����
				// �������������� �����.
				strcpy( cLastInputString, cInputString );
				ucInputIndex = 0;
				memset( cInputString, 0x00, cmdMAX_INPUT_SIZE );

				HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)pcEndOfOutputMessage, strlen( pcEndOfOutputMessage ));

			}
			else
			{
				if( cRxedChar == '\r' )
				{
					// ���������� ������
				}
				else if( cRxedChar == '\b' )
				{
					// Backspace - �������� ���������� ������� � ��������� ������
					if( ucInputIndex > 0 )
					{
						ucInputIndex--;
						cInputString[ ucInputIndex ] = '\0';
					}
				}
				else
				{
					// ��������� ������ � ������ �������� �� �����. ���� ������� \n �� ������� ���������� �� ����������.
					if( ( cRxedChar >= ' ' ) && ( cRxedChar <= '~' ) )
					{
						if( ucInputIndex < cmdMAX_INPUT_SIZE )
						{
							cInputString[ ucInputIndex ] = cRxedChar;
							ucInputIndex++;
						}
					}
				}
			}

			// ����� �������.
			xSemaphoreGive( xDEBUGbusMutex );
		}
	}
}
/*-----------------------------------------------------------*/

void vOutputString( const char * const pcMessage )
{
	if( xSemaphoreTake( xDEBUGbusMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )						// ���� ������� cmdMAX_MUTEX_WAIT ����� ������
	{
//		HAL_UART_Transmit(&BOOT_UART, (uint8_t *)pcMessage, strlen( pcMessage ), 0xFFFF);
		HAL_UART_Transmit_DMA(&BOOT_UART, (uint8_t *)pcMessage, strlen( pcMessage ));
		xSemaphoreGive( xDEBUGbusMutex );													// ����� �������
	}
}
/*-----------------------------------------------------------*/

char cGetChar( void )
{
uint8_t cInputChar;

	if( xSemaphoreTake( xDEBUGbusMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )	{
		 HAL_UART_Receive_DMA(&BOOT_UART,&cInputChar,1);
		xSemaphoreGive( xDEBUGbusMutex );									// �����

		xSemaphoreTake( xNewDataSemaphore, portMAX_DELAY );					// ���� �� ������� �� ������ �����, �� ����������� ������� ������������� � ����� ����� ������ � ���� �� ���������� ����
	}

	return cInputChar;
}
/*-----------------------------------------------------------*/
// ������ ������� ����� ������ �� USART

/*************************************************************************
 * xMBMasterRTUReceiveFSM
 * ������� ������ �� USART
 *************************************************************************/
void	xDEBUGRTUReceiveFSM( void ){

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

		// �������� ��������� ��������� ��� ��� ?
		configASSERT( xNewDataSemaphore );

		// ������������� � ����� ������ �������� � ����
//		if( xNewDataSemaphore != NULL )
		xSemaphoreGiveFromISR( xNewDataSemaphore, &xHigherPriorityTaskWoken );										// ����� ������� ������������� � ����� ����� ������

		// pxHigherPriorityTaskWoken � ��� ��� ��������� �������� ����� �������� � ��������������� �����,
		// ���������� ������ �������� �������� ������� ���������, ��� ������� ����, �� ��� ���������� ���������
		// ������������� ������������ ��������� (��� ����� ���������� ������� ������ taskYIELD()).
		if( xHigherPriorityTaskWoken == pdTRUE) 	taskYIELD();


}
/*************************************************************************
 * xDEBUGRTUTransmitFSM
 * ������ ��������
 *************************************************************************/
void	xDEBUGRTUTransmitFSM( void ){

	taskYIELD();
}
/*************************************************************************
 * xDEBUGRTUTransmitFSM
 * ������ ��������
 *************************************************************************/

void 	vOutputTime (void){
	extern 	RTC_HandleTypeDef hrtc;
	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;

//	if( xSemaphoreTake( xDEBUGbusMutex, cmdMAX_MUTEX_WAIT ) == pdPASS )						// ���� ������� cmdMAX_MUTEX_WAIT ����� ������
//	{
		HAL_RTC_GetTime(&hrtc, &sTime, FORMAT_BIN);			// ������ �����
		HAL_RTC_GetDate(&hrtc, &sDate, FORMAT_BIN);			// ������ ����

		char* WriteBuffer = pvPortMalloc(30);

		sprintf( WriteBuffer, "[%d.%d.%d.%u] ",sTime.Hours,sTime.Minutes,sTime.Seconds,(uint16_t)(sTime.SubSeconds/2) );
		USART_0TRACE(WriteBuffer);

		vPortFree(WriteBuffer);
//		xSemaphoreGive( xDEBUGbusMutex );													// ����� �������
//	}

}
