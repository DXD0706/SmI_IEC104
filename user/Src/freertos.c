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
#include "cmsis_os.h"
#include "queue.h"

#include "main.h"
#include "time.h"
#include "stdlib.h"
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
/*Modbus includes ------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"
#include "modbus.h"

// ������ SPI
#include "at45db161d.h"
/* Variables -----------------------------------------------------------------*/


// ���������� ��������� ��� ������ ������ ������� MODBUS
	typedef struct					// ��� �������� ����� ������� ��������.
	{
	  MBFrame	MBData;
	  uint8_t 	Source;
	} xData;


//extern xQueueHandle 	  xQueueMODBUS;				//��� ���������� ������ �� �������

osThreadId defaultTaskHandle;

extern osMessageQId xQueueMODBUSHandle;

extern RTC_HandleTypeDef hrtc;
extern RTC_TimeTypeDef sTime;
extern RTC_DateTypeDef sDate;

extern UART_HandleTypeDef MODBUS;

//  --------------------------------------------------------------------------------
//Master mode: ��������� ���������� ������
extern USHORT   usMDiscInStart;
#if      M_DISCRETE_INPUT_NDISCRETES%8
extern UCHAR    ucMDiscInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_DISCRETE_INPUT_NDISCRETES/8+1];
#else
extern UCHAR    ucMDiscInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_DISCRETE_INPUT_NDISCRETES/8];
#endif
//Master mode:Coils variables
extern USHORT   usMCoilStart;
#if      M_COIL_NCOILS%8
extern UCHAR    ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS/8+1];
#else
extern UCHAR    ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS/8];
#endif
//Master mode: ��������� ������� ���������
extern USHORT   usMRegInStart;
extern USHORT   usMRegInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_INPUT_NREGS];
//Master mode:��������� �������� ���������
extern USHORT   usMRegHoldStart;
extern USHORT   usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];
//  --------------------------------------------------------------------------------

struct netconn *conn, *newconn;
struct netconn *conn850,*newconn850;

struct netif 	first_gnetif,second_gnetif;
struct ip_addr 	first_ipaddr,second_ipaddr;
struct ip_addr 	netmask;
struct ip_addr 	gw;

struct iechooks default_hooks;
struct iecsock 	*s;

uint8_t *outbuf;
size_t  outbufLen;

/* Function prototypes -------------------------------------------------------*/

void StartIEC104Task(void const * argument);
void StartTimersTask(void const * argument);
void StartMODBUSTask(void const * argument);

static int iec104_iframe_recv(struct netconn *newconn, struct iecsock *s, struct iec_buf *buf);
static int iec104_sframe_recv(struct iecsock *s, struct iec_buf *buf);
static int iec104_uframe_recv(struct netconn *newconn, struct iecsock *s, struct iec_buf *buf);

//static void iec104_uframe_send(struct iecsock *s, enum uframe_func func);
//static void iec104_sframe_send(struct iecsock *s);
void iec104_prepare_iframe(struct iec_buf *buf);

static inline enum frame_type frame104_type(struct iechdr *h);
static inline enum uframe_func uframe_func(struct iechdr *h);

static inline int check_nr(struct iecsock *s, unsigned short nr);
static inline int check_ns(struct iecsock *s, unsigned short ns);

void activation_hook(struct iecsock *s);
void connect_hook(struct iecsock *s);
void disconnect_hook(struct iecsock *s, short reason);
void data_received_hook(struct iecsock *s, struct iec_buf *b);

void iec104_set_options(struct iecsock *s, struct iecsock_options *opt);
static void iec104_set_defaults(struct iecsock *s);

void send_base_request(struct iecsock *s, void *arg);
void iec_send_frame(struct iecsock *s, u_char *buf, size_t *buflen);
void iec104_run_write_queue(struct iecsock *s);
size_t iec104_can_queue(struct iecsock *s);


static inline char * uframe_func_to_string(enum uframe_func func);


//extern void MX_LWIP_Init(void);
//extern void tcpecho_init(void);
//extern void udpecho_init(void);

void FREERTOS_Init(void);


/* Hook prototypes */


/*************************************************************************
 * FREERTOS_Init
 *************************************************************************/
void FREERTOS_Init(void) {
 size_t	fre;
  /* USER CODE BEGIN Init */
       
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
//	vSemaphoreCreateBinary( xBinarySemaphore );			//��������� �������� �������
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */


  fre = xPortGetFreeHeapSize();			// ������ ����
  USART_TRACE("FreeHeap:%u\n",fre);

  osThreadDef(IEC850, StartIEC850Task, osPriorityAboveNormal,0, 1500);//1024
  defaultTaskHandle = osThreadCreate(osThread(IEC850), NULL);
  fre = xPortGetFreeHeapSize();
  USART_TRACE("FreeHeap(IEC850):%u\n",fre);

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
  osThreadDef(MBUS, StartMODBUSTask, osPriorityNormal,0, 128);//256
  defaultTaskHandle = osThreadCreate(osThread(MBUS), NULL);

  fre = xPortGetFreeHeapSize();
  USART_TRACE("FreeHeap(MBUS):%u\n",fre);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_QUEUES */
//   xQueueMODBUS = xQueueCreate( 3, sizeof(xData) );

  /* definition and creation of xQueueMODBUS */
//  osMessageQDef(xQueueMODBUS, 3, sizeof(xData));
//  xQueueMODBUSHandle = osMessageCreate(osMessageQ(xQueueMODBUS), NULL);

   fre = xPortGetFreeHeapSize();
   USART_TRACE("FreeHeap(Queue):%u\n",fre);

  /* USER CODE END RTOS_QUEUES */
}


