/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "port.h"
#include "cmsis_os.h"
#include "queue.h"

#include "main.h"
#include "time.h"
#include "stdlib.h"
//#include "stdbool.h"
#include "string.h"

#include "ethernetif.h"
#include "lwip/tcpip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/api.h"
#include "lwip/sys.h"

// IEC 60870-5-104
#include "iec104.h"
#include "usart.h"

// IEC 61850
#include "iec850.h"
#include "iec61850_server.h"
//#include "static_model.h"

#if defined (MR5_700)
#include "static_model_MR5_700.h"
#endif
#if defined (MR5_600)
#include "static_model_MR5_600.h"
#endif
#if defined (MR5_500)
#include "static_model_MR5_500.h"
#endif
#if defined (MR771)
#include "static_model_MR771.h"
#endif
#if defined (MR761) || defined (MR762) || defined (MR763)
#include "static_model_MR76x.h"
#endif
#if defined (MR801)
#include "static_model_MR801.h"
#endif
#if defined (MR901) || defined (MR902)
#include "static_model_MR901_902.h"
#endif

#include "datatoPTOC.h"					// ����������� ������ � PTOC
#include "datatoPTOV.h"					// ����������� ������ � PTOV PTUV
#include "datatoPTOF.h"					// ����������� ������ � PTOF PTUF
#include "datatoGGIO.h"					// ����������� ������ �
#include "datatoPDIF.h"					// ����������� ������ �
#include "datatoRREC.h"					// ����������� ������ �
#include "datatoCSWI.h"					// ����������� ������ �
#include "datatoPTRC.h"					// ����������� ������ �
#include "datatoMMXU.h"
#include "datatoMSQI.h"
#include "datatoLLN0LPHD.h"
#include "datatoSG.h"					// ��������� ������ �������
/*Modbus includes ------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"
#include "modbus.h"

/*������ SPI -----------------------------------------------------------------*/
#include "ExtSPImem.h"

/*���������� ������� �� USART ------------------------------------------------*/
#include "DebugConsole.h"

/* HTTP ������ ��� ���������� �������� ---------------------------------------*/
#include "httpServer.h"

/* NTP ������  ---------------------------------------*/
#include "sntpclient.h"


/* define --------------------------------------------------------------------*/
#define DEBUG_CONSOLE_STACK_SIZE		( configMINIMAL_STACK_SIZE * 4 )		// configMINIMAL_STACK_SIZE * 4 * N = 3072�����			// * 4 ������ ��� 32 ���� ����  ���� 5 11.07.2016
#define DEBUG_CONSOLE_TASK_PRIORITY		osPriorityAboveNormal//osPriorityNormal

#define IEC850_STACK_SIZE				( configMINIMAL_STACK_SIZE * 10 )		// 3072�����			configMINIMAL_STACK_SIZE * 7		8- 05122016 12-30012017 ������
#define IEC850Task__PRIORITY			osPriorityHigh//osPriorityNormal//osPriorityAboveNormal

#define	MODBUSTask_STACK_SIZE			( configMINIMAL_STACK_SIZE * 4 )		// 1024�����			onfigMINIMAL_STACK_SIZE * 2
#define MODBUSTask__PRIORITY			osPriorityAboveNormal//osPriorityBelowNormal//osPriorityNormal

#define	HTTPTask_STACK_SIZE				( configMINIMAL_STACK_SIZE * 4 )		// 1024�����			onfigMINIMAL_STACK_SIZE * 2
#define HTTPTask__PRIORITY				osPriorityNormal

#define DEBUG_USARTOUT_STACK_SIZE		( configMINIMAL_STACK_SIZE * 4 )
#define DEBUG_USARTOUT_TASK_PRIORITY	osPriorityRealtime


/* Variables -----------------------------------------------------------------*/
extern uint16_t	GLOBAL_QUALITY;
// �������� -----------------------------
static xSemaphoreHandle xConsoleMutex = NULL;			// ������� ������ � �������

osMutexId xIEC850StartMutex;		// ������� ���������� � ������� TCP/IP
osMutexDef(xIEC850StartMutex);

osMutexId xIEC850ServerStartMutex;		// ������� ���������� � ������� TCP/IP
osMutexDef(xIEC850ServerStartMutex);

xQueueHandle		xDebugUsartOut;			// ������� ��� �������� � ����������

extern IedServer 	iedServer;
extern uint16_t		SNTP_Period;

extern uint64_t 	nextSynchTime;
extern bool 		resynch;

// ���������� ��������� ��� ������ ������ ������� MODBUS
	typedef struct					// ��� �������� ����� ������� ��������.
	{
	  MBFrame	MBData;
	  uint8_t 	Source;
	} xData;

osThreadId defaultTaskHandle;
osThreadId IEC850TaskHandle;
osThreadId MBUSTaskHandle;
osThreadId CONSOLETaskHandle;
osThreadId DEBUGUSARTOUTTaskHandle;


extern 	RTC_HandleTypeDef hrtc;

//  --------------------------------------------------------------------------------
int8_t				Nextread = 0;
uint8_t				ReadNmb=0;
uint16_t			NumbBlokReadMB = 0;	// �����
uint8_t				writeNmb;
uint8_t	  			writeNmbSG;			// ����� ������ �������.

//Master mode: ���� ������
//Master mode: ���� ������ ������������ �������

#if defined (MR5_700)

extern uint16_t   usMDateStart;
extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   usMRevStart;
extern uint16_t   ucMRevBuf[MB_NumbWordRev];
extern uint16_t   usMDiscInStart;
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern uint16_t   usMAnalogInStart;
extern uint16_t   ucMAnalogInBuf[MB_NumbAnalog];
extern uint16_t   usMConfigStart;
extern uint16_t   ucMConfigBuf[MB_NumbConfig];
extern uint16_t   usMConfigStartall;
extern uint16_t   ucMConfigBufall[MB_NumbConfigall];
extern uint16_t   usMConfigStartSWCrash;
extern uint16_t   ucMSWCrash[MB_NumbSWCrash];

extern uint16_t	    Ktt,Ktn;
#endif
#if defined (MR5_600)

extern uint16_t   usMDateStart;
extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   usMRevStart;
extern uint16_t   ucMRevBuf[MB_NumbWordRev];
extern uint16_t   usMDiscInStart;
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern uint16_t   usMAnalogInStart;
extern uint16_t   ucMAnalogInBuf[MB_NumbAnalog];
extern uint16_t   usMConfigStart;
extern uint16_t   ucMConfigBuf[MB_NumbConfig];
extern uint16_t   usMConfigStartall;
extern uint16_t   ucMConfigBufall[MB_NumbConfigall];
extern uint16_t   usMConfigStartSWCrash;
extern uint16_t   ucMSWCrash[MB_NumbSWCrash];

extern uint16_t	    Ktn;
#endif
#if defined (MR5_500)

extern uint16_t   usMDateStart;
extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   usMRevStart;
extern uint16_t   ucMRevBuf[MB_NumbWordRev];
extern uint16_t   usMDiscInStart;
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern uint16_t   usMAnalogInStart;
extern uint16_t   ucMAnalogInBuf[MB_NumbAnalog];
extern uint16_t   usMConfigStart;
extern uint16_t   ucMConfigBuf[MB_NumbConfig];
extern uint16_t   usMConfigStartall;
extern uint16_t   ucMConfigBufall[MB_NumbConfigall];
extern uint16_t   usMConfigStartSWCrash;
extern uint16_t   ucMSWCrash[MB_NumbSWCrash];

extern uint16_t	    Ktt,Ktn;
#endif

#if defined (MR771)

extern uint16_t   usMDateStart;
extern uint16_t   usMRevStart;
extern uint16_t   usMDiscInStart;
extern uint16_t   usMAnalogInStart;
extern uint16_t   usMConfigStartSW;
extern uint16_t   usConfigUstavkiStart;			// ������ �������
extern uint16_t   usConfigAutomatStart;			// ��������� ����������
extern uint16_t   usConfigVLSInStart;			// ������ ������������ ������� ���������� ��������
extern uint16_t   usConfigVLSOutStart;			// ������ ������������ �������� ���������� ��������
extern uint16_t   usConfigAPWStart;				// ������������ ���
extern uint16_t   usSystemCfgStart;				// ��������� �������
extern uint16_t   usConfigTRMeasStart;			// ������������ �������������� ������
extern uint16_t   usSGStart;					// ��������� ������ �������


extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern	uint16_t   ucSGBuf[MB_NumbSG];

#endif
/*************************************************************************
 * MR761 MR762 MR763
 *************************************************************************/
#if defined (MR761) || defined (MR762) || defined (MR763)

extern uint16_t   usMDateStart;
extern uint16_t   usMRevStart;
extern uint16_t   usMDiscInStart;
extern uint16_t   usMAnalogInStart;
extern uint16_t   usMConfigStartSW;
extern uint16_t   usConfigUstavkiStart;			// ������ �������
extern uint16_t   usConfigAutomatStart;			// ��������� ����������
extern uint16_t   usConfigVLSInStart;			// ������ ������������ ������� ���������� ��������
extern uint16_t   usConfigVLSOutStart;			// ������ ������������ �������� ���������� ��������
extern uint16_t   usConfigAPWStart;				// ������������ ���
extern uint16_t   usSystemCfgStart;				// ��������� �������
extern uint16_t   usConfigTRMeasStart;			// ������������ �������������� ������
extern uint16_t   usSGStart;					// ��������� ������ �������


extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
extern	uint16_t   ucSGBuf[MB_NumbSG];

#endif
#if defined (MR801)

extern uint16_t   usMDateStart;
extern uint16_t   usMRevStart;
extern uint16_t   usMDiscInStart;
extern uint16_t   usMAnalogInStart;
extern uint16_t   usMConfigStartSW;
extern uint16_t   usConfigUstavkiStart;			// ������ �������
extern uint16_t   usConfigAutomatStart;			// ��������� ����������
extern uint16_t   usConfigVLSInStart;			// ������ ������������ ������� ���������� ��������
extern uint16_t   usConfigVLSOutStart;			// ������ ������������ �������� ���������� ��������
extern uint16_t   usConfigAPWStart;				// ������������ ���
extern uint16_t   usConfigAWRStart;				// ������������ ���
extern uint16_t   usSystemCfgStart;				// ��������� �������
extern uint16_t   usConfigTRMeasStart;			// ������������ �������������� ������
extern uint16_t   usConfigTRPWRStart;			// ������������ �������� ������


extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
#endif

#if defined (MR901) || defined (MR902)

extern uint16_t   usMDateStart;
extern uint16_t   usMRevStart;
extern uint16_t   usMDiscInStart;
extern uint16_t   usMAnalogInStart;
extern uint16_t   usMConfigStartSW;
extern uint16_t   usConfigUstavkiStart;			// ������ �������
extern uint16_t   usConfigAutomatStart;			// ��������� ����������
extern uint16_t   usConfigVLSInStart;			// ������ ������������ ������� ���������� ��������
extern uint16_t   usConfigVLSOutStart;			// ������ ������������ �������� ���������� ��������
extern uint16_t   usConfigAPWStart;				// ������������ ���
extern uint16_t   usConfigAWRStart;				// ������������ ���
extern uint16_t   usSystemCfgStart;				// ��������� �������
extern uint16_t   usConfigTRMeasStart;			// ������������ �������������� ������
extern uint16_t   usConfigTRPWRStart;			// ������������ �������� ������


extern uint16_t   ucMDateBuf[MB_NumbDate];
extern uint16_t   ucMDiscInBuf[MB_NumbDiscreet];
#endif
//  --------------------------------------------------------------------------------

struct netconn *conn, *newconn;
struct netconn *conn850,*newconn850;

struct netif 	first_gnetif,second_gnetif,gnetif;
struct ip_addr 	first_ipaddr,second_ipaddr;
struct ip_addr 	netmask;
struct ip_addr 	gw;

/* Semaphore to signal Ethernet Link state update */
osSemaphoreId Netif_LinkSemaphore = NULL;
/* Ethernet link thread Argument */
struct link_str link_arg;

struct iechooks default_hooks;
struct iecsock 	*s;

uint8_t *outbuf;
size_t  outbufLen;

