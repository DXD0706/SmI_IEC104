
/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "queue.h"

#include "main.h"
#include "time.h"
#include "stdlib.h"
#include "string.h"

#include "stm32f4xx_hal_spi.h"
#include "stm32f4xx_hal_gpio.h"
#include "at45db161d.h"

extern SPI_HandleTypeDef 		SpiHandle;
extern MEMDeviceIDTypeDef 		SPIMemoryID;

/* test_page - ����� ��� ������ � ������   */
//uint8_t page_Buf_In[PAGE_SIZE],page_Buf_Out[PAGE_SIZE];

//*******************************************************************************
// void AT45DB161D_spi_init(void)
//
//*******************************************************************************
void AT45DB161D_spi_init(void)
{

	/* Set the SPI parameters */
	SpiHandle.Instance              	= SPI1;
	SpiHandle.Init.BaudRatePrescaler 	= SPI_BAUDRATEPRESCALER_2;
	SpiHandle.Init.Direction        	= SPI_DIRECTION_2LINES;
	SpiHandle.Init.CLKPhase          	= SPI_PHASE_1EDGE;
	SpiHandle.Init.CLKPolarity       	= SPI_POLARITY_LOW;
	SpiHandle.Init.CRCCalculation    	= SPI_CRCCALCULATION_DISABLE;
	SpiHandle.Init.CRCPolynomial     	= 7;
	SpiHandle.Init.DataSize          	= SPI_DATASIZE_8BIT;
	SpiHandle.Init.FirstBit          	= SPI_FIRSTBIT_MSB;
	SpiHandle.Init.NSS               	= SPI_NSS_SOFT;
	SpiHandle.Init.TIMode            	= SPI_TIMODE_DISABLE;
	SpiHandle.Init.Mode 			 	= SPI_MODE_MASTER;


	if(HAL_SPI_Init(&SpiHandle) != HAL_OK)
	{
	  USART_TRACE("SPI Init error.. ������ ad45db161d �� ����� ����������. \n");
	  //for(;;){}
	} else {
	  USART_TRACE("���� ���� SPI... ok. \n");

	}

//  HAL_SPI_TransmitReceive_DMA(&SpiHandle, (uint8_t*)page_Buf_Out, (uint8_t *)page_Buf_In, PAGE_SIZE);

}
//*******************************************************************************
// uint32_t	MEM_ID_Read (void)
// ������ ID ������
//*******************************************************************************
int8_t	MEM_ID_Read (MEMDeviceIDTypeDef *IDInfo){

	uint8_t 	DataOut[5] , DataIn[5];
	int8_t		ret = TRUE;

	DataOut[0] = AT45DB_ID_READ;
	DataOut[1] = 0;
	DataOut[2] = 0;
	DataOut[3] = 0;
	DataOut[4] = 0;

    MEM_Chipselect(GPIO_PIN_RESET);

    if(HAL_SPI_TransmitReceive_DMA(&SpiHandle,DataOut,DataIn, 5) != HAL_OK)
    {
    	USART_TRACE_RED("������ ������-�������� SPI. \n");
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}
    MEM_Chipselect(GPIO_PIN_SET);				// �������� CS

    IDInfo->ManufacturerID = DataIn[1];
    IDInfo->FamilyCode = DataIn[2]>>5;
    IDInfo->DensityCode = DataIn[2] & 0b00011111;
    IDInfo->MLCCode = DataIn[3]>>5;
    IDInfo->ProductVersionCode = DataIn[3] & 0b00011111;
    IDInfo->ByteCount = DataIn[4];

    if (IDInfo->ManufacturerID == 0x1f) ret = TRUE; else ret = FALSE;

	USART_TRACE("-- ��������� ������ SPI ------------------------------\n");
	USART_TRACE("SPIMemory:\n");
	USART_TRACE("JEDEC Assigned Code - %.2X\n",IDInfo->ManufacturerID);
	USART_TRACE("Family Code - %.2X\n",IDInfo->FamilyCode);
	USART_TRACE("Density Code - %.2X, �����:%u �������\n",IDInfo->DensityCode,0x40<<IDInfo->DensityCode);
	USART_TRACE("MLC Code - %.2X\n",IDInfo->MLCCode);
	USART_TRACE("Product Version Code - %.2X\n",IDInfo->ProductVersionCode);
	USART_TRACE("Byte Count - %.2X\n",IDInfo->ByteCount);
	USART_TRACE("------------------------------------------------------\n");


    return ret;
}
//*******************************************************************************
// void	MEM_Reset (void)
// ������ ID ������
//*******************************************************************************
void MEM_Reset (void){

}