/*************************************************************************
 * StartIEC104Task
 *************************************************************************/
void StartIEC104Task(void const * argument)
{
	 err_t err, accept_err;
	 struct netbuf *buf;
	 struct iechdr *h;
	 struct iec_buf *bufIEC;

	 uint16_t len;
	 uint8_t *data;
	 int ret = 0;

	//	MX_LWIP_Init();		// ����. LWIP

		tcpip_init( NULL, NULL );
		// ������������� IP ��������� ��� ���������� IP ����������, ������ ������
		IP4_ADDR(&first_ipaddr, first_IP_ADDR0, first_IP_ADDR1, first_IP_ADDR2, first_IP_ADDR3);
		// ������������� IP ��������� ��� ���������� IP ����������, ������ ����
		IP4_ADDR(&second_ipaddr, second_IP_ADDR0, second_IP_ADDR1, second_IP_ADDR2, second_IP_ADDR3);
		// ������������� ����� �������.
		IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
		// ������������� ����� �����.
		IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);

		// �������  � ������������ NETWORK ���������
		netif_add(&first_gnetif, &first_ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
		netif_set_default(&first_gnetif);

		if (netif_is_link_up(&first_gnetif))	netif_set_up(&first_gnetif);		// When the netif is fully configured this function must be called
		else									netif_set_down(&first_gnetif);		// When the netif link is down this function must be called
		//dhcp_start(&first_gnetif);		// �������������� ��������� IP

		default_hooks.disconnect_indication = disconnect_hook;						// disconnect_indication - called when link layer connection terminates.
		default_hooks.connect_indication = connect_hook;							// connect_indication - called when link layer connection is established.
		default_hooks.data_indication = data_received_hook;							// data_activation - called when ASDU was received, buf points to allocated structure. It is user responsibility to free allocated resources.
		default_hooks.activation_indication = activation_hook;						// activation_indication - called when monitor direction activates with  STARTACT/STARTCON S-frames.
	//	default_hooks.deactivation_indication = deactivation_hook;					// deactivation_indication - called when monitor direction deactivates  with STOPACT/STOPCON S-frames.
	//	default_hooks.transmit_wakeup = transmit_wakeup;							// transmit_wakeup - called when all frames from transmition queue were sent, acknowledged and iecsock can accept more.

		s = calloc(1, sizeof(struct iecsock));

		if (!s)		{USART_TRACE("������ ��������� ������ ��� s ��� �����.\n");}
		else		{USART_TRACE("�������� ������ ��� s: %u ����.\n",sizeof(struct iecsock));}

		iec104_set_defaults(s);														// ��������� ��������� 104 �� ��������� � �������� s ������.
		s->type = IEC_SLAVE;
		s->stopdt = 1;

		for(;;) {
		conn = netconn_new(NETCONN_TCP);											// �������� ����� ����������

		if (conn!=NULL){
			USART_TRACE("netconn_new TCP ... \n");
			err = netconn_bind(conn, &first_ipaddr, IEC104_Port);					// �������� ���������� �� IP � ���� IEC 60870-5-104 (IEC104_Port)
//TODO: ����������� � etconn_delete(newconn); ������ newconn ????
			if (err != ERR_OK) 		netconn_delete(conn);						//
		    else
		    {
				USART_TRACE("netconn_listen ... \n");
		        netconn_listen(conn);												// ��������� ���������� � ����� �������������
		        while (1)															// ����� ������� �  ����������� ����������
		        {
		             accept_err = netconn_accept(conn, &newconn);					// ��������� ����������
		             if (accept_err == ERR_OK)										// ���� �������, �� ���������� ���
		             {
		     			USART_TRACE("netconn_accept ... ok \n");
		                 while (netconn_recv(newconn, &buf) == ERR_OK)				// ��������� ������ � �����
		                 {
		                	 Port_On(LED1);
		                     do
		                     {
		                        netbuf_data(buf,(void *)&data, &len);				// ��������� ������� ����� ��. ������
		                   	    h = (struct iechdr *) &data[0];

		    	     			bufIEC = calloc(1,len + sizeof(struct iec_buf) - sizeof(struct iechdr));		// �������� ���� ������ ��� bufIEC

		                   		t3_timer_stop(s);
		                   		t3_timer_start(s);

		                   		memcpy(&s->buf[0], h, len);
		                   		memcpy(&bufIEC->h, h, len);
		                   		s->len = len;										// ������ ��������� ������.

		                   		// ---------------------------------------------------
								switch (frame104_type(h)) {	                   		// ���������� ��� ������.
									case FRAME_TYPE_I:								// ������� �������� ���������� � ����������.
										if (s->type == IEC_SLAVE && s->stopdt) 	break;
										ret = iec104_iframe_recv(newconn, s, bufIEC);
									break;
									case FRAME_TYPE_S:								// ������� �������� � ����������.
										ret = iec104_sframe_recv(s, bufIEC);
									break;
									case FRAME_TYPE_U:								// ������� ���������� ��� ���������.
										ret = iec104_uframe_recv(newconn, s, bufIEC);
									break;
								}
								if (ret) {											// ���� ���������� ������� ������ ������
									s->vr = 0;
									s->vs = 0;
									s->va = 0;
									USART_TRACE("active close TCP/IP\n");
									netbuf_delete(buf);
									free(bufIEC);
				                    goto TCPCLOSE;
								}
								// ---------------------------------------------------
								free(bufIEC);
	    	     				USART_TRACE("��������� ������ ����� bufIEC.\n");
		                     }
		                     while (netbuf_next(buf) >= 0);
		                     netbuf_delete(buf);

		  		           Port_Off(LED1);
		                 }
		         TCPCLOSE:
		         	 	 USART_TRACE("netconn_close TCPCLOSE:\n");
		                 netconn_close(newconn);									// ��������� � ����������� ����������
		                 netconn_delete(newconn);
		                 t1_timer_stop(s);
		                 t2_timer_stop(s);
		                 t3_timer_stop(s);
		             } else 	USART_TRACE("netconn_accept error: %d\n", accept_err);
		        }//! while (1)
		    }
		  } //! if (conn!=NULL)
	  }
}
/*************************************************************************
 * StartTimersTask
 * ���������� ��������� 104 ���������.
 *************************************************************************/