/* Function prototypes -------------------------------------------------------*/

//int16_t		GetDirGeneral(uint8_t	Currdata);
//float		GetRealU(uint16_t	Currdata, uint16_t	ktn);
float		GetRealP(float	Currdata, uint16_t	ktn,  uint16_t	Ittf);

void StartTimersTask(void const * argument);
void StartMODBUSTask(void const * argument);

extern volatile uint8_t	MAC_ADDR[6];


void FREERTOS_Init(void);

extern void vRegisterDEBUGCommands( void );

/* Hook prototypes */

void ReStartIEC850_task(void) {

	USART_TRACE_BLUE("�������. \n");
	NVIC_SystemReset();

}
/*************************************************************************
 * FREERTOS_Init
 *************************************************************************/
void FREERTOS_Init(void) {
 size_t	fre,fre_pr;
 /* BEGIN RTOS_MUTEX */
	// �������� ������� ��� ���������� ������� � �������
	xConsoleMutex = osMutexCreate(NULL);

	// �������� ������� ��� ���������� ������� � TCP/IP �����
	xIEC850StartMutex = osMutexCreate(NULL);
	// �������� ������� ��� ���������� ������� � ������ �������
	xIEC850ServerStartMutex = osMutexCreate(NULL);

	osMutexWait(xIEC850StartMutex,0);						// ������� �������
	osMutexWait(xIEC850ServerStartMutex,0);				// ������� �������

  /* END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
//	vSemaphoreCreateBinary( xBinarySemaphore );			//��������� �������� �������
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */



	fre = xPortGetFreeHeapSize();			// ������ ����
	fre_pr = fre;
	USART_TRACE("������ ����:%u ����\n",fre);

	osThreadDef(MBUS, StartMODBUSTask, MODBUSTask__PRIORITY ,0, MODBUSTask_STACK_SIZE);//256
	MBUSTaskHandle = osThreadCreate(osThread(MBUS), NULL);

	osThreadDef(IEC850, StartIEC850Task,IEC850Task__PRIORITY,0, IEC850_STACK_SIZE);//1024		//1500 �������� IEC850Task__PRIORITY
	IEC850TaskHandle = osThreadCreate(osThread(IEC850), NULL);

	// ������ ������, ������� ��������� ������� � ������� USART �����.
//	osThreadDef(CONSOLE, DEBUGConsoleTask, DEBUG_CONSOLE_TASK_PRIORITY ,0, DEBUG_CONSOLE_STACK_SIZE);
//	CONSOLETaskHandle = osThreadCreate(osThread(CONSOLE), NULL);
	// ������������ ������� �������
//	vRegisterDEBUGCommands();


//	osThreadDef(DEBUG_OUT, DEBUGUSARTOUTTask, DEBUG_USARTOUT_TASK_PRIORITY ,0, DEBUG_USARTOUT_STACK_SIZE);
//	DEBUGUSARTOUTTaskHandle = osThreadCreate(osThread(DEBUG_OUT), NULL);

//	osThreadDef(HTTP, StartHTTPTask, HTTPTask__PRIORITY ,0, HTTPTask_STACK_SIZE);
//	MBUSTaskHandle = osThreadCreate(osThread(HTTP), NULL);

 //  fre = xPortGetFreeHeapSize();
 //  USART_TRACE("���� IEC850:%u ����\n",fre_pr - fre);
 //  fre_pr = fre;

/*
  osThreadDef(IEC104, StartIEC104Task, osPriorityAboveNormal,0, 640);//1024
  defaultTaskHandle = osThreadCreate(osThread(IEC104), NULL);
  fre = xPortGetFreeHeapSize();
  USART_TRACE("FreeHeap(IEC104):%u\n",fre);

  // �������� ���� ������ ����� IEC104. ������� ����� ������ ���
  if (defaultTaskHandle)
  {
	  osThreadDef(Timers, StartTimersTask, osPriorityNormal,0, 128);//128
	  defaultTaskHandle = osThreadCreate(osThread(Timers), NULL);

	  fre = xPortGetFreeHeapSize();
	  USART_TRACE("FreeHeap(Timers):%u\n",fre);
  }
*/

//  fre = xPortGetFreeHeapSize();
//  USART_TRACE("���� MBUS:%u ����\n",fre_pr - fre);
//  fre_pr = fre;

 //   fre = xPortGetFreeHeapSize();			// ������ ����
//   USART_TRACE("���� CONSOLE:%u ����\n",fre_pr - fre);
//   fre_pr = fre;

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
//   xQueueMODBUS = xQueueCreate( 3, sizeof(xData) );

  /* definition and creation of xQueueMODBUS */
//  osMessageQDef(xQueueMODBUS, 3, sizeof(xData));
//  xQueueMODBUSHandle = osMessageCreate(osMessageQ(xQueueMODBUS), NULL);

   fre = xPortGetFreeHeapSize();
   USART_TRACE("�������� %u ����\n",fre);

  /* USER CODE END RTOS_QUEUES */
}

/*************************************************************************
 * MR801
 *************************************************************************/
#if defined (MR801)
void StartMODBUSTask(void const * argument)
{
	uint16_t			NumbBlokReadMBLim=0;
	uint16_t			SizeBlokReadMB = MaxSizeBlok;			// ������ ������ ������
	uint16_t			addrConfig;

	uint8_t				LimitRead=0xFE;
	uint32_t			TimerReadMB;
	eMBErrorCode		errorType;
	uint64_t 			currentTime;
	RTC_TimeTypeDef 	Time;


	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

	writeNmb = 0;
	ReadNmb = 0;

	Port_On(LED1);

	TimerReadMB = HAL_GetTick();

	for(;;)
	{
   	   	if (writeNmb == 10) {
   	   		USART_TRACE_GREEN("1.��������� ����� � �������.\n");
   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
   	   	}
   	   	else
		if (((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS)||(Nextread)){		// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			Nextread = false;
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>2 && ReadNmb<10)ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� 0.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SGroupe0,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 8) {
	   	   		USART_TRACE_RED("������������ ������ ������� 1.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SGroupe1,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_GREEN("2.��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ -----------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ������ ���� �� �������� NTP -----
	   	     if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = LimitRead;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	   	     }
			// ������� ����� ������ ������ ��� ������ �� ������� ��������, �������� ���� ������� ������
			if ((ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG0) && ((ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG1) == 0))
					usConfigUstavkiStart = MB_StartUstavkiaddr0;
			 else	usConfigUstavkiStart = MB_StartUstavkiaddr1;



			// ������������� ������
	   	   	if (ReadNmb==0)  		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);					// ������ ������� ����������
	   	   	else if (ReadNmb==1)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==2) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);				// ������ ���������� ����

	   		// ����������� ������
	   	   	else if (ReadNmb==10)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==11) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);						// ������ �������� �������
	   	   	else if (ReadNmb==12) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSystemCfgStart,MB_NumbSystemCfg,RT_WAITING_FOREVER);					// ������ ������������ �������
	   	  	else if (ReadNmb==13) 	ReadNmb = 19;	// ������� � 20

	   	  	else if (ReadNmb==20) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStartSW,MB_NumbConfigSW,RT_WAITING_FOREVER);}				// ������ ������������ �����������
	   	  	else if (ReadNmb==21) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAPWStart,MB_NumbConfigAPW,RT_WAITING_FOREVER);}			// ������ ������������ ���
	   	  	else if (ReadNmb==22) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAWRStart,MB_NumbConfigAWR,RT_WAITING_FOREVER);}			// ������ ������������ ���+���
	   	  	else if (ReadNmb==23) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRPWRStart,MB_NumbConfigTRPWR,RT_WAITING_FOREVER);}		// ������ ���� ������� �������� ������
	   	  	else if (ReadNmb==24) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRMeasStart,MB_NumbConfigTRMeas,RT_WAITING_FOREVER);}		// ������ ���� ������� �������������� ������
	   	  	else if (ReadNmb==25) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSIn,MB_NumbConfigVLSIn,RT_WAITING_FOREVER);}		// ������ ������������ ������� ���������� ��������
	   	  	else if (ReadNmb==26) {																													// ������ ������������ �������� ���������� ��������

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbConfigVLSOut;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = MB_StartConfigVLSOut + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbConfigVLSOut;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}


	   	  				if (NumbBlokReadMB > NumbBlokReadMBLim){
	   	  					addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
	   	  				} else
	   	  				if((SizeBlokReadMB) && addrConfig<(MB_StartConfigVLSOut+MB_NumbConfigVLSOut)){
	   					   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
	   					   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
	   	  				}

	   	  			}
	   	  	else if (ReadNmb==27) {																													// ������ �������� ��� ��������� ������ �������

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbUstavki;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = usConfigUstavkiStart + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbUstavki;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}else{
	   	  				}

	   	  			   if((SizeBlokReadMB) && addrConfig<(usConfigUstavkiStart+MB_NumbUstavki)) {
	   	  				   	   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
	   	  					   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
	   	  			   	  }
	   	  				else {
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}

	   	  				if((SizeBlokReadMB != MaxSizeBlok) && (NumbBlokReadMB > NumbBlokReadMBLim))	{																																			// ���������
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}
	   	  			}
//	   	  	else if (ReadNmb==26) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSOut,MB_NumbConfigVLSOut,RT_WAITING_FOREVER);}		// ������ ������������ �������� ���������� ��������
//	   	  	else if (ReadNmb==27) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigUstavkiStart,MB_NumbUstavki,RT_WAITING_FOREVER);}			// ������ �������� ��� ��������� ������ �������
	   	  	else if (ReadNmb==28) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAutomatStart,MB_NumbAutomat,RT_WAITING_FOREVER);}			// ������ ���������� ����������
	   	  	else {ReadNmb++;}

//	   	 ReadNmb = 29;
	   	   	LimitRead = 29;
	   	   	if (ReadNmb >= LimitRead) { ReadNmb = 1; osMutexRelease(xIEC850StartMutex);}

	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;			// ���� ���� ������ � ������� ������ �� ���� ��������� �������. ����� ��� ����������.
	   	   		writeNmb = 0;
	   	   	}
/*******************************************************
 * ��������� ������������ ������� ���������
 *******************************************************/
			if(IEC850TaskHandle && (iedServer != NULL)){
				IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

// LLN0 Health --------
					Set_LLN0(1,currentTime);
// PROT_IPTOC1..8 ------						������ �� ��������� ����
					Set_IPTOC(8,currentTime);
// PROT_I20PTOC1..6 ------						������ �� ��������� �� ����� � ��������� ���� �������� ������������������
					Set_I20PTOC(6,currentTime);
// PROT_UPTOV1..4 ------						������ �� ��������� ����������
					Set_UPTOV(4,currentTime);
// PROT_UPTUV1..4 ------						������ �� ��������� ����������
					Set_UPTUV(4,currentTime);
// PROT_UPTOF1..4 ------						������ �� ��������� �������
					Set_PTOF(4,currentTime);
// PROT_UPTUF1..4 ------						������ �� ��������� �������
					Set_PTUF(4,currentTime);
// PROT_VZGGIO1..16 ------						������� ������
					Set_VZGGIO(16,currentTime);
// PROT_IDPDIF ------							���� ������
					Set_IDPDIF(1,currentTime);
// PROT_IDDPDIF ------							���� ������
					Set_IDDPDIF(1,currentTime);
// PROT_IDDMPDIF ------							���� ������
					Set_IDDMPDIF(1,currentTime);
// PROT_ID0PDIF ------							���� ������
					Set_ID0PDIF(3,currentTime);
// PROT_RREC ------
					Set_RREC(1,currentTime);
// PROT_AVRGGIO ------
					Set_AVRGGIO(1,currentTime);
// PROT_LZSHPTOC ------									���
					Set_LZSHPTOC(1,currentTime);
// PROT_RREC ------
					Set_RBRF(1,currentTime);
// CTRL_LLN0 ------

// CTRL_CSWI ------
					Set_CSWI(1,currentTime);
// CTRL_PTRC ------
					Set_PTRC(1,currentTime);
// CTRL_XCBR ------
					Set_XCBR(1,currentTime);
// CTRL_GGIO ------
					Set_XCBRGGIO(1,currentTime);
// MES_MMXU ------
					Set_MMXU(1,currentTime);
// MES_MSQI ------
					Set_MSQI(1,currentTime);

// GGIO_IN24GGIO ------
					Set_INGGIO(24,currentTime);
					Set_OUTGGIO(18,currentTime);
					Set_LEDGGIO(12,currentTime);
					Set_SSLGGIO(32,currentTime);
					Set_VLSGGIO(16,currentTime);
					Set_LSGGIO(16,currentTime);
// LD0 SG
					Set_SG(0,currentTime);

				IedServer_unlockDataModel(iedServer);
			}
