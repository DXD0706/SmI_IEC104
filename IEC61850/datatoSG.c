/*
 * datatoSG.c
 *
 *  Created on: 03.05.2017
 *      Author: sagok
 */

#include "main.h"
#include "IEC850.h"

#include "iec850.h"
#include "iec61850_server.h"

#if defined (MR801)
#include "static_model_MR801.h"

extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern uint8_t				writeNmb;
extern uint8_t	  			writeNmbSG;			// ����� ������ �������.

/*******************************************************
 * Set_SG  �������� ������ �������
 *******************************************************/
void	Set_SG	(uint8_t num, uint64_t currentTime )
{

			uint8_t	currSG = 0;
			if (ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG0){ currSG = 1;}
			else
			if (ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG1){ currSG = 2;}

			if (currSG != IedServer_getActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0))) {
				IedServer_changeActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0),currSG );
//				USART_TRACE("���������� ������ �������. %u\n",iedModel_LD0_LLN0_sgcb0.actSG);
			}
			// -------------------- ���� �� ��������� � �������� ---------------------------
			if (ucMDiscInBuf[MB_offset_Jurnals] & MB_bOffsetSysNote) {
				USART_TRACE("���������� �������.\n");
				writeNmb = 9;
				ucMDiscInBuf[MB_offset_Jurnals] ^= MB_bOffsetSysNote;
			}

}

#endif

#if defined (MR771)
#include "static_model_MR771.h"

/*******************************************************
 * Set_SG  �������� ������ �������
 *******************************************************/
void	Set_SG	(uint8_t num, uint64_t currentTime )
{


}

#endif