void StartTimersTask(void const * argument)
{
	  t0_timer_stop(s);
	  t1_timer_stop(s);
	  t2_timer_stop(s);
	  t3_timer_stop(s);

	  for(;;)
	  {
        if (s->t1.evnt) t1_timer_run(s);		// �������� ������� ������������ ��������.
        if (s->t2.evnt) t2_timer_run(s);
        if (s->t3.evnt) t3_timer_run(s);
        taskYIELD();							// �������� ������.
	  }
}
/*************************************************************************
 * StartMODBUSTask
 *************************************************************************/
void StartMODBUSTask(void const * argument)
{
	uint32_t	TimerReadMB;
	uint8_t		ReadNmb=0;

	eMBMasterInit(MB_RTU, 4, 115200,  MB_PAR_NONE);
	eMBMasterEnable();

	TimerReadMB = HAL_GetTick();
	for(;;)
	{
		if ((HAL_GetTick()-TimerReadMB)>100){					// ������������� ����� ���������
			TimerReadMB = HAL_GetTick();
			ReadNmb++; if (ReadNmb>1) ReadNmb = 0;
	   	   	if (ReadNmb==0) eMBMasterReqReadCoils(MB_Slaveaddr,MB_StartDiscreetaddr,MB_NumbDiscreet,RT_WAITING_FOREVER);
	   		if (ReadNmb==1) eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartAnalogINaddr,MB_NumbAnalogIN,RT_WAITING_FOREVER);
		}

		eMBMasterPoll();						// ��������� ������� �� MODBUS.
		taskYIELD();							// �������� ������.
	}
}

/*************************************************************************
 * iecsock_iframe_recv
 *************************************************************************/
static int iec104_iframe_recv(struct netconn *newconn, struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;
	buf->data_len = h->length ;										// �������� �������� �������� ASDU

	if (!check_nr(s, h->ic.nr))	{									// �������� ����������� ���������� �����
		USART_TRACE("not check_nr. s->vs:%d h->ic.nr:%d\n",s->vs, h->ic.nr);
		return -1;
	}

	s->va = h->ic.nr;
	if (s->va == s->vs)		t1_timer_stop(s);						// ���� ����� ����������� ������ � ��������.

	t2_timer_stop(s);												// ������� �������������.
	t2_timer_start(s);

	if (!check_ns(s, h->ic.ns)) {
		USART_TRACE("not check_ns: s->vr:%i h->ic.ns:%i\n",s->vr,h->ic.ns );
		return -1;													// �������� ������������ ���������� �����
	}
	s->vr = (s->vr + 1) % 32767;									// ����������� ����� ���������

	if ((s->vr - s->va_peer + 32767) % 32767 == s->w) {				// ������� Sframe ������ "w" �������� �������
		iec104_sframe_send(s);
		s->va_peer = s->vr;
	}

	if (s->hooks.data_indication)				s->hooks.data_indication(s, buf);			//�������������� ASDU
	else if (default_hooks.data_indication)		default_hooks.data_indication(s, buf);

	return 0;
}
/*************************************************************************
 * iecsock_sframe_recv
 *************************************************************************/