/*******************************************************
 *
 *******************************************************/
		}
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
//    		ReadNmb++;
			GLOBAL_QUALITY =  QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_OLD_DATA;
    		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR || errorType == MB_ESENT){
			if (writeNmb == 10) {writeNmb = 0;}
//	   		USART_TRACE_RED("MB_ENOERR: %u\n",ReadNmb);
		}else
		if (errorType == MB_ERECVDATA){
			GLOBAL_QUALITY = QUALITY_VALIDITY_GOOD;
//			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
			Nextread = true;
		}


		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR761 MR762 MR763
 *******************************************************/
#if defined	(MR761) || defined	(MR762) || defined	(MR763)
void StartMODBUSTask(void const * argument)
{
	uint16_t			NumbBlokReadMBLim=0;
	uint16_t			SizeBlokReadMB = MaxSizeBlok;			// ������ ������ ������
	uint16_t			addrConfig;

	uint8_t				LimitRead=0xFE;
	uint32_t			TimerReadMB;
	eMBErrorCode		errorType;
	uint64_t 			currentTime;
	RTC_TimeTypeDef 	Time;


	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

	writeNmb = 0;
	ReadNmb = 0;

	Port_On(LED1);

	TimerReadMB = HAL_GetTick();

	for(;;)
	{
   	   	if (writeNmb == 10) {
   	   		USART_TRACE_GREEN("1.��������� ����� � �������.\n");
   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
   	   	}
   	   	else
		if (((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS)||(Nextread)){		// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			Nextread = false;
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>2 && ReadNmb<10)ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� %u\n",writeNmbSG);
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_Startaddr_SG,writeNmbSG - 1,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_GREEN("2.��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ -----------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ������ ���� �� �������� NTP -----
	   	     if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = LimitRead;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	   	     }
			// ������� ����� ������ ������ ��� ������ �� ������� ��������, �������� ���� ������� ������

			usConfigUstavkiStart = ucSGBuf[0];

//TODO: �������
			ReadNmb = 29;

			// ������������� ������
	   	   	if (ReadNmb==0)  		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);					// ������ ������� ����������
	   	   	else if (ReadNmb==1)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==2) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);				// ������ ���������� ����

	   		// ����������� ������
	   	   	else if (ReadNmb==10) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);						// ������ �������� �������
	   	   	else if (ReadNmb==11) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSystemCfgStart,MB_NumbSystemCfg,RT_WAITING_FOREVER);				// ������ ������������ �������
	   	   	else if (ReadNmb==12) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSGStart,MB_NumbSG,RT_WAITING_FOREVER);							// ������ ������ �������

	   	   	else if (ReadNmb==13) 	ReadNmb = 20;	// ������� � 20

	   	  	else if (ReadNmb==20) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStartSW,MB_NumbConfigSW,RT_WAITING_FOREVER);}				// ������ ������������ �����������
	   	  	else if (ReadNmb==21) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAPWStart,MB_NumbConfigAPW,RT_WAITING_FOREVER);}			// ������ ������������ ���
//	   	  	else if (ReadNmb==22) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAWRStart,MB_NumbConfigAWR,RT_WAITING_FOREVER);}			// ������ ������������ ���+���
//	   	  	else if (ReadNmb==23) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRPWRStart,MB_NumbConfigTRPWR,RT_WAITING_FOREVER);}		// ������ ���� ������� �������� ������
	   	  	else if (ReadNmb==24) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRMeasStart,MB_NumbConfigTRMeas,RT_WAITING_FOREVER);}		// ������ ���� ������� �������������� ������
	   	  	else if (ReadNmb==25) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSIn,MB_NumbConfigVLSIn,RT_WAITING_FOREVER);}		// ������ ������������ ������� ���������� ��������
	   	  	else if (ReadNmb==26) {																													// ������ ������������ �������� ���������� ��������

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

						NumbBlokReadMBLim = (uint16_t)MB_NumbConfigVLSOut;
						NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

						addrConfig = MB_StartConfigVLSOut + NumbBlokReadMB * MaxSizeBlok;

						if (NumbBlokReadMB == NumbBlokReadMBLim){
							SizeBlokReadMB = (uint16_t)MB_NumbConfigVLSOut;
							SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
						}


						if (NumbBlokReadMB > NumbBlokReadMBLim){
							addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
						} else
						if((SizeBlokReadMB) && addrConfig<(MB_StartConfigVLSOut+MB_NumbConfigVLSOut)){
						   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
						   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
						}

	   	  			}
	   	  	else if (ReadNmb==27) {																													// ������ �������� ��� ��������� ������ �������

						if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

						NumbBlokReadMBLim = (uint16_t)MB_NumbUstavki;
						NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

						addrConfig = usConfigUstavkiStart + NumbBlokReadMB * MaxSizeBlok;

						if (NumbBlokReadMB == NumbBlokReadMBLim){
							SizeBlokReadMB = (uint16_t)MB_NumbUstavki;
							SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
						}

						if (NumbBlokReadMB > NumbBlokReadMBLim){
							addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
						} else
					   if((SizeBlokReadMB) && addrConfig<(usConfigUstavkiStart+MB_NumbUstavki)) {
							   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
							   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
						  }
	   	  			}
//	   	  	else if (ReadNmb==26) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSOut,MB_NumbConfigVLSOut,RT_WAITING_FOREVER);}		// ������ ������������ �������� ���������� ��������
//	   	  	else if (ReadNmb==27) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigUstavkiStart,MB_NumbUstavki,RT_WAITING_FOREVER);}			// ������ �������� ��� ��������� ������ �������
	   	  	else if (ReadNmb==28) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAutomatStart,MB_NumbAutomat,RT_WAITING_FOREVER);}			// ������ ���������� ����������
	   	  	else {ReadNmb++;}

	   	   	LimitRead = 29;
	   	   	if (ReadNmb >= LimitRead) { ReadNmb = 1; osMutexRelease(xIEC850StartMutex);}

	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;			// ���� ���� ������ � ������� ������ �� ���� ��������� �������. ����� ��� ����������.
	   	   		writeNmb = 0;
	   	   	}
/*******************************************************
 * ��������� ������������ ������� ���������
 *******************************************************/
			if(IEC850TaskHandle && (iedServer != NULL)){
				IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

// PROT_LLN0 Health --------
				{
					uint32_t	LLN0_Health = STVALINT32_OK;
					if ((ucMDiscInBuf[MB_offsetHardFaults] & MB_bOffsetModule4) || (ucMDiscInBuf[MB_offsetHardFaults] & MB_bOffsetModule5)) {LLN0_Health = STVALINT32_Warning;}
					IedServer_updateInt32AttributeValue(iedServer, &iedModel_PROT_LLN0_Health_stVal, LLN0_Health);
					IedServer_updateUTCTimeAttributeValue(iedServer,&iedModel_PROT_LLN0_Health_t, currentTime);
				}
// PROT_IPTOC1..8 ------						������ �� ��������� ����
//					Set_IPTOC(8,currentTime);
// Set_I2I1PTOC ------
//					Set_I2I1PTOC(1,currentTime);
// Set_IPTUC ------
//					Set_IPTUC(1,currentTime);
// PROT_I20PTOC1..6 ------						������ �� ��������� �� ����� � ��������� ���� �������� ������������������
//					Set_I20PTOC(6,currentTime);
// PROT_UPTOV1..4 ------						������ �� ��������� ����������
//					Set_UPTOV(4,currentTime);
// PROT_UPTUV1..4 ------						������ �� ��������� ����������
//					Set_UPTUV(4,currentTime);
// PROT_UPTOF1..4 ------						������ �� ��������� �������
//					Set_PTOF(4,currentTime);
// PROT_UPTUF1..4 ------						������ �� ��������� �������
//					Set_PTUF(4,currentTime);
// PROT_VZGGIO1..16 ------						������� ������
//					Set_VZGGIO(16,currentTime);
// PROT_RREC ------
//					Set_RREC(1,currentTime);
// PROT_RREC ------
//					Set_RBRF(1,currentTime);

// CTRL_CSWI ------
//					Set_CSWI(1,currentTime);
// CTRL_PTRC ------
//					Set_PTRC(1,currentTime);
// CTRL_XCBR ------
//					Set_XCBR(1,currentTime);
// CTRL_GGIO ------
//					Set_XCBRGGIO(1,currentTime);
// MES_MMXU ------
//					Set_MMXU(1,currentTime);

// GGIO_IN24GGIO ------
//					Set_INGGIO(24,currentTime);
//					Set_OUTGGIO(18,currentTime);
//					Set_LEDGGIO(12,currentTime);
//					Set_SSLGGIO(32,currentTime);
//					Set_VLSGGIO(16,currentTime);
//					Set_LSGGIO(16,currentTime);
// LD0 SG
//					Set_SG(0,currentTime);

				IedServer_unlockDataModel(iedServer);
			}
/*******************************************************
 *
 *******************************************************/
		}
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
//    		ReadNmb++;
			GLOBAL_QUALITY =  QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_OLD_DATA;
    		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR || errorType == MB_ESENT){
			if (writeNmb == 10) {writeNmb = 0;}
//	   		USART_TRACE_RED("MB_ENOERR: %u\n",ReadNmb);
		}else
		if (errorType == MB_ERECVDATA){
			GLOBAL_QUALITY = QUALITY_VALIDITY_GOOD;
//			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
			Nextread = true;
		}


		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR901 MR902
 *******************************************************/
#if defined	(MR901) || defined	(MR902)
void StartMODBUSTask(void const * argument)
{
	uint16_t			NumbBlokReadMBLim=0;
	uint16_t			SizeBlokReadMB = MaxSizeBlok;			// ������ ������ ������
	uint16_t			addrConfig;

	uint8_t				LimitRead=0xFE;
	uint32_t			TimerReadMB;
	eMBErrorCode		errorType;
	uint64_t 			currentTime;
	RTC_TimeTypeDef 	Time;


	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

	writeNmb = 0;
	ReadNmb = 0;

	Port_On(LED1);

	TimerReadMB = HAL_GetTick();

	for(;;)
	{
   	   	if (writeNmb == 10) {
   	   		USART_TRACE_GREEN("1.��������� ����� � �������.\n");
   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
   	   	}
   	   	else
		if (((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS)||(Nextread)){		// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			Nextread = false;
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>2 && ReadNmb<10)ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� 0.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SGroupe0,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 8) {
	   	   		USART_TRACE_RED("������������ ������ ������� 1.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SGroupe1,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_GREEN("2.��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ -----------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ������ ���� �� �������� NTP -----
	   	     if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = LimitRead;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	   	     }
			// ������� ����� ������ ������ ��� ������ �� ������� ��������, �������� ���� ������� ������
			if ((ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG0) && ((ucMDiscInBuf[MB_offset_SG] & MB_bOffsetSG1) == 0))
					usConfigUstavkiStart = MB_StartUstavkiaddr0;
			 else	usConfigUstavkiStart = MB_StartUstavkiaddr1;



			// ������������� ������
	   	   	if (ReadNmb==0)  		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);					// ������ ������� ����������
	   	   	else if (ReadNmb==1)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==2) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);				// ������ ���������� ����

	   		// ����������� ������
	   	   	else if (ReadNmb==10)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==11) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);						// ������ �������� �������
	   	   	else if (ReadNmb==12) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSystemCfgStart,MB_NumbSystemCfg,RT_WAITING_FOREVER);				// ������ ������������ �������
	   	  	else if (ReadNmb==13) 	ReadNmb = 19;	// ������� � 20

	   	  	else if (ReadNmb==20) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStartSW,MB_NumbConfigSW,RT_WAITING_FOREVER);}				// ������ ������������ �����������
	   	  	else if (ReadNmb==21) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAPWStart,MB_NumbConfigAPW,RT_WAITING_FOREVER);}			// ������ ������������ ���
	   	  	else if (ReadNmb==22) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAWRStart,MB_NumbConfigAWR,RT_WAITING_FOREVER);}			// ������ ������������ ���+���
	   	  	else if (ReadNmb==23) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRPWRStart,MB_NumbConfigTRPWR,RT_WAITING_FOREVER);}		// ������ ���� ������� �������� ������
	   	  	else if (ReadNmb==24) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRMeasStart,MB_NumbConfigTRMeas,RT_WAITING_FOREVER);}		// ������ ���� ������� �������������� ������
	   	  	else if (ReadNmb==25) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSIn,MB_NumbConfigVLSIn,RT_WAITING_FOREVER);}		// ������ ������������ ������� ���������� ��������
	   	  	else if (ReadNmb==26) {																													// ������ ������������ �������� ���������� ��������

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbConfigVLSOut;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = MB_StartConfigVLSOut + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbConfigVLSOut;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}


	   	  				if (NumbBlokReadMB > NumbBlokReadMBLim){
	   	  					addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
	   	  				} else
	   	  				if((SizeBlokReadMB) && addrConfig<(MB_StartConfigVLSOut+MB_NumbConfigVLSOut)){
	   					   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
	   					   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
	   	  				}

	   	  			}
	   	  	else if (ReadNmb==27) {																													// ������ �������� ��� ��������� ������ �������

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbUstavki;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = usConfigUstavkiStart + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbUstavki;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}else{
	   	  				}

	   	  			   if((SizeBlokReadMB) && addrConfig<(usConfigUstavkiStart+MB_NumbUstavki)) {
	   	  				   	   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
	   	  					   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
	   	  			   	  }
	   	  				else {
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}

	   	  				if((SizeBlokReadMB != MaxSizeBlok) && (NumbBlokReadMB > NumbBlokReadMBLim))	{																																			// ���������
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}
	   	  			}
