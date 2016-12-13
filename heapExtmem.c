/*
 * SPIAddrmap.c
 *
 *  Created on: 27.04.2016
 *      Author: sagok
 */

#include <stdlib.h>
//#include "at45db161d.h"
#include "ExtSPImem.h"
#include "heapExmem.h"

/* Defining MPU_WRAPPERS_INCLUDED_FROM_API_FILE prevents task.h from redefining
all the API functions to use the MPU wrappers.  That should only be done when
task.h is included from an application file. */
//#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

//#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

// ������ ������ ���� �� ����� ���������
#define SPIheapMINIMUM_BLOCK_SIZE	( ( size_t ) ( SPIHeapStructSize * 2 ) )

/* Assumes 8bit bytes! */
#define BITS_PER_BYTE		( ( size_t ) 8 )


// ��������� ������ � ������
typedef struct SPI_Mem_Block
{
	uint32_t	pxAddresBlock;			// ����� �������� �����
	uint32_t	pxNextFreeBlock;		// ��������� ��������� ���� � ������
	uint32_t 	xBlockSize;				// ������ �����  The size of the free block.
} SPI_BlockLink_t;

/*-----------------------------------------------------------*/

/*
 * Inserts a block of memory that is being freed into the correct position in
 * the list of free memory blocks.  The block being freed will be merged with
 * the block in front it and/or the block behind it if the memory blocks are
 * adjacent to each other.
 */
static void SPIInsertBlockIntoFreeList( SPI_BlockLink_t SPIBlockToInsert );


// ������������ ��� ������� ����� ���� � SPI������, ���������� � SPIPortMalloc()
static void SPIHeapInit( void );

/*-----------------------------------------------------------*/

// ������ ��������� �������� � ������ ������ ���������� ������.
static const size_t SPIHeapStructSize	= ( ( sizeof( SPI_BlockLink_t ) + ( portBYTE_ALIGNMENT - 1 ) ) & ~portBYTE_ALIGNMENT_MASK );

// 2-� ������ �� ������ � ����� ������ � ������ SPI
static SPI_BlockLink_t SPIStart, SPIEnd;

// ����������� ����� ��������� ���������� ����
static size_t SPIFreeBytesRemaining = 0U;
static size_t SPIMinimumEverFreeBytesRemaining = 0U;

// ����� ���� ��� � �������� BlockSize �� ��������� BlockLink_t ����������, �� ���� ����������� ����������.
// ����� ��� �������� ���� ��� ��� �������� ������ ��������� ������ � ����
static size_t SPIBlockAllocatedBit = 0;

/*-----------------------------------------------------------*/
uint8_t 	SPIPortWrite(uint32_t addres, uint8_t *data, size_t Size )
{
	if(!memory_write(data,addres,Size)) {
		return FALSE;
	}
	return TRUE;
}
/*************************************************************************
 * SPIPortRead
 *  Size - ������ ����� ��� ������. ���� = 0 �� ������ ��
 *  ��������� ������ � ��������� �� ����� ��������.
 *************************************************************************/
uint8_t		SPIPortRead(uint32_t addres, uint8_t *data, size_t Size )
{
	SPI_BlockLink_t		SPIHeap;
	if (Size == 0) {										// ���� ������ = 0 �� ����� �� ������ ����� ��������� ������ � ��������� ���� ���� �� �����.
		if(!memory_read((uint8_t *)&SPIHeap, addres - SPIHeapStructSize , SPIHeapStructSize)) {
				return FALSE;
			}															// �������� ����� ������� ��� ������.
		Size = SPIHeap.xBlockSize & ~SPIBlockAllocatedBit;				// ������� ��� �������������� � ������� ������.
	}
	if(!memory_read(data,addres,Size)) {
		return FALSE;
	}
	return TRUE;
}
/*-----------------------------------------------------------*/