static int iec104_sframe_recv(struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;
	buf->data_len = h->length - 2;

	if (!check_nr(s, h->ic.nr))	return -1;

	s->va = h->ic.nr;
	if (s->va == s->vs) {
		t1_timer_stop(s);																	// ������� ��� ������� ��� ������������ APDU.
		//if (s->hooks.transmit_wakeup)				s->hooks.transmit_wakeup(s);			// ���� ���� �������� �����������
		//else if (default_hooks.transmit_wakeup)		default_hooks.transmit_wakeup(s);
	}
	return 0;
}
/*************************************************************************
 * iecsock_uframe_recv
 *************************************************************************/
static int iec104_uframe_recv(struct netconn *newconn, struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;

	switch(uframe_func(h)) {
		case STARTACT:
			if (s->type != IEC_SLAVE)	return -1;

			s->stopdt = 0;
			s->vs = 0;
			s->vr = 0;
			s->va_peer= 0;
			iec104_uframe_send(s, STARTCON);

			if (s->hooks.activation_indication)					s->hooks.activation_indication(s);
			else if (default_hooks.activation_indication)		default_hooks.activation_indication(s);

		break;
		case STARTCON:
			if (s->type != IEC_MASTER)	return -1;

			t1_timer_stop(s);
			s->stopdt = 0;

			if (s->hooks.activation_indication)					s->hooks.activation_indication(s);
			else if (default_hooks.activation_indication)		default_hooks.activation_indication(s);
		break;

		case STOPACT:
			if (s->type != IEC_SLAVE)	return -1;

			s->stopdt = 1;
			iec104_uframe_send(s, STOPCON);

			if (s->hooks.activation_indication)					s->hooks.activation_indication(s);
			else if (default_hooks.activation_indication)		default_hooks.activation_indication(s);
		break;

		case STOPCON:
			if (s->type != IEC_MASTER)	return -1;

			s->stopdt = 1;

			if (s->hooks.activation_indication)					s->hooks.activation_indication(s);
			else if (default_hooks.activation_indication)		default_hooks.activation_indication(s);
		break;

		case TESTACT:
			iec104_uframe_send(s, TESTCON);

			if (s->type == IEC_SLAVE && !s->testfr)		t3_timer_stop(s);
		break;

		case TESTCON:
			if (!s->testfr)		return -1;
			t1_timer_stop(s);
			s->testfr = 0;
		break;

	}
	return 0;
}
/*************************************************************************
 * iec104_uframe_send
 *************************************************************************/
void iec104_uframe_send(struct iecsock *s, enum uframe_func func)
{
	struct iechdr h;
	memset(&h, 0, sizeof(struct iechdr));				// ������� h

	USART_TRACE("TX U %s stopdt:%i \n", uframe_func_to_string(func), s->stopdt);

	h.start = 0x68;
	h.length = sizeof(struct iec_u);
	h.uc.ft = 3;

	if (func == STARTACT)			h.uc.start_act = 1;
	else if (func == STARTCON)		h.uc.start_con = 1;
	else if (func == STOPACT)		h.uc.stop_act = 1;
	else if (func == STOPCON)		h.uc.stop_con = 1;
	else if (func == TESTACT)		h.uc.test_act = 1;
	else if (func == TESTCON)		h.uc.test_con = 1;

	netconn_write(newconn, &h, sizeof(h), NETCONN_COPY);
	s->xmit_cnt++;
}
/*************************************************************************
 * iec104_sframe_send
 *************************************************************************/
void iec104_sframe_send(struct iecsock *s)
{
	struct iechdr h;
	memset(&h, 0, sizeof(h));

	USART_TRACE("TX S N(r)=%i \n", s->vr);

	h.start = 0x68;
	h.length = sizeof(struct iec_s);
	h.sc.res1 = 0;
	h.sc.res2 = 0;
	h.sc.ft = 1;
	h.sc.nr = s->vr;

    netconn_write(newconn, &h, sizeof(h), NETCONN_COPY);
	s->xmit_cnt++;
}
/*************************************************************************
 * iec104_prepare_iframe
 *************************************************************************/
void iec104_prepare_iframe(struct iec_buf *buf)
{
	struct iechdr *h;

	h = &buf->h;
	h->start = 0x68;
	h->length = buf->data_len + sizeof(struct iec_i);

	h->ic.res = 0;
	h->ic.ns = 0;
	h->ic.nr = 0;

	h->ic.ft = 0;

}
/*************************************************************************
 * frame104_type
 *************************************************************************/
static inline enum frame_type frame104_type(struct iechdr *h)
{
	if (!(h->raw[0] & 0x1))			return FRAME_TYPE_I;
	else if (!(h->raw[0] & 0x2))	return FRAME_TYPE_S;
	else							return FRAME_TYPE_U;
}
/*************************************************************************
 * uframe_func
 *************************************************************************/