//	   	  	else if (ReadNmb==26) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSOut,MB_NumbConfigVLSOut,RT_WAITING_FOREVER);}		// ������ ������������ �������� ���������� ��������
//	   	  	else if (ReadNmb==27) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigUstavkiStart,MB_NumbUstavki,RT_WAITING_FOREVER);}			// ������ �������� ��� ��������� ������ �������
	   	  	else if (ReadNmb==28) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAutomatStart,MB_NumbAutomat,RT_WAITING_FOREVER);}			// ������ ���������� ����������
	   	  	else {ReadNmb++;}

//	   	 ReadNmb = 29;
	   	   	LimitRead = 29;
	   	   	if (ReadNmb >= LimitRead) { ReadNmb = 1; osMutexRelease(xIEC850StartMutex);}

	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;			// ���� ���� ������ � ������� ������ �� ���� ��������� �������. ����� ��� ����������.
	   	   		writeNmb = 0;
	   	   	}
/*******************************************************
 * ��������� ������������ ������� ���������
 *******************************************************/
			if(IEC850TaskHandle && (iedServer != NULL)){
				IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

// LLN0 Health --------
					Set_LLN0(1,currentTime);
// PROT_IPTOC1..8 ------						������ �� ��������� ����
//					Set_IPTOC(8,currentTime);
// PROT_I20PTOC1..6 ------						������ �� ��������� �� ����� � ��������� ���� �������� ������������������
//					Set_I20PTOC(6,currentTime);
// PROT_UPTOV1..4 ------						������ �� ��������� ����������
//					Set_UPTOV(4,currentTime);
// PROT_UPTUV1..4 ------						������ �� ��������� ����������
//					Set_UPTUV(4,currentTime);
// PROT_UPTOF1..4 ------						������ �� ��������� �������
//					Set_PTOF(4,currentTime);
// PROT_UPTUF1..4 ------						������ �� ��������� �������
//					Set_PTUF(4,currentTime);
// PROT_VZGGIO1..16 ------						������� ������
//					Set_VZGGIO(16,currentTime);
// PROT_IDPDIF ------							���� ������
//					Set_IDPDIF(1,currentTime);
// PROT_IDDPDIF ------							���� ������
//					Set_IDDPDIF(1,currentTime);
// PROT_IDDMPDIF ------							���� ������
//					Set_IDDMPDIF(1,currentTime);
// PROT_ID0PDIF ------							���� ������
//					Set_ID0PDIF(3,currentTime);
// PROT_RREC ------
//					Set_RREC(1,currentTime);
// PROT_AVRGGIO ------
//					Set_AVRGGIO(1,currentTime);
// PROT_LZSHPTOC ------									���
//					Set_LZSHPTOC(1,currentTime);
// PROT_RREC ------
//					Set_RBRF(1,currentTime);
// CTRL_LLN0 ------

// CTRL_CSWI ------
//					Set_CSWI(1,currentTime);
// CTRL_PTRC ------
//					Set_PTRC(1,currentTime);
// CTRL_XCBR ------
//					Set_XCBR(1,currentTime);
// CTRL_GGIO ------
//					Set_XCBRGGIO(1,currentTime);
// MES_MMXU ------
//					Set_MMXU(1,currentTime);
// MES_MSQI ------
//					Set_MSQI(1,currentTime);

// GGIO_IN24GGIO ------
//					Set_INGGIO(24,currentTime);
//					Set_OUTGGIO(18,currentTime);
//					Set_LEDGGIO(12,currentTime);
//					Set_SSLGGIO(32,currentTime);
//					Set_VLSGGIO(16,currentTime);
//					Set_LSGGIO(16,currentTime);
// LD0 SG
//					Set_SG(0,currentTime);

				IedServer_unlockDataModel(iedServer);
			}
/*******************************************************
 *
 *******************************************************/
		}
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
//    		ReadNmb++;
			GLOBAL_QUALITY =  QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_OLD_DATA;
    		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR || errorType == MB_ESENT){
			if (writeNmb == 10) {writeNmb = 0;}
//	   		USART_TRACE_RED("MB_ENOERR: %u\n",ReadNmb);
		}else
		if (errorType == MB_ERECVDATA){
			GLOBAL_QUALITY = QUALITY_VALIDITY_GOOD;
			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
			Nextread = true;
		}

		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR771 MODBUS
 *******************************************************/
#if defined	(MR771)
void StartMODBUSTask(void const * argument)
{
	uint16_t			NumbBlokReadMBLim=0;
	uint16_t			SizeBlokReadMB = MaxSizeBlok;			// ������ ������ ������
	uint16_t			addrConfig;

	uint8_t				LimitRead=0xFE;
	uint32_t			TimerReadMB;
	eMBErrorCode		errorType;
	uint64_t 			currentTime;
	RTC_TimeTypeDef 	Time;


	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

	writeNmb = 0;
	ReadNmb = 0;

	Port_On(LED1);

	TimerReadMB = HAL_GetTick();

	for(;;)
	{
   	   	if (writeNmb == 10) {
   	   		USART_TRACE_GREEN("1.��������� ����� � �������.\n");
   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
   	   	}
   	   	else
		if (((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS)||(Nextread)){		// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			Nextread = false;
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>2 && ReadNmb<10)ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� %u\n",writeNmbSG);
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_Startaddr_SG,writeNmbSG - 1,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_Sw,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_GREEN("2.��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (USHORT *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ -----------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ������ ���� �� �������� NTP -----
	   	     if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = LimitRead;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	   	     }

			// ������� ����� ������ ������ ��� ������ �� ������� ��������, �������� ���� ������� ������
	   	     {
	   	    	 uint16_t	adin = (uint16_t)MB_NumbUstavki;
	   	    	 adin = adin * ucSGBuf[0];
	   	    	 usConfigUstavkiStart = (uint16_t)MB_StartUstavkiaddr0 + adin;
	   	     }

//TODO: �������
//			ReadNmb = 29;

			// ������������� ������
	   	   	if (ReadNmb==0)  		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);					// ������ ������� ����������
	   	   	else if (ReadNmb==1)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==2) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);				// ������ ���������� ����

	   		// ����������� ������
	   	   	else if (ReadNmb==10)	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);				// ������ ���������� ����
	   	   	else if (ReadNmb==11) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);						// ������ �������� �������
	   	   	else if (ReadNmb==12) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSystemCfgStart,MB_NumbSystemCfg,RT_WAITING_FOREVER);				// ������ ������������ �������
	   	   	else if (ReadNmb==13) 	eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usSGStart,MB_NumbSG,RT_WAITING_FOREVER);							// ������ ������ �������

	   	   	else if (ReadNmb==14) 	ReadNmb = 19;	// ������� � 20

	   	  	else if (ReadNmb==20) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStartSW,MB_NumbConfigSW,RT_WAITING_FOREVER);}				// ������ ������������ �����������
	   	   	else if (ReadNmb==21) 	ReadNmb = 26;	// ������� � 27

// ������ ��������� � ������� �������
//	   	  	else if (ReadNmb==21) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAPWStart,MB_NumbConfigAPW,RT_WAITING_FOREVER);}			// ������ ������������ ���
//	   	  	else if (ReadNmb==22) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAWRStart,MB_NumbConfigAWR,RT_WAITING_FOREVER);}			// ������ ������������ ���+���
//	   	  	else if (ReadNmb==23) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRPWRStart,MB_NumbConfigTRPWR,RT_WAITING_FOREVER);}		// ������ ���� ������� �������� ������
//	   	  	else if (ReadNmb==24) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigTRMeasStart,MB_NumbConfigTRMeas,RT_WAITING_FOREVER);}		// ������ ���� ������� �������������� ������
//	   	  	else if (ReadNmb==25) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSIn,MB_NumbConfigVLSIn,RT_WAITING_FOREVER);}		// ������ ������������ ������� ���������� ��������
/*	   	  	else if (ReadNmb==26) {																													// ������ ������������ �������� ���������� ��������

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

						NumbBlokReadMBLim = (uint16_t)MB_NumbConfigVLSOut;
						NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

						addrConfig = MB_StartConfigVLSOut + NumbBlokReadMB * MaxSizeBlok;

						if (NumbBlokReadMB == NumbBlokReadMBLim){
							SizeBlokReadMB = (uint16_t)MB_NumbConfigVLSOut;
							SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
						}


						if (NumbBlokReadMB > NumbBlokReadMBLim){
							addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
						} else
						if((SizeBlokReadMB) && addrConfig<(MB_StartConfigVLSOut+MB_NumbConfigVLSOut)){
						   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
						   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
						}

	   	  			}
*/
	   	  	else if (ReadNmb==27) {																													// ������ �������� ��� ��������� ������ �������

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbUstavki;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = usConfigUstavkiStart + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbUstavki;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}

	   	  				if (NumbBlokReadMB > NumbBlokReadMBLim){
	   	  					addrConfig = 0;
							NumbBlokReadMB = 0;
							NumbBlokReadMBLim = 0;
							SizeBlokReadMB = MaxSizeBlok;
							ReadNmb++;
	   	  				} else
					    if((SizeBlokReadMB) && addrConfig<(usConfigUstavkiStart+MB_NumbUstavki)) {
							   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
							   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
						  }


	   	  			}
	   	  	else if (ReadNmb==28) {																													// ������ ���������� ����������.

	   	  				if (NumbBlokReadMBLim == 0) NumbBlokReadMB = 0;

	  					NumbBlokReadMBLim = (uint16_t)MB_NumbAutomat;
	  					NumbBlokReadMBLim = NumbBlokReadMBLim/(uint16_t)MaxSizeBlok;

	   	  				addrConfig = usConfigAutomatStart + NumbBlokReadMB * MaxSizeBlok;

	   	  				if (NumbBlokReadMB == NumbBlokReadMBLim){
	   	  					SizeBlokReadMB = (uint16_t)MB_NumbAutomat;
	   	  					SizeBlokReadMB = SizeBlokReadMB % (uint16_t)MaxSizeBlok;
	   	  				}else{
	   	  				}

	   	  			   if((SizeBlokReadMB) && addrConfig<(usConfigAutomatStart+MB_NumbAutomat)) {
	   	  				   	   USART_TRACE_BLUE("%u-%u:������:0x%X\n",ReadNmb,NumbBlokReadMB,(unsigned int)addrConfig);
	   	  					   eMBMasterReqReadHoldingRegister(MB_Slaveaddr,addrConfig,SizeBlokReadMB,RT_WAITING_FOREVER);
	   	  			   	  }
	   	  				else {
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}

	   	  				if((SizeBlokReadMB != MaxSizeBlok) && (NumbBlokReadMB > NumbBlokReadMBLim))	{																																			// ���������
	   	  					NumbBlokReadMB = 0;
	   	  					SizeBlokReadMB = MaxSizeBlok;
	   	  					ReadNmb++;
	   	  				}
	   	  			}