/*************************************************************************
 * Page_memory_read
 * PageAddr - ����� ��������
 * ����� ��������: A20-A9
 * ����� ������ ��������: A6-A0
 *************************************************************************/
int8_t 	Page_memory_read (uint8_t *dst, uint16_t PageAddr, uint16_t PageSize){

	uint8_t 	DataOut[8];

	DataOut[0] = AT45DB_MM_PAGE_READ;				// CMD
	DataOut[1] = (uint8_t)(PageAddr >> 6);			// A20-A16
	DataOut[2] = (uint8_t)(PageAddr << 2);			// A15-A8
	DataOut[3] = (uint8_t) 0;						// A7-A0

	// 	4 Dummy Bytes
	DataOut[4] = (uint8_t) 0;
	DataOut[5] = (uint8_t) 0;
	DataOut[6] = (uint8_t) 0;
	DataOut[7] = (uint8_t) 0;

    MEM_Chipselect(GPIO_PIN_RESET);

    if(HAL_SPI_Transmit_DMA(&SpiHandle,DataOut,8) != HAL_OK){
    	USART_TRACE_RED("������ �������� SPI. \n");
    	return FALSE;
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}

    if(HAL_SPI_Receive_DMA(&SpiHandle,dst,PAGE_SIZE_512) != HAL_OK){
    	USART_TRACE_RED("������ ������ SPI. \n");
    	return FALSE;
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}

    MEM_Chipselect(GPIO_PIN_SET);

	return TRUE;
}
/*************************************************************************
* Block_memory_read
* PageSize - ������ ��������
* NumberOfPages - ����� �������
 *************************************************************************/
int8_t 	Block_memory_read (uint8_t *dst, uint16_t PageAddr, uint16_t PageSize,  uint16_t NumberOfPages){

uint16_t	i;

	for (i=0; i<NumberOfPages ;i++){
		if (!Page_memory_read(dst+i*PageSize,PageAddr,PageSize)) return FALSE;
	}

	return TRUE;
}
/*************************************************************************
* Page_memory_write
* PageAddr - ����� ��������
* PageSize - ������ ��������
 *************************************************************************/
int8_t 		Page_memory_write(uint8_t *src, uint16_t PageAddr, uint16_t PageSize){

	uint8_t 	DataOut[8];

	// ��� ���� ����������� �������� � 2-�� ��������. ���� ������� ������ � ������ �����
	// �������� �� ������ �����, �� �� ������������ ������ �� ������ �������� ������ ��� � ������.

	// ������ �� ������ � ����� 1
	DataOut[0] = AT45DB_MM_PAGE_TO_B1_XFER;				// CMD
	DataOut[1] = (uint8_t)(PageAddr >> 6);			// A20-A16
	DataOut[2] = (uint8_t)(PageAddr << 2);			// A15-A8
	DataOut[3] = (uint8_t) 0;						// A7-A0

    MEM_Chipselect(GPIO_PIN_RESET);

    if(HAL_SPI_Transmit_DMA(&SpiHandle,DataOut,4) != HAL_OK){
    	USART_TRACE_RED("������ �������� SPI. \n");
    	return FALSE;
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}
    MEM_Chipselect(GPIO_PIN_SET);

	while(!((AT45DB_StatusRegisterRead()>>7) & 1 )){}			// ������� ���������� ������


    // �������� � ����� ���� ������
	DataOut[0] = AT45DB_BUFFER_1_WRITE;	// CMD
	DataOut[1] = 0;						// xxxx
	DataOut[2] = 0;						// BFA9-8
	DataOut[3] = 0;						// BFA7-0

    MEM_Chipselect(GPIO_PIN_RESET);

    if(HAL_SPI_Transmit_DMA(&SpiHandle,DataOut,4) != HAL_OK){
    	USART_TRACE_RED("������ �������� ������� ������ � ����� ������ SPI. \n");
    	return FALSE;
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}

    if(HAL_SPI_Transmit_DMA(&SpiHandle,src,PAGE_SIZE_512) != HAL_OK){
    	USART_TRACE_RED("������ �������� ������ � ����� ������ SPI. \n");
    	return FALSE;
    }
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}

    MEM_Chipselect(GPIO_PIN_SET);

	while(!((AT45DB_StatusRegisterRead()>>7) & 1 )){}			// ������� ���������� ������

	MEM_Chipselect(GPIO_PIN_RESET);
    // ������������ ����� � main memory
	DataOut[0] = AT45DB_B1_TO_MM_PAGE_PROG_WITH_ERASE;	// CMD
	DataOut[1] = (uint8_t)(PageAddr >> 6);			// A20-A16
	DataOut[2] = (uint8_t)(PageAddr << 2);			// A15-A8
	DataOut[3] = (uint8_t) 0;							// A7-A0

	if(HAL_SPI_Transmit_DMA(&SpiHandle,DataOut,4) != HAL_OK){
		USART_TRACE_RED("������ �������� ������� ���������� �� ������ � ������ SPI. \n");
		return FALSE;
	}
    while (HAL_SPI_GetState(&SpiHandle) != HAL_SPI_STATE_READY){}

    MEM_Chipselect(GPIO_PIN_SET);

	return TRUE;
}