static inline enum uframe_func uframe_func(struct iechdr *h)
{
	if (h->raw[0] & 0x4)			return STARTACT;
	else if (h->raw[0] & 0x8)		return STARTCON;
	else if (h->raw[0] & 0x10)		return STOPACT;
	else if (h->raw[0] & 0x20)		return STOPCON;
	else if (h->raw[0] & 0x40)		return TESTACT;
	else							return TESTCON;
}
/*************************************************************************
 * check_nr
 * �������� ����������� ������ ��������� �����.
 *************************************************************************/
static inline int check_nr(struct iecsock *s, unsigned short nr)
{
	return ((nr - s->va + 32767) % 32767 <= (s->vs - s->va + 32767) % 32767);
}
/*************************************************************************
 * check_ns
 * �������� ����������� ������ ������������� �����.
 *************************************************************************/
static inline int check_ns(struct iecsock *s, unsigned short ns)
{
	return (ns == s->vr);
}
/*************************************************************************
 * activation_hook
 *************************************************************************/
void activation_hook(struct iecsock *s)
{

	USART_TRACE("send_base_request...\n");
	send_base_request(s,NULL);

}
/*************************************************************************
 * send_base_request
 *************************************************************************/
void send_base_request(struct iecsock *s, void *arg)
{
/*	struct timeval tv; */
	u_char *bufreq;
	size_t buflen = 0;

	bufreq = calloc(1,sizeof(struct iec_buf) + 249);
	if (!bufreq)	return;

	buflen+=sizeof(struct iechdr);
	iecasdu_create_header_all(bufreq+buflen, &buflen, C_IC_NA_1,1,SINGLE,ACTIVATION,NOTTEST,ACTIVATIONOK,0,0,0);
	iecasdu_create_type_100(bufreq+buflen,&buflen);									// 100
	iec_send_frame(s,bufreq,&buflen);

	free(bufreq);


}
/*************************************************************************
 * iec_send_frame
 *************************************************************************/
void iec_send_frame(struct iecsock *s, u_char *buf, size_t *buflen){

	struct iec_buf *b;

	if (! iec104_can_queue(s))	return;

	b = calloc(1, sizeof(struct iec_buf) + *buflen);
	if (!b)	return;

	b->data_len = *buflen;
	memcpy(b->data, buf, *buflen );

	iec104_prepare_iframe(b);
	//TAILQ_INSERT_TAIL(&s->write_q, b, head);
	iec104_run_write_queue(s);

	free(b);
}
/*************************************************************************
 * iec104_run_write_queue
 *************************************************************************/
void iec104_run_write_queue(struct iecsock *s)
{
	struct iechdr *h;
	struct iec_buf *b;

	if (s->type == IEC_SLAVE && s->stopdt)	return;

	// ����� �� ����� ����� ��� ������ �� �����(�������� ���)

	h = &b->h;
	h->ic.res = 0;
	h->ic.nr = s->vr;
	h->ic.ns = s->vs;

	USART_TRACE("TX I V(s)=%d V(a)=%d N(s)=%d N(r)=%d \n", s->vs, s->va, h->ic.ns, h->ic.nr);

	if (t1_timer_pending(s))	t1_timer_stop(s);
	t1_timer_start(s);

    netconn_write(newconn, &h, sizeof(struct iechdr), NETCONN_COPY);

	s->vs = (s->vs + 1) % 32767;
	s->xmit_cnt++;

/*
	while ( !(TAILQ_EMPTY(&s->write_q)) && (s->vs != (s->va + s->k) % 32767) ) {

		b = TAILQ_FIRST(&s->write_q);
		TAILQ_REMOVE(&s->write_q, b, head);

		h = &b->h;
		h->ic.nr = s->vr;
		h->ic.ns = s->vs;

		if (t1_timer_pending(s))	t1_timer_stop(s);
		t1_timer_start(s);

		bufferevent_write(s->io, h, h->length + 2);
		TAILQ_INSERT_TAIL(&s->ackw_q, b, head);

		s->vs = (s->vs + 1) % 32767;
		s->xmit_cnt++;
	}
*/
}
/*************************************************************************
 * iec104_can_queue
 *************************************************************************/
size_t iec104_can_queue(struct iecsock *s)
{
	return (s->k - ((s->vs - s->va + 32767) % 32767));
}
/*************************************************************************
 * connect_hook
 *************************************************************************/
void connect_hook(struct iecsock *s)
{
	struct iecsock_options opt;
	opt.w	= 1;
	opt.k	= 3;
	opt.t0	= 30;
	opt.t1  = 15;
	opt.t2  = 10;
	opt.t3  = 20;
	iec104_set_options(s,&opt);

//	USART_TRACE(stderr, "%s: Sucess 0x%lu\n", __FUNCTION__, (unsigned long) s);

}
/*************************************************************************
 * disconnect_hook
 * ���������� TCP
 *************************************************************************/