//	   	  	else if (ReadNmb==26) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfigVLSOut,MB_NumbConfigVLSOut,RT_WAITING_FOREVER);}		// ������ ������������ �������� ���������� ��������
//	   	  	else if (ReadNmb==27) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigUstavkiStart,MB_NumbUstavki,RT_WAITING_FOREVER);}			// ������ �������� ��� ��������� ������ �������
	   	  	else if (ReadNmb==28) { eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usConfigAutomatStart,MB_NumbAutomat,RT_WAITING_FOREVER);}			// ������ ���������� ����������
	   	  	else {ReadNmb++;}

	   	   	LimitRead = 29;
	   	   	if (ReadNmb >= LimitRead) { ReadNmb = 1; osMutexRelease(xIEC850StartMutex);}

	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;			// ���� ���� ������ � ������� ������ �� ���� ��������� �������. ����� ��� ����������.
	   	   		writeNmb = 0;
	   	   	}
/*******************************************************
 * ��������� ������������ ������� ���������
 *******************************************************/
			if(IEC850TaskHandle && (iedServer != NULL)){
				IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

// PROT_LLN0 Health --------
				{
					uint32_t	LLN0_Health = STVALINT32_OK;
					if ((ucMDiscInBuf[MB_offsetHardFaults] & MB_bOffsetModule4) || (ucMDiscInBuf[MB_offsetHardFaults] & MB_bOffsetModule5)) {LLN0_Health = STVALINT32_Warning;}
					IedServer_updateInt32AttributeValue(iedServer, &iedModel_PROT_LLN0_Health_stVal, LLN0_Health);
					IedServer_updateUTCTimeAttributeValue(iedServer,&iedModel_PROT_LLN0_Health_t, currentTime);
				}
// PROT_IPTOC1..8 ------						������ �� ��������� ����
					Set_IPTOC(6,currentTime);
// Set_I2I1PTOC ------
//					Set_I2I1PTOC(1,currentTime);
// Set_IPTUC ------
//					Set_IPTUC(1,currentTime);
// PROT_I20PTOC1..6 ------						������ �� ��������� �� ����� � ��������� ���� �������� ������������������
//					Set_I20PTOC(6,currentTime);
// PROT_UPTOV1..4 ------						������ �� ��������� ����������
//					Set_UPTOV(4,currentTime);
// PROT_UPTUV1..4 ------						������ �� ��������� ����������
//					Set_UPTUV(4,currentTime);
// PROT_UPTOF1..4 ------						������ �� ��������� �������
//					Set_PTOF(4,currentTime);
// PROT_UPTUF1..4 ------						������ �� ��������� �������
//					Set_PTUF(4,currentTime);
// PROT_VZGGIO1..16 ------						������� ������
//					Set_VZGGIO(16,currentTime);
// PROT_RREC ------
//					Set_RREC(1,currentTime);
// PROT_RREC ------
//					Set_RBRF(1,currentTime);

// CTRL_CSWI ------
//					Set_CSWI(1,currentTime);
// CTRL_PTRC ------
//					Set_PTRC(1,currentTime);
// CTRL_XCBR ------
//					Set_XCBR(1,currentTime);
// CTRL_GGIO ------
//					Set_XCBRGGIO(1,currentTime);
// MES_MMXU ------
//					Set_MMXU(1,currentTime);

// GGIO_IN24GGIO ------
//					Set_INGGIO(24,currentTime);
//					Set_OUTGGIO(18,currentTime);
//					Set_LEDGGIO(12,currentTime);
//					Set_SSLGGIO(32,currentTime);
//					Set_VLSGGIO(16,currentTime);
//					Set_LSGGIO(16,currentTime);
// LD0 SG
//					Set_SG(0,currentTime);

				IedServer_unlockDataModel(iedServer);
			}
/*******************************************************
 *
 *******************************************************/
		}
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.

			GLOBAL_QUALITY =  QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_OLD_DATA;
    		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR || errorType == MB_ESENT){
			if (writeNmb == 10) {writeNmb = 0;}
//	   		USART_TRACE_RED("MB_ENOERR: %u\n",ReadNmb);
		}else
		if (errorType == MB_ERECVDATA){
			GLOBAL_QUALITY = QUALITY_VALIDITY_GOOD;
			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
			Nextread = true;
		}


		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR5_700 MODBUS
 *******************************************************/
#if defined (MR5_700)
void StartMODBUSTask(void const * argument)
{
	extern uint64_t nextSynchTime;
	extern bool resynch;
	uint32_t			TimerReadMB;
	uint8_t				ReadNmb;
	eMBErrorCode		errorType;
	uint8_t				i;
	RTC_TimeTypeDef 	Time;
	uint8_t  			PerForSynch;
	extern const uint8_t userConfig[];

//	osMutexWait(xIEC850StartMutex, osWaitForever);				// ������� ������� ��� ���������� TCP ���� �� ������� ��������� IP
//	USART_TRACE_BLUE("MODBUS ������ ������� IEC850Start\n");

//	usMDiscInStart = MB_StartDiscreetaddr;

	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

//	vTaskDelay(1000);
	writeNmb = 0;
	ReadNmb = 0;

	Port_On(LED1);
/*
	{
		uint8_t 			HardFaultcnt;												// ������� ������������
		memory_read((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));						// ������ IP �� ������� ������
		HardFaultcnt++;
		memory_write_to_mem((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));
		USART_TRACE_GREEN("����� ��������: %u\n",HardFaultcnt);
	}
*/
	TimerReadMB = HAL_GetTick();
	for(;;)
	{
 //		IedServer_setControlHandler(iedServer, &iedModel_GGIO_OUTGGIO1_SPCSO1, (ControlHandler)controlListener, &iedModel_GGIO_OUTGGIO1_SPCSO1);

		// ����������� ��� ��������� ����� �� ����� ��� �����, � ����� ���������
	 	if (writeNmb == 10) {
		   	   		USART_TRACE_RED("��������� ����� � �������.\n");
		   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
		   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (UCHAR *)&ucMDateBuf,RT_WAITING_FOREVER);
		   	   	}
	 	else
		if (((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS)||(Nextread)){		// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			Nextread = false;
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>14) {ReadNmb = 1;}
			if (ReadNmb>2 && ReadNmb<10) ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_SwON,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_SwON,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� 0.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe0,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 8) {
	   	   		USART_TRACE_RED("������������ ������ ������� 1.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe1,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
/*
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_RED("��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (UCHAR *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
*/
	   		uint64_t currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ -----------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ������ ���� �� �������� NTP -----
	   	     if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = 15;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	   	     }
			// ������������� ������
	   	   	if (ReadNmb==0)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);		// ������ ������� ����������
	   		if (ReadNmb==1)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);
	   		if (ReadNmb==2)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);

	   		// ����������� ������
	   	   	if (ReadNmb==10) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);				// ������ �������� �������
	   	   	if (ReadNmb==11) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStart,MB_NumbConfig,RT_WAITING_FOREVER);			// ������ �������(������ IP �����)
	   	   	if (ReadNmb==12) {eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktt,1,RT_WAITING_FOREVER);}		// ������ �������
	   	   	if (ReadNmb==13) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktn,1,RT_WAITING_FOREVER);		// ������ �������
	   	   	if (ReadNmb==14) {
	   	   		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig,MB_NumbConfigall,RT_WAITING_FOREVER);		// ������ ���� �������
	   	   		osMutexRelease(xIEC850StartMutex);
	   	   	}
	   	   	ReadNmb++;	// ������ ����, ����� �������������� ���������� ��������
	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;
	   	   		writeNmb = 0;
	   	   	}
//+++++++++++++++++++
/*
			errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
			if (errorType == MB_ETIMEDOUT){
	    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
	    		//if (ReadNmb) ReadNmb--;
	    	Port_Off(LED1);
			}else
			if (errorType == MB_ENOERR){
				if (writeNmb == 0) ReadNmb++;
			}
*/
//+++++++++++++++++++
if(IEC850TaskHandle && (iedServer != NULL)){

            IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD1){
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else
{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<0));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<1));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<2));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<3));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<4));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<5));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<6));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<7));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_t, currentTime);

if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD2){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<8));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<9));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<10));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<11));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<12));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<13));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<14));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<15));

}
   	   	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_t, currentTime);
      	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_t, currentTime);

if (ucMDiscInBuf[MB_offsetError] & MB_errorMRV1){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
}else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,0);

          	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<0));
       	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<1));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<2));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<3));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<4));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<5));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<6));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<7));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_t, currentTime);
     	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_t, currentTime);

//PTOC ------------------------

    IedServer_updateInt32AttributeValue(iedServer,&iedModel_PROT_IPTOC1_Health_stVal,(ucMDiscInBuf[MB_offsetError] & (MB_errorMSAI | MB_errorMSAU)));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Health_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_IPTOC1_Health_q,0);


	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Str_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<0));

//	int16_t		Valtmp = GetDirGeneral((ucMDiscInBuf[MB_offsetPTOC] >> 2) & 0b111111);
	int16_t		Valtmp = ucMDiscInBuf[MB_offsetDirGeneral] & 0b11;
	MmsValue* ValMMS = MmsValue_newIntegerFromInt16(Valtmp);

	IedServer_updateAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Str_dirGeneral, ValMMS);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Str_t, currentTime);

	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		IedServer_updateQuality(iedServer,&iedModel_PROT_IPTOC1_Str_q, QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	} else{
		IedServer_updateQuality(iedServer,&iedModel_PROT_IPTOC1_Str_q,0);
	}
	MmsValue_delete(ValMMS);

	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Op_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<1));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Op_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_IPTOC1_Op_q,0);

//!PTOC ------------------------

	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAU){
		// ����������
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_res_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_Hz_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

	}
	else{
		// ����������
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsC_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_res_q,0);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsC_q,0);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_Hz_q,0);
// phV
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsA_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ua],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsB_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ub],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsC_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uc],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_res_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Un],ucMConfigBufall[MB_offset_Ktnnp]));

// PPV
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsA_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uab],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsB_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ubc],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsC_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uca],Ktn));

		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_Hz_mag_f,(float)ucMAnalogInBuf[MB_offset_Hz]/256);

	}
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsC_t, currentTime);

	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsC_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Hz_t, currentTime);

// ���������� ������� ������
	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	}
	else{
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,0);
		float CurrA;
		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ia] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsA_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ib] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsB_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ic] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsC_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_In] * 5)/65536 * ucMConfigBufall[MB_offset_Kttnp];
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_res_cVal_mag_f,CurrA);

	}
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsC_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_res_t, currentTime);

    //�������� ----------------------------------------------------
	if ((ucMDiscInBuf[MB_offsetError] & MB_errorMSAU) || (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI)){

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totW_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totVAr_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totPF_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	}else{
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totW_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totVAr_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totPF_q,0);

		float CurrW;
		CurrW = (float)(ucMAnalogInBuf[MB_offset_TotW]);
		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotWZ) {CurrW = -CurrW;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totW_mag_f,CurrW);

		CurrW = (float)(ucMAnalogInBuf[MB_offset_TotVAr]);
		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrW = -CurrW;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totVAr_mag_f,CurrW);

		CurrW = (float)ucMAnalogInBuf[MB_offset_TotPF]/256;
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totPF_mag_f,CurrW);

	}
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totW_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totVAr_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totPF_t, currentTime);

// !!���������� ������� ������


//CSWI
//-------------------------------

	{
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_t, currentTime);


	uint32_t	res32 = STVALINT32_OK;
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorHard) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorLogic) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorData) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorSW) 		{res32 |= STVALINT32_Warning;}

	IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_stVal, res32);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Health_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_t, currentTime);

	if (ucMConfigBufall[MB_offset_ControlBits] & MB_bOffset_ControlSDTU){
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Mod_stVal, 1);
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Beh_stVal, 1);
	}else {
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Mod_stVal, 5);
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Beh_stVal, 5);
	}
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Mod_q,0);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Beh_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Mod_t, currentTime);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Beh_t, currentTime);

	}