uint32_t 	SPIPortMalloc( size_t xWantedSize )
{
SPI_BlockLink_t SPIBlock, SPIPreviousBlock, SPINewBlockLink;
uint32_t Return = 0;
int8_t	ret;


//TODO: �������� ����������� ������, ��� ������ � SPI ���� ����������
// �������� �������� ��� ����������
	vTaskSuspendAll();
	{
		if( SPIEnd.pxAddresBlock == 0 )	SPIHeapInit();							// ������ ���� ����.


		if( ( xWantedSize & SPIBlockAllocatedBit ) == 0 )						// ��������, ���� ���� �������� ??
		{

			if( xWantedSize > 0 )
			{
				xWantedSize += SPIHeapStructSize;								// ������� ������ ��� ��������� ���� (���������)

				if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0x00 )			// ���������, ��� ����� ������ ��������� � ���������� ����� ������
				{
					// ���� ���������
					xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
					configASSERT( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) == 0 );
				}
			}


			if( ( xWantedSize > 0 ) && ( xWantedSize <= SPIFreeBytesRemaining ) )									// ���� ���� ��� ������ ������� �������
			{

				ret = memory_read((uint8_t *)&SPIPreviousBlock,0,sizeof(SPI_BlockLink_t));
				if (!ret) return 0;
				ret = memory_read((uint8_t *)&SPIBlock, SPIPreviousBlock.pxNextFreeBlock, sizeof(SPI_BlockLink_t));		// ���������� ��������� ������� (������ ���������)
				if (!ret) return 0;
				while( (( SPIBlock.xBlockSize < xWantedSize ) && ( SPIBlock.pxNextFreeBlock != 0 )) )				// ����
				{
					SPIPreviousBlock.pxAddresBlock 		= SPIBlock.pxAddresBlock;									// ���� ������ ������� ����
					SPIPreviousBlock.pxNextFreeBlock 	= SPIBlock.pxNextFreeBlock;
					SPIPreviousBlock.xBlockSize 		= SPIBlock.xBlockSize;
					ret = memory_read((uint8_t *)&SPIBlock, SPIPreviousBlock.pxNextFreeBlock, sizeof(SPI_BlockLink_t));	// ���������� ��������� ������� (������ ���������)
					if (!ret) return 0;
				}
																													// ���� ��������� �� ����� �� ����������� ����� �� ��������, ������.
				if( SPIBlock.pxAddresBlock != SPIEnd.pxAddresBlock )
				{
					// ������������� ����� ��������� ���� (xHeapStructSize) �������� ����� ������ ����� ������ �� ����� ����� ������� �� �������
					Return =  SPIBlock.pxAddresBlock + (uint32_t)SPIHeapStructSize;

					if( ( SPIBlock.xBlockSize - xWantedSize ) > SPIheapMINIMUM_BLOCK_SIZE )							// ���� ���� ������, ��� ��������� ��� ����� ��������� �� ��� �����. �� ����� ������ ���� ������ heapMINIMUM_BLOCK_SIZE
					{
						SPINewBlockLink.pxAddresBlock = SPIBlock.pxAddresBlock  + xWantedSize;						// ��������� �� ������ ����(����������) ����� ����������.
						configASSERT( (SPINewBlockLink.pxAddresBlock  & portBYTE_ALIGNMENT_MASK ) == 0 );
						SPINewBlockLink.pxNextFreeBlock = 0;

						// ������ �������� ���� ������ ������������ �� ������ �����.
						SPINewBlockLink.xBlockSize = SPIBlock.xBlockSize - xWantedSize;								// ������ ����������� �����
						SPIBlock.xBlockSize = xWantedSize;															// ������ ����������� ��� ��� �����

						// � �������
						SPIBlock.pxNextFreeBlock = SPINewBlockLink.pxAddresBlock;

						// �������� �����(���������) ���� � ������ ��������� ������.
						SPIInsertBlockIntoFreeList( SPINewBlockLink );
					}

					// ���� ���� ���� ��� �������������, ������� ������� �� ������ ��������� ������
					// �.�. ����� ���������� ����������� ������ �����������.
					//SPIPreviousBlock.pxNextFreeBlock = SPIBlock.pxAddresBlock  + xWantedSize;//SPIBlock.pxNextFreeBlock; // ��� �� ���������, �������� ��� � �� ���� ��������� ���� �� ����� ����� ����� ���������� ����� �������
					SPIPreviousBlock.pxNextFreeBlock = SPIBlock.pxNextFreeBlock;

					SPIFreeBytesRemaining -= SPIBlock.xBlockSize;												// ���������� ��������� ������
					if( SPIFreeBytesRemaining < SPIMinimumEverFreeBytesRemaining )
						SPIMinimumEverFreeBytesRemaining = SPIFreeBytesRemaining;								// ������� ����������� �� ����� ������

					// ���� ������������ - �� ���������� � ����������� ���������� � �� ����� ���������� �����.
					SPIBlock.xBlockSize |= SPIBlockAllocatedBit;												// ������������� ������� ��� (���� ����������� ����������.)
					SPIBlock.pxNextFreeBlock = 0;																// ������ ��� ����� ��� � ������ ���� ������

					ret = memory_write((uint8_t *)&SPIPreviousBlock,SPIPreviousBlock.pxAddresBlock,sizeof(SPI_BlockLink_t));
					if (!ret) return 0;
					ret = memory_write((uint8_t *)&SPIBlock,SPIBlock.pxAddresBlock,sizeof(SPI_BlockLink_t));
					if (!ret) return 0;
				}
			}
		}

	}
	( void ) xTaskResumeAll();

	configASSERT( ( ( uint32_t ) Return  & portBYTE_ALIGNMENT_MASK ) == 0 );

	USART_TRACE_BLUE("�������� ������ SPI:%u\n",(unsigned int)SPIFreeBytesRemaining);

	return Return;
}
/*-----------------------------------------------------------*/
void SPIPortFree( uint32_t pv )
{
	uint32_t 		puc = pv;
	SPI_BlockLink_t pxLink;
	int8_t	ret;

	if( pv != 0 )
	{
		puc -= SPIHeapStructSize;

		ret = memory_read((uint8_t *)&pxLink,puc,sizeof(SPI_BlockLink_t));
		if (!ret) {USART_TRACE_RED("������ ������������ ������ SPI.\n");}
		// ��������, ��� �� �� ����� ���� ������� ���� ������
		configASSERT( ( pxLink.xBlockSize & SPIBlockAllocatedBit ) != 0 );
		configASSERT( pxLink.pxNextFreeBlock == 0 );

		if( ( pxLink.xBlockSize & SPIBlockAllocatedBit ) != 0 )
		{
			if( pxLink.pxNextFreeBlock == 0 )
			{
				//���� ������������ � ���� - �� ������ �� �������
				pxLink.xBlockSize &= ~SPIBlockAllocatedBit;

				vTaskSuspendAll();
				{
					// �������� ���� ���� � ������ ��������� ������.
					SPIFreeBytesRemaining += pxLink.xBlockSize;
					SPIInsertBlockIntoFreeList(pxLink);
				}
				( void ) xTaskResumeAll();
			}
		}
	}
	USART_TRACE_BLUE("�������� ������ SPI:%u\n",(unsigned int)SPIFreeBytesRemaining);

}
/*-----------------------------------------------------------*/