void disconnect_hook(struct iecsock *s, short reason)
{
	//USART_TRACE("%s: what=0x%02x\n", __FUNCTION__, reason);
	return;
}
/*************************************************************************
 * data_received_hook
 *  ���� ������
 *************************************************************************/
void data_received_hook(struct iecsock *s, struct iec_buf *b)
{

//	struct netconn *newconn;
	struct iec_object obj[IEC_OBJECT_MAX];
	int ret, n, i;
	u_short caddr;
	u_char cause, test, pn, t, str_ioa;

	struct iechdr *h;
	uint8_t apcibuf[6];

	eMBMasterReqErrCode    errorCode = MB_MRE_NO_ERR;


	h = (struct iechdr *)&apcibuf;
	// ������ ���������� ASDU
   ret = iecasdu_parse(obj, &t, &caddr, &n, &cause, &test, &pn, &str_ioa, b->data, b->data_len);
   if (!ret){
	   switch(t) {
	   //--------- M -------------
		   case M_SP_NA_1: /* 1 */			// �������������� ����������

						   break;
		   case M_DP_NA_1: /* 3 */			// �������������� ����������

						   break;
		   case M_ST_NA_1: /* 5 */			// ���������� � ��������� ������

						   break;
		   case M_BO_NA_1: /* 7 */			// ������ �� 32 ���

						   break;
		   case M_ME_NA_1: /* 9 */			// �������� ���������� ��������, ��������������� ��������

						   break;
		   case M_ME_NB_1: /* 11 */			// �������� ���������� ��������, ���������������� ��������

						   break;
		   case M_ME_NC_1: /* 13 */			// �������� ���������� ��������, �������� ������ � ����.�����.

						   break;
		   case M_SP_TB_1: /* 30 */			// �������������� ���������� � ������ ������� CP56�����2�

						   break;
		   case M_BO_TB_1: /* 33 */			// ������ �� 32 ��� � ������ ������� CP56Time2a

						   break;
		   case M_ME_TD_1: /* 34 */			// �������� ���������� ��������, ��������������� �������� � ������ ������� CP56Time2a

						   break;
		   case M_ME_TE_1: /* 35 */			// �������� ���������� ��������, ���������������� �������� � ������ ������� CP56Time2a

						   break;
		   case M_ME_TF_1: /* 36 */			// �������� ���������� ��������, �������� ������ � ��������� ������� � ������ ������� CP56�����2�

						   break;
		   case M_IT_TB_1: /* 37 */			// ������������ ����� � ������ ������� CP56Time2a

						   break;
		   case M_EP_TD_1: /* 38 */			// �������� ��������� ������ � ������ ������� CP56Time2a

						   break;
		   case M_EP_TE_1: /* 39 */			// ����������� ���������� � ������������ �������� ������� ������ � ������ ������� CP56Time2a

						   break;
		   case M_EP_TF_1: /* 40 */			// ����������� ���������� � ������������ �������� ����� ��������� ������ � ������ ������� CP56Time2a

						   break;
	//--------- C -------------
		   case C_IC_NA_1: /* 100 */		//ASDU Type ID 100 : C_IC_NA_1 - ������� ������

//				   	   	   errorCode = eMBMasterReqReadHoldingRegister(MB_Slaveaddr,MB_StartAnalogINaddr,MB_NumbAnalogIN,RT_WAITING_FOREVER);


   	   	   	   	   	   	    outbuf = calloc(1,sizeof(struct iec_buf) + 249);
							if (!outbuf) {
								USART_TRACE("������ ��������� ������ outbuf � C_IC_NA_1 �������.\n");
								return;
							} else{
								USART_TRACE("�������� ������ outbuf � C_IC_NA_1: %u ����.\n",sizeof(struct iec_buf) + 249);
							}
							outbufLen = 0;


							// ���� APCI
							outbufLen+=sizeof(struct iechdr);
							//iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, C_IC_NA_1,1,SINGLE,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,5,2);
   	   	   	   	   	   	    //iecasdu_create_type_100(outbuf+outbufLen,&outbufLen);									// 100

//							iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, M_ME_NA_1,1,SINGLE,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,5,2);

//							iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, M_ME_NA_1,MB_NumbAnalogIN,SERIAL,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,5,0);
//							for (i=MB_StartAnalogINaddr;i<(MB_StartAnalogINaddr+MB_NumbAnalogIN);i++){
//								iecasdu_create_type_9(outbuf+outbufLen,&outbufLen, usMRegInBuf[0][i]); //M_ME_NA_1 - �������� ���������� ��������, ��������������� ��������
//							}


							iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, M_ME_NA_1,MB_NumbAnalogIN,SERIAL,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,MB_Slaveaddr,MB_StartAnalogINaddr);
							for (i=0;i<MB_NumbAnalogIN;i++){
								iecasdu_create_type_9(outbuf+outbufLen,&outbufLen, usMRegHoldBuf[0][i]); //M_ME_NA_1 - �������� ���������� ��������, ��������������� ��������
							}
							//iecasdu_create_header(outbuf+outbufLen, &outbufLen, M_SP_NA_1, 1, 20, 1);		// ��������� ASDU �� ����� �������
   	   	   	   	   	   	    //iecasdu_create_type_1(outbuf+outbufLen,&outbufLen);										// 1
   	   	   	   	   	   	    // ------------- APCI -------------------
   	   	   	   	   	   	    iecasdu_add_APCI(outbuf, outbufLen);
   	   	   	   	   	   	    /*
							h->start = 0x68;
							h->length = outbufLen-2;
							h->ic.ft = 0;
							h->ic.ns = s->vs;
							h->ic.nr = s->vr;
							h->ic.res = 0;
							memcpy(outbuf, h, sizeof(struct iechdr));
							*/

							// !------------ APCI -------------------
							/*
							sa = outbuf--;
							printf("netconn_write...\n");
							for (i=0;i<outbufLen;i++){
								//printf("0x%02x ",s->buf[i]);
								printf("0x%02x ",*sa);
								sa++;
							}
								printf("\n");
*/

							netconn_write(newconn, outbuf , outbufLen, NETCONN_COPY);

							s->vs = (s->vs + 1) % 32767;			// ������� ����������
							s->xmit_cnt++;

							free(outbuf);
							USART_TRACE("��������� ������ outbuf � C_IC_NA_1\n");


							USART_TRACE("| 100 C_IC_NA_1: IDX:%i qoi:0x%02x\n",obj[0].ioa, obj[0].o.type100.qoi);
						   break;
		   case C_CI_NA_1: /* 101 */		//ASDU Type ID 101 : C_CI_NA_1 - Counter Interrogation Command



//			   	   	   	   errorCode = eMBMasterReqReadCoils(MB_Slaveaddr,MB_StartDiscreetaddr,MB_NumbDiscreet,RT_WAITING_FOREVER);		// ��������� ����� �������� ������ �� ��������� �������

	  	   	   	   	   	    outbuf = calloc(1,sizeof(struct iec_buf) + 249);//249
							if (!outbuf) {
								USART_TRACE("������ ��������� ������ outbuf � C_CI_NA_1 �������.\n");
								return;
							} else{
								USART_TRACE("�������� ������ outbuf � C_CI_NA_1: %u ����.\n",sizeof(struct iec_buf) + 249);
							}

							outbufLen = 0;
							// ���� APCI
							outbufLen+=sizeof(struct iechdr);
							iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, M_SP_NA_1,MB_NumbDiscreet,SERIAL,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,MB_Slaveaddr,MB_StartDiscreetaddr);

							for (i=0;i<1+MB_NumbDiscreet/8;i++){
								for (n=0;n<8;n++){
									if (ucMCoilBuf[0][i]&(1<<n))  iecasdu_create_type_1(outbuf+outbufLen,&outbufLen,1);
											else 				  iecasdu_create_type_1(outbuf+outbufLen,&outbufLen,0);
								}
							}

  	   	   	   	   	   	    // ------------- APCI -------------------
   	   	   	   	   	   	    iecasdu_add_APCI(outbuf, outbufLen);
   	   	   	   	   	   	    /*
							h->start = 0x68;
							h->length = outbufLen-2;
							h->ic.ft = 0;
							h->ic.ns = s->vs;
							h->ic.nr = s->vr;
							h->ic.res = 0;
							memcpy(outbuf, h, sizeof(struct iechdr));
							*/
							// !------------ APCI -------------------
							netconn_write(newconn, outbuf , outbufLen, NETCONN_COPY);

							s->vs = (s->vs + 1) % 32767;			// ������� ����������
							s->xmit_cnt++;

							free(outbuf);
							USART_TRACE("��������� ������ outbuf � C_CI_NA_1\n");

							USART_TRACE("| 101 C_CI_NA_1: IDX:%i rqt:0x%02x frz:0x%02x\n",obj[0].ioa, obj[0].o.type101.rqt, obj[0].o.type101.frz);

						   break;

		   case C_RD_NA_1: /* 102 */		//ASDU Type ID 102 : C_RD_NA_1 - ������� ������
			   	   	   	   USART_TRACE("| 101 C_CI_NA_1: IDX:%i \n",obj[0].ioa);
						   break;

		   case C_CS_NA_1: /* 103 */		//ASDU Type ID 103 : C_CS_NA_1 - ������� ������������� �������


			   	   	   	   sTime.Hours = obj[0].o.type103.time.hour;
			   	   	   	   sTime.Minutes = obj[0].o.type103.time.min;
			   	   	   	   sTime.Seconds = obj[0].o.type103.time.msec/1000;
			   	   	   	   //sTime.SubSeconds = (obj[0].o.type103.time.msec*60)/1000;
			    		   HAL_RTC_SetTime(&hrtc, &sTime, FORMAT_BIN);

			    		   sDate.Date = obj[0].o.type103.time.mday;
			    		   sDate.Month = obj[0].o.type103.time.month;
			    		   sDate.Year = obj[0].o.type103.time.year + 20;
			    		   sDate.WeekDay = obj[0].o.type103.time.wday;
			    		   HAL_RTC_SetDate(&hrtc, &sDate, FORMAT_BIN);


							outbuf = calloc(1,sizeof(struct iec_buf) + 249);
							if (!outbuf) {
								USART_TRACE("������ ��������� ������ outbuf � C_CS_NA_1 �������.\n");
								return;
							} else{
								USART_TRACE("�������� ������ outbuf � C_CS_NA_1: %u ����.\n",sizeof(struct iec_buf) + 249);
							}

							outbufLen = 0;

							// ���� APCI
							outbufLen+=sizeof(struct iechdr);
							// buf, buflen, type(������������� ����), num(����� ��������), sq(������������������ ��� ���������), cause(�������), t(����), pn(������������� ���������), ma(����� �����), ca(����� �������)
							iecasdu_create_header_all(outbuf+outbufLen, &outbufLen, C_CS_NA_1,1,SINGLE,ACTCONFIRM,NOTTEST,ACTIVATIONOK,0,0,1);
							iecasdu_create_type_103(outbuf+outbufLen,&outbufLen);
							// ------------- APCI -------------------
							h->start = 0x68;
							h->length = outbufLen-2;
							h->ic.ft = 0;
							h->ic.ns = s->vs;
							h->ic.nr = s->vr;
							h->ic.res = 0;
							memcpy(outbuf, h, sizeof(struct iechdr));
							// !------------ APCI -------------------
							netconn_write(newconn, outbuf , outbufLen, NETCONN_COPY);

							s->vs = (s->vs + 1) % 32767;			// ������� ����������
							s->xmit_cnt++;

							free(outbuf);
							USART_TRACE("��������� ������ outbuf � C_CS_NA_1\n");


							USART_TRACE("| 103 C_CS_NA_1: IDX:%i hour:%02d min:%02d\n",obj[0].ioa, obj[0].o.type103.time.hour, obj[0].o.type103.time.min);

						   break;
//		   case C_TS_NA_1: /* 104 */		//ASDU Type ID 104: C_TS_NA_1 - �������� �������				�� ������������ � 104 ���������
//	   	   	   	   	   	   USART_TRACE("| 104 C_TS_NA_1: IDX:%i qoi:0x%02x\n",obj[0].ioa, obj[0].o.type104.res);
						   break;
		   case C_RP_NA_1: /* 105 */		//ASDU Type ID 105: C_RP_NA_1 - Reset Process Command
			   	   	   	   USART_TRACE("| 105 C_RP_NA_1: IDX:%i qoi:0x%02x\n",obj[0].ioa, obj[0].o.type105.res);
						   break;
		   case C_TS_TA_1: /* 107 */		//ASDU Type ID 107 : C_TS_TA_1 - �������� ������� � ������ ������� CP56�����2�
			   	   	   	   USART_TRACE("| 107 C_TS_TA_1: IDX:%i qoi:0x%02x\n",obj[0].ioa, obj[0].o.type107.res);
						   break;
	   }
   }

}
/*************************************************************************
 * iecsock_set_options
 * ���� ��� � ��
 *************************************************************************/