//XCBR
//--------------------------------
	{
	//Pos
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_t, currentTime);


	uint32_t	res32 = STVALINT32_OK;
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorHard) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorLogic) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorData) 	{res32 |= STVALINT32_Warning;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorSW) 		{res32 |= STVALINT32_Warning;}

	IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_XCBR1_Health_stVal, res32);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Health_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Health_t, currentTime);

	if (ucMConfigBufall[MB_offset_ControlBits] & MB_bOffset_ControlSDTU){
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_XCBR1_Mod_stVal, 1);
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_XCBR1_Beh_stVal, 1);
	}else {
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_XCBR1_Mod_stVal, 5);
		IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_XCBR1_Beh_stVal, 5);
	}
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Mod_q,0);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Beh_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Mod_t, currentTime);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Beh_t, currentTime);


	//Loc
	bool XCBR1_Loc=0;
	if (ucMConfigBufall[MB_offset_BlockSDTU]) {XCBR1_Loc = 1;}
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_stVal, XCBR1_Loc);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Loc_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_t, currentTime);
	}

//	IedServer_updateInt32AttributeValue(iedServer, iedModel_CTRL_XCBR1_OpCnt_stVal,(ucMConfigBufall[MB_offset_BlockSDTU]);

	//--------------------------------

//LEDGGIO1
//--------------------------------
 	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<8));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<9));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<10));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<11));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<12));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<13));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<14));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<15));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<4));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<5));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<6));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<7));

	// ������ ������ �����������
	if (ucMDiscInBuf[MB_offsetLED] & 0xFF00){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	}


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_t, currentTime);


    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind1_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind2_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind3_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind4_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind5_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind6_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind7_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind8_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind9_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind10_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind11_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind12_q,0);

//--------------------------------
// ����� ������

	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetError){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetErrorNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);
	}

//--------------------------------
// MSQI1
	int32_t MSQI_stval = 0;

	if (ucMDiscInBuf[MB_offsetError] & (MB_errorMRV1|MB_errorMSAI)) {
		MSQI_stval = 2;
	} else		MSQI_stval = 1;

	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_MSQI1_Health_stVal,  MSQI_stval);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_Health_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_Health_t, currentTime);

// SeqA
    float	cXtmp = (float)ucMAnalogInBuf[MB_offset_NI1] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c1_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI2] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c2_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI0] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c3_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_t, currentTime);

// SeqU
    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU1],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c1_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c1_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c1_t, currentTime);

    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU2],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c2_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c2_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c2_t, currentTime);

    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU0],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c3_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c3_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c3_t, currentTime);

//--------------------------------

//--------------------------------
// RFLO
	uint32_t	OMP;
	if (ucMConfigBufall[MB_offset_OMP] & 1){OMP=1;}	else{OMP=5;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Mod_stVal,  OMP);
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Beh_stVal,  OMP);

    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Mod_q,0);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Beh_q,0);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Mod_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Beh_t, currentTime);

	uint32_t	err;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetError){err=1;}	else{err=5;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Health_stVal,  err);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Health_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Health_t, currentTime);


    float	fltz = (float)ucMAnalogInBuf[MB_offset_OMPLkz] * (float)ucMConfigBufall[MB_offset_OMP+1] / (float)256000;
	IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_RFLO1_Fltz_mag_f,  (float)fltz);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Fltz_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Fltz_t, currentTime);

    fltz = (float)ucMAnalogInBuf[MB_offset_OMPLkz]/(float)256;
	IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_RFLO1_FltDiskm_mag_f, fltz);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_FltDiskm_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_FltDiskm_t, currentTime);


//--------------------------------
    // ��������� ��������
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Mod_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_Proxy_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_PhyHealth_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Mod_t, currentTime);
//LEDGGIO1
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Mod_t, currentTime);


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Mod_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Mod_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Str_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_IPTOC1_Op_t, currentTime);


    // ��������� ������ �������
    uint8_t	currSG = 1;
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSettingGr) currSG = 2;

	if (currSG != IedServer_getActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0))) {
		IedServer_changeActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0),currSG );
		USART_TRACE("���������� ������ �������. %u\n",iedModel_LD0_LLN0_sgcb0.actSG);
	}

	// -------------------- ���� �� ��������� � �������� ---------------------------
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote) {
    	// ���� ��������� �������. ����� �� ����������.
    	USART_TRACE("���������� �������.\n");
    	writeNmb = 9;
    	ucMDiscInBuf[MB_offset_adr0] ^= MB_bOffsetSysNote;
    }


    IedServer_unlockDataModel(iedServer);																// ����� ���������� mmsServer'��
}
//+++++++++++++++++++
		}
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
    		ReadNmb++;
			GLOBAL_QUALITY =  QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_OLD_DATA;
    		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR || errorType == MB_ESENT){
			if (writeNmb == 10) {writeNmb = 0;}
//	   		USART_TRACE_RED("MB_ENOERR: %u\n",ReadNmb);
		}else
		if (errorType == MB_ERECVDATA){
			GLOBAL_QUALITY = QUALITY_VALIDITY_GOOD;
			vTaskDelay(5);									// ������ �����, �� �������� ������� ������.
			Nextread = true;
		}
		/*
		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
    		//if (ReadNmb) ReadNmb--;
    	Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR){
//			if (writeNmb == 0) ReadNmb++;
			if (writeNmb == 10) {writeNmb = 0;}
		}
*/
		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR5_600 MODBUS
 *******************************************************/
#if defined (MR5_600)
void StartMODBUSTask(void const * argument)
{
	extern uint64_t nextSynchTime;
	extern bool resynch;
	uint32_t			TimerReadMB;
	uint8_t				ReadNmb=0;
	eMBErrorCode		errorType;
	uint8_t				i;
	RTC_TimeTypeDef 	Time;
	uint8_t  			PerForSynch;
	extern const uint8_t userConfig[];

//	osMutexWait(xIEC850StartMutex, osWaitForever);				// ������� ������� ��� ���������� TCP ���� �� ������� ��������� IP
//	USART_TRACE_BLUE("MODBUS ������ ������� IEC850Start\n");

//	usMDiscInStart = MB_StartDiscreetaddr;

	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();

//	ReadNmb = userConfig[4];			// ������ �� flash.

	Port_On(LED1);
	/*
	{
			uint8_t 			HardFaultcnt;												// ������� ������������
			memory_read((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));						// ������ IP �� ������� ������
			HardFaultcnt++;
			memory_write_to_mem((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));
			USART_TRACE_GREEN("����� ��������: %u\n",HardFaultcnt);
		}
*/
	TimerReadMB = HAL_GetTick();
	for(;;)
	{
 //		IedServer_setControlHandler(iedServer, &iedModel_GGIO_OUTGGIO1_SPCSO1, (ControlHandler)controlListener, &iedModel_GGIO_OUTGGIO1_SPCSO1);

   	   	if (writeNmb == 10) {
   	   		USART_TRACE_RED("��������� ����� � �������.\n");
   	   	Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (uint16_t *)&ucMDateBuf,RT_WAITING_FOREVER);
   	   	}
   	   	else
		if ((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS){					// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>13) {ReadNmb = 1;}
			if (ReadNmb>2 && ReadNmb<10) ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_SwON,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_SwON,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� 0.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe0,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 8) {
	   	   		USART_TRACE_RED("������������ ������ ������� 1.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe1,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   	eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_RED("��������� ����� � �������.\n");
	   	   	Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (uint16_t *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		uint64_t currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ ------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ------------------------------------
	   	   	// �������������������� ����� ������ � �� �� �������
	  	    if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) 					//���� ������ ��� �� ������ ������������� �����
				{
					ReadNmb = 15;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
	  	     }
			// ������������� ������
	   	   	if (ReadNmb==0)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);		// ������ ������� ����������
	   		if (ReadNmb==1)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);
	   		if (ReadNmb==2)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);

	   		// ����������� ������
	   	   	if (ReadNmb==10) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);				// ������ �������� �������
	   	   	if (ReadNmb==11) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStart,MB_NumbConfig,RT_WAITING_FOREVER);			// ������ �������(������ IP �����)
//	   	   	if (ReadNmb==12) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktt,1,RT_WAITING_FOREVER);		// ������ �������
	   	   	if (ReadNmb==12) {eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktn,1,RT_WAITING_FOREVER);}		// ������ �������
	   	   	if (ReadNmb==13) {
	   	   		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig,MB_NumbConfigall,RT_WAITING_FOREVER);					// ������ ���� �������
	   	   		osMutexRelease(xIEC850StartMutex);	// �������� ������ 61850
	   	   	}
	   	   	ReadNmb++;
	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;
	   	   		writeNmb = 0;
	   	   	}
//+++++++++++++++++++
/*
	   	 		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
	   	 		if (errorType == MB_ETIMEDOUT){
	   	    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
	   	     		//if (ReadNmb) ReadNmb--;
	   	     	Port_Off(LED1);
	   	 		}
*/
//+++++++++++++++++++
if(IEC850TaskHandle && (iedServer != NULL)){

            IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD1){
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else
{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet1);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet2);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet3);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet4);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet5);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet6);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet7);
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & MB_boffsetDiscreet8);
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_t, currentTime);

/*
if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD2){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<8));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<9));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<10));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<11));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<12));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<13));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<14));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<15));

}
   	   	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_t, currentTime);
      	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_t, currentTime);
*/
if (ucMDiscInBuf[MB_offsetError] & MB_errorMRV1){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
}else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,0);

          	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<0));
       	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<1));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<2));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<3));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<4));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<5));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<6));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<7));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_t, currentTime);
     	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_t, currentTime);

if (ucMDiscInBuf[MB_offsetError] & MB_errorMRV2){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO9_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO10_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO11_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO12_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO13_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO14_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO15_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO16_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
}else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO9_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO10_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO11_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO12_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO13_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO14_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO15_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO16_q,0);

			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO9_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<8));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO10_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<9));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO11_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<10));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO12_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<11));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO13_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<12));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO14_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<13));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO15_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<14));
			IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO16_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<15));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO9_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO10_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO11_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO12_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO13_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO14_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO15_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO16_t, currentTime);

//PTOC
/*
    IedServer_updateInt32AttributeValue(iedServer,&iedModel_PROT_PTOC1_Health_stVal,(ucMDiscInBuf[MB_offsetError] & (MB_errorMSAI | MB_errorMSAU)));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Health_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Health_q,0);


	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<0));

	int16_t		Valtmp = ucMDiscInBuf[MB_offsetDirGeneral] & 0b11;
	MmsValue* ValMMS = MmsValue_newIntegerFromInt16(Valtmp);

	IedServer_updateAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_dirGeneral, ValMMS);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_t, currentTime);

	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Str_q, QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	} else{
		IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Str_q,0);
	}
	MmsValue_delete(ValMMS);

	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_PTOC1_Op_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<1));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Op_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Op_q,0);
*/

	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAU){
		// ����������
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_res_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_Hz_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);

	}
	else{
		// ����������
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_phsC_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_phV_res_q,0);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_PPV_phsC_q,0);

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_Hz_q,0);
// phV
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsA_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ua],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsB_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ub],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_phsC_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uc],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_phV_res_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Un],ucMConfigBufall[MB_offset_Ktnnp]));

// PPV
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsA_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uab],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsB_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Ubc],Ktn));
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_PPV_phsC_cVal_mag_f,GetRealU(ucMAnalogInBuf[MB_offset_Uca],Ktn));

		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_Hz_mag_f,(float)ucMAnalogInBuf[MB_offset_Hz]/256);

	}
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_phV_phsC_t, currentTime);

	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_PPV_phsC_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Hz_t, currentTime);