size_t SPIPortGetFreeHeapSize( void )
{
	return SPIFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

size_t SPIPortGetMinimumEverFreeHeapSize( void )
{
	return SPIMinimumEverFreeBytesRemaining;
}
/*-----------------------------------------------------------*/

void SPIPortInitialiseBlocks( void )
{
	/* This just exists to keep the linker quiet. */
}
/*-----------------------------------------------------------*/

static void SPIHeapInit( void )
{
SPI_BlockLink_t 	SPIFirstFreeBlock;

/*
 *  ����� ��������� ��� ������ ����� �� SPI:
 * 	SPIStart 	- ����� ������ ����� ������
 * 	SPIEnd		- ����� ����� ����� ������
 *
 */
	AT45DB161D_spi_init();

	// SPIStart ������ ���� ������������ ��� �������� ��������� �� ������ ������� � ������ ��������� ������.
	SPIStart.pxAddresBlock = 0;
	SPIStart.pxNextFreeBlock = SPIHeapStructSize;		// ����� ������ ������.
	SPIStart.xBlockSize =  0;							// ������ �����.

	memory_write((uint8_t *)&SPIStart,SPIStart.pxAddresBlock ,sizeof(SPI_BlockLink_t));

	// ����� ����� ������
	SPIEnd.pxAddresBlock = SPI_DEVICE_SIZE - SPIHeapStructSize;
	SPIEnd.pxNextFreeBlock = 0;
	SPIEnd.xBlockSize = SPIHeapStructSize;

	memory_write((uint8_t *)&SPIEnd,SPIEnd.pxAddresBlock,sizeof(SPI_BlockLink_t));

	// ��� ������ ���� ���� ��������� ����, ������� ����� ����� �������, ����� ������ ��� ������������ ����, ����� ������������, �������� pxEnd.
	SPIFirstFreeBlock.pxAddresBlock = SPIStart.pxNextFreeBlock;
	SPIFirstFreeBlock.xBlockSize = SPI_DEVICE_SIZE - SPIFirstFreeBlock.pxAddresBlock - SPIHeapStructSize;
	SPIFirstFreeBlock.pxNextFreeBlock = SPIEnd.pxAddresBlock;

	memory_write((uint8_t *)&SPIFirstFreeBlock,SPIFirstFreeBlock.pxAddresBlock,sizeof(SPI_BlockLink_t));

	SPIMinimumEverFreeBytesRemaining = SPIFirstFreeBlock.xBlockSize;
	SPIFreeBytesRemaining = SPIFirstFreeBlock.xBlockSize;

	USART_TRACE_BLUE("����� ��������� ������ SPI: %u\n",SPIFreeBytesRemaining);

	SPIBlockAllocatedBit = ( ( size_t ) 1 ) << ( ( sizeof( size_t ) * BITS_PER_BYTE ) - 1 );

}
/*-----------------------------------------------------------*/
static void SPIInsertBlockIntoFreeList( SPI_BlockLink_t SPIBlockToInsert )
{
SPI_BlockLink_t 	SPIIterator,SPINextIterator;
uint32_t 			puc;
int8_t		ret;
SPI_BlockLink_t		SPIBlockToInsertTmp;

	SPIBlockToInsertTmp.pxAddresBlock 	= SPIBlockToInsert.pxAddresBlock;
	SPIBlockToInsertTmp.pxNextFreeBlock = SPIBlockToInsert.pxNextFreeBlock;
	SPIBlockToInsertTmp.xBlockSize 		= SPIBlockToInsert.xBlockSize;

	ret = memory_read((uint8_t *)&SPIIterator,0,sizeof(SPI_BlockLink_t));
	if (!ret) {USART_TRACE_RED("������ ���������� ���������� ����� ������ SPI.\n");}

	// ���� ��������� ��������� ����, ������� ��������� ����� ����� ������ SPIBlockToInsert
	while( SPIIterator.pxNextFreeBlock < SPIBlockToInsertTmp.pxAddresBlock ){
		ret = memory_read((uint8_t *)&SPIIterator, SPIIterator.pxNextFreeBlock, sizeof(SPI_BlockLink_t));
		if (!ret) {USART_TRACE_RED("������ ���������� ���������� ����� ������ SPI.\n");}

	}

	// -------------------------------------------------------------------------------------
	// [ SPIIterator(������ ������ ����) ] [ SPIBlockToInsert]
	// ���� ��� ���� ����� ������� � ������� ��������� ����,
	// � �� ��������� � ����� ������� ������� ����� �� ������ ���� ����������� ����.
	// -------------------------------------------------------------------------------------
	puc = SPIIterator.pxAddresBlock;
	if( ( puc + SPIIterator.xBlockSize ) ==  SPIBlockToInsertTmp.pxAddresBlock )		// ���� ���������� ����� ���� ������ ������ � ����� ����� � ���� ������ ���� ������
	{
		SPIIterator.xBlockSize += SPIBlockToInsertTmp.xBlockSize;						// �������� ���������� ����

		SPIBlockToInsertTmp.xBlockSize = SPIIterator.xBlockSize;						// � �������� ��� ������ ������������� ��� �������� �����.
		SPIBlockToInsertTmp.pxAddresBlock = SPIIterator.pxAddresBlock;
		SPIBlockToInsertTmp.pxNextFreeBlock = SPIIterator.pxNextFreeBlock;
	}

	// -------------------------------------------------------------------------------------
	// [ SPIBlockToInsert ] [ SPIIterator(������ ������ ����)]
	//  [ SPIIterator(������ ������ ����)] [ SPIBlockToInsert ] [ SPIIterator(������ ������ ����)] - ���� �����������
	// ���� ��� ���� ����� ������� � ������� ��������� ����,
	// � �� ��������� � ������ ������� ������� ����� �� ������ ���� ����������� ����.
	// -------------------------------------------------------------------------------------
	puc = SPIBlockToInsertTmp.pxAddresBlock;
	if( ( puc + SPIBlockToInsertTmp.xBlockSize ) == SPIIterator.pxNextFreeBlock )		// ���� ��������� ������ ������ � ����� ����� � ���� ������ ���� ������
	{
		if( SPIIterator.pxNextFreeBlock != SPIEnd.pxAddresBlock )
		{
			ret = memory_read((uint8_t *)&SPINextIterator, SPIIterator.pxNextFreeBlock, sizeof(SPI_BlockLink_t));
			if (!ret) {USART_TRACE_RED("������ ���������� ���������� ����� ������ SPI.\n");}

			SPIBlockToInsertTmp.xBlockSize += SPINextIterator.xBlockSize;
			SPIBlockToInsertTmp.pxNextFreeBlock = SPINextIterator.pxNextFreeBlock;
		}
		else
		{
			SPIBlockToInsertTmp.pxNextFreeBlock = SPIEnd.pxAddresBlock;
		}
	}
	else
	{
		SPIBlockToInsertTmp.pxNextFreeBlock = SPIIterator.pxNextFreeBlock;
	}

	// -------------------------------------------------------------------------------------

	if( SPIIterator.pxAddresBlock != SPIBlockToInsertTmp.pxAddresBlock )
	{
		SPIIterator.pxNextFreeBlock = SPIBlockToInsertTmp.pxAddresBlock;
	}

	ret = memory_write((uint8_t *)&SPIIterator,SPIIterator.pxAddresBlock,sizeof(SPI_BlockLink_t));
	if (!ret) {USART_TRACE_RED("������ ���������� ���������� ����� ������ SPI.\n");}
	ret = memory_write((uint8_t *)&SPIBlockToInsertTmp,SPIBlockToInsertTmp.pxAddresBlock,sizeof(SPI_BlockLink_t));
	if (!ret) {USART_TRACE_RED("������ ���������� ���������� ����� ������ SPI.\n");}

}