void iec104_set_options(struct iecsock *s, struct iecsock_options *opt)
{
	s->t0.cntdest = opt->t0*1000;
	s->t1.cntdest = opt->t1*1000;
	s->t2.cntdest = opt->t2*1000;
	s->t3.cntdest = opt->t3*1000;
	s->w = opt->w;
	s->k = opt->k;
}
/*************************************************************************
 * iecsock_set_defaults
 * t0_timer	- ����-��� ��� ������������ ����������
 * t1_timer	- ����-��� ��� ������� ��� ������������
 * t2_timer	- ����-��� ��� ������������� � ������ ���������� ��������� � �������
 * t3_timer	- ����-��� ��� ������� ������ ������������ � ������ ������� �������
 *************************************************************************/
static void iec104_set_defaults(struct iecsock *s)
{
	s->t0.cntdest	= DEFAULT_T0*1000;
	s->t1.cntdest	= DEFAULT_T1*1000;
	s->t2.cntdest	= DEFAULT_T2*1000;
	s->t3.cntdest	= DEFAULT_T3*1000;
	s->w 	= DEFAULT_W;
	s->k	= DEFAULT_K;

	iec104_sframe_send(s);					// �������� s ������ ����� �����
	s->va_peer = s->vr;
}

/*************************************************************************
 * uframe_func_to_string
 *************************************************************************/
static inline char * uframe_func_to_string(enum uframe_func func)
{
	switch (func) {
	case STARTACT:
		return "STARTACT";
	case STARTCON:
		return "STARTCON";
	case STOPACT:
		return "STOPACT";
	case STOPCON:
		return "STOPCON";
	case TESTACT:
		return "TESTACT";
	case TESTCON:
		return "TESTCON";
	default:
		return "UNKNOWN";
	}
}