/*
// ���������� ������� ������
	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	}
	else{
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,0);
		float CurrA;
		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ia] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsA_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ib] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsB_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ic] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsC_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_In] * 5)/65536 * ucMConfigBufall[MB_offset_Kttnp];
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_res_cVal_mag_f,CurrA);

	}
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsC_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_res_t, currentTime);

    //�������� ----------------------------------------------------
	if ((ucMDiscInBuf[MB_offsetError] & MB_errorMSAU) || (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI)){

		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totW_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totVAr_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totPF_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	}else{
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totW_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totVAr_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_totPF_q,0);

		float CurrW;
		CurrW = (float)(ucMAnalogInBuf[MB_offset_TotW]);
		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotWZ) {CurrW = -CurrW;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totW_mag_f,CurrW);

		CurrW = (float)(ucMAnalogInBuf[MB_offset_TotVAr]);
		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrW = -CurrW;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totVAr_mag_f,CurrW);

		CurrW = (float)ucMAnalogInBuf[MB_offset_TotPF]/256;
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_totPF_mag_f,CurrW);

	}
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totW_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totVAr_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_totPF_t, currentTime);
*/
// !!���������� ������� ������

/*
//CSWI
//-------------------------------

	{
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_t, currentTime);

#define MB_bOffsetErrorHard		1<<0
#define MB_bOffsetErrorLogic	1<<1
#define MB_bOffsetErrorData		1<<2

	uint32_t	res32 = STVALINT32_OK;
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorHard) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorLogic) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorData) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorSW) 		{res32 |= STVALINT32_Alarm;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_stVal, res32);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Health_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_t, currentTime);

	}
//XCBR
//--------------------------------
	{
	//Pos
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_t, currentTime);

	//Loc
	bool XCBR1_Loc=0;
	if (ucMConfigBufall[MB_offset_BlockSDTU]) {XCBR1_Loc = 1;}
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_stVal, XCBR1_Loc);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Loc_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_t, currentTime);
	}

*/
	//--------------------------------

//LEDGGIO1
//--------------------------------
 	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<8));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<9));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<10));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<11));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<12));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<13));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<14));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<15));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<5));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<4));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<6));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<7));

	// ������ ������ �����������
	if (ucMDiscInBuf[MB_offsetLED] & 0xFF00){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	}


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_t, currentTime);


    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind1_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind2_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind3_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind4_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind5_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind6_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind7_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind8_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind9_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind10_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind11_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind12_q,0);

//--------------------------------
// ����� ������

	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetError){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetErrorNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);
	}

//--------------------------------
// MSQI1
	int32_t MSQI_stval = 0;

	if (ucMDiscInBuf[MB_offsetError] & (MB_errorMRV1)) {
		MSQI_stval = 2;
	} else		MSQI_stval = 1;

	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_MSQI1_Health_stVal,  MSQI_stval);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_Health_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_Health_t, currentTime);
   float  cXtmp;
/*
// SeqA
    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI1] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c1_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI2] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c2_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI0] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c3_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_t, currentTime);
*/
// SeqU
    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU1],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c1_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c1_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c1_t, currentTime);

    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU2],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c2_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c2_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c2_t, currentTime);

    cXtmp = (float)GetRealU(ucMAnalogInBuf[MB_offset_NU0],Ktn);
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c3_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqU_c3_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqU_c3_t, currentTime);

//--------------------------------

//--------------------------------
// RFLO
/*
	uint32_t	OMP;
	if (ucMConfigBufall[MB_offset_OMP] & 1){OMP=1;}	else{OMP=5;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Mod_stVal,  OMP);
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Beh_stVal,  OMP);

    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Mod_q,0);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Beh_q,0);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Mod_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Beh_t, currentTime);

	uint32_t	err;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetError){err=1;}	else{err=5;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_RFLO1_Health_stVal,  err);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Health_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Health_t, currentTime);


    float	fltz = (float)ucMAnalogInBuf[MB_offset_OMPLkz] * (float)ucMConfigBufall[MB_offset_OMP+1] / (float)256000;
	IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_RFLO1_Fltz_mag_f,  (float)fltz);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_Fltz_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_Fltz_t, currentTime);

    fltz = (float)ucMAnalogInBuf[MB_offset_OMPLkz]/(float)256;
	IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_RFLO1_FltDiskm_mag_f, fltz);
    IedServer_updateQuality(iedServer,&iedModel_MES_RFLO1_FltDiskm_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_RFLO1_FltDiskm_t, currentTime);
*/

//--------------------------------
    // ��������� ��������
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Mod_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_Proxy_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_PhyHealth_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Mod_t, currentTime);
//LEDGGIO1
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Mod_t, currentTime);


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Mod_t, currentTime);


    // ��������� ������ �������
    uint8_t	currSG = 1;
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSettingGr) currSG = 2;

	if (currSG != IedServer_getActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0))) {
		IedServer_changeActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0),currSG );
		USART_TRACE("���������� ������ �������. %u\n",iedModel_LD0_LLN0_sgcb0.actSG);
	}

	// -------------------- ���� �� ��������� � �������� ---------------------------
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote) {
    	// ���� ��������� �������. ����� �� ����������.
    	USART_TRACE("���������� �������.\n");
    	writeNmb = 9;
    	ucMDiscInBuf[MB_offset_adr0] ^= MB_bOffsetSysNote;
    }

    IedServer_unlockDataModel(iedServer);																// ����� ���������� mmsServer'��
}
//+++++++++++++++++++
		}

//		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
//		if (errorType == MB_ETIMEDOUT){
//    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
//    	Port_Off(LED1);
//		}

		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
			USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
			//if (ReadNmb) ReadNmb--;
		Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR){
	//			if (writeNmb == 0) ReadNmb++;
			if (writeNmb == 10) {writeNmb = 0;}
		}
		taskYIELD();										// �������� ������.
	}
}
#endif
/*******************************************************
 * MR5_500 MODBUS
 *******************************************************/
#if defined (MR5_500)
void StartMODBUSTask(void const * argument)
{
	extern uint64_t nextSynchTime;
	extern bool resynch;
	uint32_t			TimerReadMB;
	uint8_t				ReadNmb=0;
	eMBErrorCode		errorType;
	uint8_t				i;
	RTC_TimeTypeDef 	Time;
	uint8_t  			PerForSynch;
	extern const uint8_t userConfig[];

//	osMutexWait(xIEC850StartMutex, osWaitForever);				// ������� ������� ��� ���������� TCP ���� �� ������� ��������� IP
//	USART_TRACE_BLUE("MODBUS ������ ������� IEC850Start\n");

//	usMDiscInStart = MB_StartDiscreetaddr;

	eMBMasterInit(MB_RTU, 4, MB_Speed,  MB_PAR_NONE);
	eMBMasterEnable();


	Port_On(LED1);
/*
	{
		uint8_t 			HardFaultcnt;												// ������� ������������
		memory_read((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));						// ������ IP �� ������� ������
		HardFaultcnt++;
		memory_write_to_mem((uint8_t *)&HardFaultcnt,_IfHardFault,sizeof(HardFaultcnt));
		USART_TRACE_GREEN("����� ��������: %u\n",HardFaultcnt);
	}
*/
	TimerReadMB = HAL_GetTick();
	for(;;)
	{
 //		IedServer_setControlHandler(iedServer, &iedModel_GGIO_OUTGGIO1_SPCSO1, (ControlHandler)controlListener, &iedModel_GGIO_OUTGGIO1_SPCSO1);
		if (writeNmb == 10) {
		   	   		USART_TRACE_RED("��������� ����� � �������.\n");
		   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
		   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (UCHAR *)&ucMDateBuf,RT_WAITING_FOREVER);
		   	   	}
		else
		if ((HAL_GetTick()-TimerReadMB)>MB_PerForReadMODBUS){					// ������������� ����� MODBUS � �������� MB_PerForReadMODBUS(��)
			TimerReadMB = HAL_GetTick();

			if (ReadNmb>14) {ReadNmb = 1;}
			if (ReadNmb>2 && ReadNmb<10) ReadNmb = 1;

			//--------------- ������������� ����� ------------------------------------
			if ((ReadNmb == 1) && HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xFFFF)	ReadNmb = 10;			// ������ ���

			//--------------- ����������� ------------------------------------
	   	   	if (writeNmb == 1) {
	   	   		USART_TRACE_GREEN("����������� ON\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwON,MB_SwON,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 2) {
	   	   		USART_TRACE_RED("����������� OFF\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SwOFF,MB_SwON,RT_WAITING_FOREVER);
	   	   	}

	   	   	if (writeNmb == 3) {
	   	   		USART_TRACE_RED("����� ���������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_LEDS_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 4) {
	   	   		USART_TRACE_RED("����� ����� �������������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_Error_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 5) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������� \n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 6) {
	   	   		USART_TRACE_RED("����� ����� ����� ������ � ������� ������\n");
	   	   		eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_ErrorNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 7) {
	   	   		USART_TRACE_RED("������������ ������ ������� 0.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe0,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 8) {
	   	   		USART_TRACE_RED("������������ ������ ������� 1.\n");
	   	   		eMBMasterReqWriteHoldingRegister(MB_Slaveaddr,MB_addr_SG,MB_selectGroupe1,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 9) {
	   	   		USART_TRACE_RED("����� ������ ������� �������.\n");
	   	   	eMBMasterReqWriteCoil(MB_Slaveaddr,MB_addr_SysNote_OFF,MB_CTRL_OFF,RT_WAITING_FOREVER);
	   	   	}
	   	   	if (writeNmb == 10) {
	   	   		USART_TRACE_RED("��������� ����� � �������.\n");
	   	   		Hal_setTimeToMB_Date((uint16_t *)&ucMDateBuf);
	   	   		eMBMasterReqWriteMultipleHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate*2, (UCHAR *)&ucMDateBuf,RT_WAITING_FOREVER);
	   	   	}
	   		uint64_t currentTime = Hal_getTimeInMs();

			//--------------- ���� ������ ------------------------------------
	   	   	if (writeNmb == 0){	// ������ ��� ������� ��������.

			//--------------- ������������� ����� ------------------------------------
		    if(SNTP_Period == 0){
				if ((currentTime > nextSynchTime) && resynch) {					//���� ������ ��� �� ������ ������������� �����
					ReadNmb = 15;
					eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);
					USART_TRACE_BLUE("����������������� �����. �����:0x%X\n",(unsigned int)currentTime);
				}
		  	}
			// ������������� ������
	   	   	if (ReadNmb==0)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMRevStart,MB_NumbWordRev,RT_WAITING_FOREVER);		// ������ ������� ����������
	   		if (ReadNmb==1)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDiscInStart,MB_NumbDiscreet,RT_WAITING_FOREVER);
	   		if (ReadNmb==2)  eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMAnalogInStart,MB_NumbAnalog,RT_WAITING_FOREVER);

	   		// ����������� ������
	   	   	if (ReadNmb==10) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMDateStart,MB_NumbDate,RT_WAITING_FOREVER);				// ������ �������� �������
	   	   	if (ReadNmb==11) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,usMConfigStart,MB_NumbConfig,RT_WAITING_FOREVER);			// ������ �������(������ IP �����)
	   	   	if (ReadNmb==12) {eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktt,1,RT_WAITING_FOREVER);}		// ������ �������
	   	   	if (ReadNmb==13) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig+MB_offset_Ktn,1,RT_WAITING_FOREVER);		// ������ �������
	   	   	if (ReadNmb==14) {
	   	   		eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartConfig,MB_NumbConfigall,RT_WAITING_FOREVER);		// ������ ���� �������
	   	   		osMutexRelease(xIEC850StartMutex);
	   	   	}
	   	   	ReadNmb++;
	   	   	} else{
	   	   		if (writeNmb == 9 ) ReadNmb = 10;
	   	   		writeNmb = 0;
	   	   	}
//+++++++++++++++++++
/*
	   	 		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
	   	 		if (errorType == MB_ETIMEDOUT){
	   	    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
	   	     		//if (ReadNmb) ReadNmb--;
	   	     	Port_Off(LED1);
	   	 		}
*/
//+++++++++++++++++++
if(IEC850TaskHandle && (iedServer != NULL)){

            IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��

if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD1){
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
    	    IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else
{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind3_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind4_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind5_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind6_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind7_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind8_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<0));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<1));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<2));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<3));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<4));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<5));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<6));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<7));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind1_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind2_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind3_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind4_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind5_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind6_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind7_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind8_t, currentTime);