/*************************************************************************
* Block_memory_write
* PageSize - ������ ��������
* NumberOfPages - ����� �������
 *************************************************************************/
int8_t 		Block_memory_write(uint8_t *src, uint16_t PageAddr, uint16_t PageSize,  uint16_t NumberOfPages){

	uint16_t	i;

		for (i=0; i<NumberOfPages ;i++){
			if (!Page_memory_write(src+i*PageSize,PageAddr,PageSize)) return FALSE;
		}

		return TRUE;
}

//*******************************************************************************
// bool Block_memory_write_noWait (uint16_t Pageaddress, uint16_t Memaddress, uint8_t *dst, uint16_t Size)
// ������ ������ � ���� Pageaddress � ��������� ������� ����� ������ �������� Memaddress
// ��� �������� ��������� ������
//*******************************************************************************
int8_t 		Block_memory_write_noWait(uint16_t Pageaddress, uint16_t Memaddress, uint8_t *dst, uint16_t Size){

    return TRUE;
}
//*******************************************************************************
// bool Bufer2_to_memory_write(uint16_t Pageaddress)
// ������ �������� ������ �� ������ Address � �����1
//*******************************************************************************
int8_t Bufer2_to_memory_write(uint16_t Pageaddress){

	uint8_t 	DataOut[4];

	DataOut[0] = AT45DB_B2_TO_MM_PAGE_PROG_WITH_ERASE;
	DataOut[1] = ((uint8_t)(Pageaddress >> 6));
	DataOut[2] = ((uint8_t)(Pageaddress << 2));
	DataOut[3] = 0x00;

	MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 4);
	MEM_Chipselect(GPIO_PIN_SET);	// ����� ����� 40��
	osDelay(40);

return TRUE;
}
//*******************************************************************************
// uint8_t memory_read (uint8_t Command, uint32_t Address)
// ������ �������� ������ �� ������ Address � �����1
//*******************************************************************************
void memory_to_buffer1_transfer (uint16_t page){

	uint8_t 	DataOut[4];

	DataOut[0] = AT45DB_MM_PAGE_TO_B1_XFER;
	DataOut[1] = ((uint8_t)(page >> 6));
	DataOut[2] = ((uint8_t)(page << 2));
	DataOut[3] = 0x00;

	MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 4);
	MEM_Chipselect(GPIO_PIN_SET);
	osDelay(1);	// ����� ����� 400���

}
//*******************************************************************************
// uint8_t memory_read (uint8_t Command, uint32_t Address)
// ������ �������� ������ �� ������ Address � �����2
//*******************************************************************************
void memory_to_buffer2_transfer (uint16_t page){

	uint8_t 	DataOut[4];

	DataOut[0] = AT45DB_MM_PAGE_TO_B2_XFER;
	DataOut[1] = ((uint8_t)(page >> 6));
	DataOut[2] = ((uint8_t)(page << 2));
	DataOut[3] = 0x00;

//TODO: ����� ������� �������� � ���������, � ����� ��������� ������.
	while(!((AT45DB_StatusRegisterRead()>>7) & 1 )){}// �������� ���� ����� ��� ����������

	MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 4);
	MEM_Chipselect(GPIO_PIN_SET);

	while(!((AT45DB_StatusRegisterRead()>>7) & 1 )){}// �������� ���� �� ��������� � �����

//	Delay_mks(400);
}
//*******************************************************************************
// uint8_t AT45DB_StatusRegisterRead(void)
// ������ �������� ������� ������
//*******************************************************************************
uint8_t AT45DB_StatusRegisterRead(void)
{
	uint8_t 	DataOut = AT45DB_READ_STATE_REGISTER,DataIn[2];

    MEM_Chipselect(GPIO_PIN_RESET);
    if(HAL_SPI_TransmitReceive_DMA(&SpiHandle,(uint8_t*)&DataOut,(uint8_t*)&DataIn[0], 2) != HAL_OK)
      {
      	USART_TRACE_RED("������ ������-�������� �������� �������. \n");
      }
 //   HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 1);
 //   HAL_SPI_Receive_DMA(&SpiHandle,(uint8_t*)&DataIn, 1);
	MEM_Chipselect(GPIO_PIN_SET);

    return DataIn[1];
}
//*******************************************************************************
// void AT45DB_buffer1_write(uint16_t addres,uint8_t data )
// ������ ����� � �����
// ��������� ����� � ������
//*******************************************************************************
void AT45DB_buffer1_write(uint16_t addres,uint8_t data )
{
	uint8_t 	DataOut[5];

	DataOut[0] = AT45DB_BUFFER_1_WRITE;
	DataOut[1] = 0x00;
	DataOut[2] = ((uint8_t)(addres >> 8));
	DataOut[3] = ((uint8_t)addres );
	DataOut[4] = data;

    MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 5);
	MEM_Chipselect(GPIO_PIN_SET);
}
//*******************************************************************************
// void AT45DB_buffer1_write(uint16_t addres,uint8_t data )
// ������ ����� � �����
// ��������� ����� � ������
//*******************************************************************************
uint8_t AT45DB_buffer1_read(uint16_t addres )
{
	uint8_t 	DataOut[5],DataIn;

	DataOut[0] = AT45DB_BUFFER_1_READ;
	DataOut[1] = 0x00;
	DataOut[2] = ((uint8_t)(addres >> 8));
	DataOut[3] = ((uint8_t)addres );
	DataOut[4] = 0x00;

    MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 5);
    HAL_SPI_Receive_DMA(&SpiHandle,(uint8_t*)&DataIn, 1);
    MEM_Chipselect(GPIO_PIN_SET);

	return DataIn;
}
//*******************************************************************************
// void AT45DB_buffer1_write(uint16_t addres,uint8_t data )
// ������ ����� � �����
// ��������� ����� � ������
//*******************************************************************************
uint8_t AT45DB_buffer2_read(uint16_t addres )
{
	uint8_t 	DataOut[5],DataIn;

	DataOut[0] = AT45DB_BUFFER_2_READ;
	DataOut[1] = 0x00;
	DataOut[2] = ((uint8_t)(addres >> 8));
	DataOut[3] = ((uint8_t)addres );
	DataOut[4] = 0x00;

    MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&DataOut, 5);
    HAL_SPI_Receive_DMA(&SpiHandle,(uint8_t*)&DataIn, 1);
    MEM_Chipselect(GPIO_PIN_SET);

	return DataIn;
}
//*******************************************************************************
// void AT45DB_buffer1_write(uint16_t addres,uint8_t data )
// ������ �����1 � ������
// ����� ��������� � �������� ������
//*******************************************************************************
void AT45DB_buffer1_to_memory(uint16_t addres)
{
	uint8_t 	Data[5];

	Data[0] = AT45DB_B1_TO_MM_PAGE_PROG_WITH_ERASE;
	Data[1] = 0x00;
	Data[2] = ((uint8_t)(addres >> 6));
	Data[3] = ((uint8_t)(addres << 2));
	Data[4] = 0x00;

    MEM_Chipselect(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&Data,5);
	MEM_Chipselect(GPIO_PIN_SET);
}
//*******************************************************************************
// void SPI_HostWriteByte(uint8_t Data)
// ������ � SPI
//*******************************************************************************
void SPI_HostWriteByte(uint8_t Data){

	HAL_SPI_Transmit_DMA(&SpiHandle,(uint8_t*)&Data,1);
}
//*******************************************************************************
// uint8_t SPI_HostReadByte(void)
// ������ �� SPI ���������
//*******************************************************************************
uint8_t SPI_HostReadByte(void){

	uint8_t 	Data;

	HAL_SPI_Receive_DMA(&SpiHandle,(uint8_t*)&Data,1);

	return Data;
}
//*******************************************************************************
// uint8_t SPI_HostReadByte(void)
// ������ �� SPI �����
//*******************************************************************************
uint8_t SPI_STATEREGISTERRead(void){

	uint8_t 	Data;

	HAL_SPI_Receive_DMA(&SpiHandle,(uint8_t*)&Data,1);

	return Data;
}
//*******************************************************************************
// void MEM_Chipselect(uint8_t select)
// ������� ������� CS ��� ������
//*******************************************************************************
void MEM_Chipselect(GPIO_PinState select){

	HAL_GPIO_WritePin(db161d_CS_GPIO_PORT,db161d_CS_PIN,select);

}