if (ucMDiscInBuf[MB_offsetError] & MB_errorMSD2){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
} else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind9_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind10_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind11_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind12_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind13_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind14_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind15_q,0 | 0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_OUTGGIO1_Ind16_q,0 | 0);

            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<8));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<9));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<10));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<11));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<12));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<13));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<14));
            IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_stVal,  ucMDiscInBuf[MB_offsetDiscreet] & (1<<15));

}
   	   	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind9_t, currentTime);
      	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind10_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind11_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind12_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind13_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind14_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind15_t, currentTime);
       	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Ind16_t, currentTime);

if (ucMDiscInBuf[MB_offsetError] & MB_errorMRV1){
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
}else{
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO1_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO2_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO3_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO4_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO5_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO6_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO7_q,0);
			IedServer_updateQuality(iedServer,&iedModel_GGIO_INGGIO1_SPCSO8_q,0);

          	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<0));
       	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<1));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<2));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<3));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<4));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<5));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<6));
      	    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_stVal,  ucMDiscInBuf[MB_offsetRelay] & (1<<7));
}
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO1_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO2_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO3_t, currentTime);
			IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO4_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO5_t, currentTime);
     	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO6_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO7_t, currentTime);
    	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_INGGIO1_SPCSO8_t, currentTime);

//PTOC

    IedServer_updateInt32AttributeValue(iedServer,&iedModel_PROT_PTOC1_Health_stVal,(ucMDiscInBuf[MB_offsetError] & (MB_errorMSAI | MB_errorMSAU)));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Health_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Health_q,0);


	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<0));

//	int16_t		Valtmp = GetDirGeneral((ucMDiscInBuf[MB_offsetPTOC] >> 2) & 0b111111);
	int16_t		Valtmp = ucMDiscInBuf[MB_offsetDirGeneral] & 0b11;
	MmsValue* ValMMS = MmsValue_newIntegerFromInt16(Valtmp);

	IedServer_updateAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_dirGeneral, ValMMS);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_t, currentTime);

	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Str_q, QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	} else{
		IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Str_q,0);
	}
	MmsValue_delete(ValMMS);

	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_PROT_PTOC1_Op_general,  ucMDiscInBuf[MB_offset_I_IO] & (1<<1));
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Op_t, currentTime);
	IedServer_updateQuality(iedServer,&iedModel_PROT_PTOC1_Op_q,0);

// ���������� ������� ������
	if (ucMDiscInBuf[MB_offsetError] & MB_errorMSAI){
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,QUALITY_VALIDITY_INVALID | QUALITY_DETAIL_FAILURE);
	}
	else{
		// ����
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsA_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsB_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_phsC_q,0);
		IedServer_updateQuality(iedServer,&iedModel_MES_MMXU1_A_res_q,0);
		float CurrA;
		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ia] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsA_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ib] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsB_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_Ic] * 40)/65536 * Ktt;
//		if (ucMDiscInBuf[MB_offsetPTOC] & MB_bTotVAZ) {CurrA = -CurrA;}
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_phsC_cVal_mag_f,CurrA);

		CurrA = (float)(ucMAnalogInBuf[MB_offset_In] * 5)/65536 * ucMConfigBufall[MB_offset_Kttnp];
		IedServer_updateFloatAttributeValue(iedServer,&iedModel_MES_MMXU1_A_res_cVal_mag_f,CurrA);

	}
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsA_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsB_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_phsC_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_A_res_t, currentTime);

// !!���������� ������� ������


//CSWI
//-------------------------------

	{
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_t, currentTime);

#define MB_bOffsetErrorHard		1<<0
#define MB_bOffsetErrorLogic	1<<1
#define MB_bOffsetErrorData		1<<2

	uint32_t	res32 = STVALINT32_OK;
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorHard) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorLogic) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorData) 	{res32 |= STVALINT32_Alarm;}
	if (ucMDiscInBuf[MB_offsetDiscreet4] & MB_bOffsetErrorSW) 		{res32 |= STVALINT32_Alarm;}
	IedServer_updateInt32AttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_stVal, res32);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_CSWI1_Health_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Health_t, currentTime);

	}
//XCBR
//--------------------------------
	{
	//Pos
	uint8_t	res=STVALCODEDENUM_INTERMEDIATE;
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_OFF) {res |= STVALCODEDENUM_OFF;}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_offsetSW_ON)  {res |= STVALCODEDENUM_ON;} 			//2 - STATE_ON ? 1- STATE_OFF ? 0 -  STATE_INTERMEDIATE

	IedServer_updateBitStrinAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_stVal, res);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Pos_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Pos_t, currentTime);

	//Loc
	bool XCBR1_Loc=0;
	if (ucMConfigBufall[MB_offset_BlockSDTU]) {XCBR1_Loc = 1;}
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_stVal, XCBR1_Loc);
	IedServer_updateQuality(iedServer,&iedModel_CTRL_XCBR1_Loc_q,0);
	IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_XCBR1_Loc_t, currentTime);
	}

//	IedServer_updateInt32AttributeValue(iedServer, iedModel_CTRL_XCBR1_OpCnt_stVal,(ucMConfigBufall[MB_offset_BlockSDTU]);

	//--------------------------------

//LEDGGIO1
//--------------------------------
 	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<8));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<9));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<10));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<11));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<12));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<13));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<14));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<15));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<4));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<5));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<6));
	IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_stVal,  ucMDiscInBuf[MB_offsetLED] & (1<<7));

	// ������ ������ �����������
	if (ucMDiscInBuf[MB_offsetLED] & 0xFF00){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_t, currentTime);

	}


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind1_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind2_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind3_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind4_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind5_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind6_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind7_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind8_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind9_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind10_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind11_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Ind12_t, currentTime);


    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind1_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind2_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind3_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind4_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind5_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind6_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind7_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind8_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind9_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind10_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind11_q,0);
    IedServer_updateQuality(iedServer,&iedModel_GGIO_LEDGGIO1_Ind12_q,0);

//--------------------------------
// ����� ������

	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetError){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO1_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO2_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_t, currentTime);
	}
	if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetErrorNote){
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  1);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);

	} else{
		IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_stVal,  0);
	    IedServer_updateQuality(iedServer,&iedModel_CTRL_GGIO1_SPCSO3_q,0);
	    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_t, currentTime);
	}

//--------------------------------
// MSQI1
	int32_t MSQI_stval = 0;

	if (ucMDiscInBuf[MB_offsetError] & (MB_errorMRV1|MB_errorMSAI)) {
		MSQI_stval = 2;
	} else		MSQI_stval = 1;

	IedServer_updateInt32AttributeValue(iedServer, &iedModel_MES_MSQI1_Health_stVal,  MSQI_stval);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_Health_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_Health_t, currentTime);

// SeqA
    float	cXtmp = (float)ucMAnalogInBuf[MB_offset_NI1] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c1_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c1_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI2] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c2_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c2_t, currentTime);

    cXtmp = (float)ucMAnalogInBuf[MB_offset_NI0] * 40 * Ktt / 65536;
    IedServer_updateFloatAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_cVal_mag_f,  cXtmp);
    IedServer_updateQuality(iedServer,&iedModel_MES_MSQI1_SeqA_c3_q,0);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MSQI1_SeqA_c3_t, currentTime);

//--------------------------------
    // ��������� ��������
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LLN0_Mod_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_Proxy_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_LPHD1_PhyHealth_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_OUTGGIO1_Mod_t, currentTime);
//LEDGGIO1
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_Mod_t, currentTime);


    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_MES_MMXU1_Mod_t, currentTime);

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Mod_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Beh_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Health_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Str_t, currentTime);
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_PROT_PTOC1_Op_t, currentTime);


    // ��������� ������ �������
    uint8_t	currSG = 1;
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSettingGr) currSG = 2;

	if (currSG != IedServer_getActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0))) {
		IedServer_changeActiveSettingGroup(iedServer,LogicalDevice_getSettingGroupControlBlock(&iedModel_Generic_LD0),currSG );
		USART_TRACE("���������� ������ �������. %u\n",iedModel_LD0_LLN0_sgcb0.actSG);
	}

	// -------------------- ���� �� ��������� � �������� ---------------------------
    if (ucMDiscInBuf[MB_offset_adr0] & MB_bOffsetSysNote) {
    	// ���� ��������� �������. ����� �� ����������.
    	USART_TRACE("���������� �������.\n");
    	writeNmb = 9;
    	ucMDiscInBuf[MB_offset_adr0] ^= MB_bOffsetSysNote;
    }


    IedServer_unlockDataModel(iedServer);																// ����� ���������� mmsServer'��
}
//+++++++++++++++++++
		}

		// ������ � ������
//		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
//		if (errorType == MB_ETIMEDOUT){
//   		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
//    		//if (ReadNmb) ReadNmb--;
//    	Port_Off(LED1);
//		}

		errorType = eMBMasterPoll();						// ��������� ������� �� MODBUS.
		if (errorType == MB_ETIMEDOUT){
    		USART_TRACE_RED("ReadNmb: %u\n",ReadNmb);
    		//if (ReadNmb) ReadNmb--;
    	Port_Off(LED1);
		}else
		if (errorType == MB_ENOERR){
//			if (writeNmb == 0) ReadNmb++;
			if (writeNmb == 10) {writeNmb = 0;}
		}

		taskYIELD();										// �������� ������.
	}
}
#endif


/*************************************************************************
 * GetRealP
 * ktn - ����������� �� ����������� �� ������� ������� 0x1009
 *************************************************************************/
float		GetRealP(float	Currdata, uint16_t	ktn,  uint16_t	Ittf){

float	realP = 0;
float	K;

	if(ktn & 1<<15){
		K = ( ((float)ktn - (float)32768)* (float)1000) /(float)256;
	} else{
		K = (float)ktn/(float)256;
	}
	realP = (float)Currdata/65536*Ittf*K*1.25;

return	realP;
}

/*************************************************************************
 * ���������� ������������
 *************************************************************************/
void	CSWI_Pos_Oper_Set(bool newState, uint64_t timeStamp){
#if defined (MR771)
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_ctlVal, newState);
    if (newState) writeNmb = 1; else writeNmb = 2;
#endif
/*************************************************************************
 * MR761 MR762 MR763
 *************************************************************************/
#if defined (MR761) || defined (MR762) || defined (MR763)
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_ctlVal, newState);
    if (newState) writeNmb = 1; else writeNmb = 2;
#endif
#if defined (MR801)
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_ctlVal, newState);
    if (newState) writeNmb = 1; else writeNmb = 2;
#endif
#if defined (MR901)
#endif
#if defined (MR902)
#endif
#if defined (MR5_700)
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_T, timeStamp);
    IedS&iedModel_ateBooleanAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_ctlVal, newState);
    if (newState) writeNmb = 1; else writeNmb = 2;
#endif
#if defined (MR5_500)
    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_CSWI1_Pos_Oper_ctlVal, newState);
    if (newState) writeNmb = 1; else writeNmb = 2;
#endif
}

/*************************************************************************
 * ������� ������ ������ � ���������
 *************************************************************************/
void	GGIO_LEDGGIO1_SPCSO1_Oper(bool newState, uint64_t timeStamp){

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_GGIO_LEDGGIO1_SPCSO1_Oper_ctlVal, 0);
    if (newState) writeNmb = 3; //else writeNmb = 2;

}

void	GGIO_SPCSO1_Oper(bool newState, uint64_t timeStamp){

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO1_Oper_ctlVal, 0);
    if (newState) writeNmb = 4; //else writeNmb = 2;

}
void	GGIO_SPCSO2_Oper(bool newState, uint64_t timeStamp){

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO2_Oper_ctlVal, 0);
    if (newState) writeNmb = 5; //else writeNmb = 2;

}
void	GGIO_SPCSO3_Oper(bool newState, uint64_t timeStamp){

    IedServer_updateUTCTimeAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_Oper_T, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, &iedModel_CTRL_GGIO1_SPCSO3_Oper_ctlVal, 0);
    if (newState) writeNmb = 6; //else writeNmb = 2;

}
