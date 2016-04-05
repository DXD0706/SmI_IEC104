/*
 * IEC850.c
 *
 *  Created on: 09.12.2015
 *      Author: sagok
 */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "queue.h"
#include "signal.h"

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

#include "lwip/dhcp.h"

/* ���� --*/
#include "clocks.h"

/*debuger ------------------------------------------------------------*/
#include "DebugConsole.h"

/*iec850 includes ------------------------------------------------------------*/
#include "iec850.h"

#include "byte_stream.h"
#include "cotp.h"
#include "iso_session.h"
#include "iso_presentation.h"
#include "acse.h"
#include "iso_server.h"
#include "iec61850_server.h"
#include "static_model.h"

#include "mms_server_connection.h"

//#include "MmsPdu.h"

/*Modbus includes ------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mbport.h"
#include "modbus.h"


/* Variables -----------------------------------------------------------------*/
/* import IEC 61850 device model created from SCL-File */
//extern IedModel iedModel;

extern IedServer iedServer;

//typedef void* Semaphore;

extern struct netconn *conn850,*newconn850;

extern struct netif 	first_gnetif;
extern struct ip_addr 	first_ipaddr;
extern struct ip_addr 	netmask;
extern struct ip_addr 	gw;

extern osSemaphoreId Netif_LinkSemaphore;
extern struct link_str link_arg;
/*  ���� ------------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;
/*  -----------------------------------------------------------------*/


osThreadId defaultTaskHandle;

/*  -----------------------------------------------------------------*/
struct sIsoConnection {
    uint8_t* 		receive_buf;					// ����� �������� ������
    uint8_t* 		send_buf_1;						// ����� ���������� ������ ��� ��������
    uint8_t* 		send_buf_2;						// ����� ���������� ������ ��� ��������
    Socket			socket;
    MessageReceivedHandler msgRcvdHandler;
    IsoServer 		isoServer;
    void* 			msgRcvdHandlerParameter;
    int 			state;							// ������ ������������ ���������		ISO_CON_STATE_STOPPED/ISO_CON_STATE_RUNNING
    IsoSession* 	session;
    IsoPresentation* presentation;
    CotpConnection* cotpConnection;					// COTP
    char* 			clientAddress;					// // ����� ������� � ���� ������ 24 ������� "�����[����]"
};

typedef struct sIsoConnection* IsoConnection;


IsoConnection	Connection_create(struct netconn *conn,IsoConnection self,uint8_t* Inbuffer);
char*			Socket_getPeerAddress(struct netconn *conn);

void			observerCallback(DataAttribute* dataAttribute);
void			controlListener(void* parameter, MmsValue* value);
void			controlHandlerForBinaryOutput(void* parameter, MmsValue* value, bool test);

void 			sigint_handler(int signalId);

//uint64_t 		Hal_getTimeInMs (void);

/*************************************************************************
 * StartIEC850Task
 *************************************************************************/
void StartIEC850Task(void const * argument){

	float t = 0.f;
	bool	STvol = true;

	struct netbuf 	*buf850;
	err_t 			accept_err850;
	uint16_t 		len;
	uint8_t 		*data;
	AcseConnection 	acseConnection;
	CotpIndication 	cotpIndication;

    ByteBuffer 		responseBuffer,responseFineBuffer;



	//IsoConnection self = calloc(1, sizeof(struct sIsoConnection));
	IsoConnection self = pvPortMalloc(sizeof(struct sIsoConnection));


	tcpip_init( NULL, NULL );
	USART_TRACE_BLUE("MAC:%.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n",MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3,MAC_ADDR4,MAC_ADDR5);
	// ������������� IP ��������� ��� ���������� IP ����������, ������ ������
	IP4_ADDR(&first_ipaddr, first_IP_ADDR0, first_IP_ADDR1, first_IP_ADDR2, first_IP_ADDR3);
	USART_TRACE_BLUE("IP:%d.%d.%d.%d \n",first_IP_ADDR0, first_IP_ADDR1, first_IP_ADDR2, first_IP_ADDR3);
	// ������������� ����� �������.
	IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
	USART_TRACE_BLUE("MASK:%d.%d.%d.%d \n",NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
	// ������������� ����� �����.
	IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
	USART_TRACE_BLUE("Gateway:%d.%d.%d.%d \n",GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);

	// �������  � ������������ NETWORK ���������
	netif_add(&first_gnetif, &first_ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
	netif_set_default(&first_gnetif);
	if (netif_is_link_up(&first_gnetif))	netif_set_up(&first_gnetif);		// When the netif is fully configured this function must be called
	else									netif_set_down(&first_gnetif);		// When the netif link is down this function must be called

//	dhcp_start(&first_gnetif);		// �������������� ��������� IP

// --------------------------------------------------------------------------------------
// freertos ����� ����� ����� ������ ����������
// ������ � ������ ��������� ���:
//
// int main(void)
// ...
//	  	osThreadDef(Start, StartThread, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 5);
//		osThreadCreate (osThread(Start), NULL);
//		osKernelStart();
//		for( ;; );
// ...
//
// static void StartThread(void const * argument)
// ...
//  	osThreadDef(DHCP, DHCP_thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE * 5);
//	    osThreadCreate (osThread(DHCP), &gnetif);
//	  	osThreadDef(LED4, ToggleLed4, osPriorityLow, 0, configMINIMAL_STACK_SIZE);
//	  	osThreadCreate (osThread(LED4), NULL);
//	  for( ;; ){
//	    osThreadTerminate(NULL);		 // ������� ���� ������� ������ Thread
//	  }
// ...
//
//	������� ������:
//		void DHCP_thread(void const * argument) { ... }
//		static void ToggleLed4(void const * argument) { ... }


// --------------------------------------------------------------------------------------
/*
	// Set the link callback function, this function is called on change of link status
	// �������� ������ ������� �� ��������� ��������� ����������
	netif_set_link_callback(&first_gnetif, ethernetif_update_config);

	// �������� ������� ��� �������������� ethernetif � �������� ������
	osSemaphoreDef(Netif_SEM);
	Netif_LinkSemaphore = osSemaphoreCreate(osSemaphore(Netif_SEM) , 1 );

	link_arg.netif = &first_gnetif;
	link_arg.semaphore = Netif_LinkSemaphore;

	// Create the Ethernet link handler thread
	osThreadDef(LinkThr, ethernetif_set_link, osPriorityNormal, 0, configMINIMAL_STACK_SIZE * 5);
	osThreadCreate (osThread(LinkThr), &link_arg);

	// ������ HHCP
	osThreadDef(DHCP, DHCP_thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE * 5);
	osThreadCreate (osThread(DHCP), &first_gnetif);

	// We should never get here as control is now taken by the scheduler
	for( ;; ){
		osThreadTerminate(NULL);
	}
*/
// --------------------------------------------------------------------------------------

	for(;;) {
		// �������� ���� � ���������� � ������� �� �����
		conn850 = netconn_new(NETCONN_TCP);										// �������� ����� ���������� 61850, ���������� ���������
		if (conn850!=NULL){														// ���� ������� �������
			USART_TRACE("Create NETCONN_TCP\n");
			int8_t ready850 = netconn_bind(conn850, &first_ipaddr, IEC850_Port);		// �������� ���������� �� IP � ���� IEC 61850 (IEC850_Port)
		    if (ready850 != ERR_OK)	{netconn_delete(conn850);USART_TRACE("Create NETCONN_TCP ERROR!!!\n");}
		    else	{netconn_listen(conn850);USART_TRACE("Open port: %d\n",IEC850_Port);}		// ��������� ���������� � ����� �������������
		}
		// -------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------
		// ������������ �������� ������, ��� ������ static_model.� .h
		// ����� dynamic_model.c
		// -------------------------------------------------------------------------------------------------------------------
/*
		iedModel = IedModel_create("testmodel");
		LogicalDevice* lDevice1 	= LogicalDevice_create("SENSORS", iedModel);
		LogicalNode* lln0 			= LogicalNode_create("LLN0", lDevice1);
		DataObject* lln0_mod 		= CDC_ENS_create("Mod", (ModelNode*) lln0, 0);
		DataObject* lln0_health 	= CDC_ENS_create("Health", (ModelNode*) lln0, 0);
		SettingGroupControlBlock_create(lln0, 1, 1);

	    // Add a temperature sensor LN
	    LogicalNode* ttmp1 			= LogicalNode_create("TTMP1", lDevice1);
	    DataObject* ttmp1_tmpsv 	= CDC_SAV_create("TmpSv", (ModelNode*) ttmp1, 0, false);

	    DataAttribute* temperatureValue = (DataAttribute*) ModelNode_getChild((ModelNode*) ttmp1_tmpsv, "instMag.f");
	    DataAttribute* temperatureTimestamp = (DataAttribute*) ModelNode_getChild((ModelNode*) ttmp1_tmpsv, "t");

	    DataSet* dataSet = DataSet_create("events", lln0);
	    DataSetEntry_create(dataSet, "TTMP1$MX$TmpSv$instMag$f", -1, NULL);

	    uint8_t rptOptions = RPT_OPT_SEQ_NUM | RPT_OPT_TIME_STAMP | RPT_OPT_REASON_FOR_INCLUSION;

	    ReportControlBlock_create("events01", lln0, "events01", false, NULL, 1, TRG_OPT_DATA_CHANGED, rptOptions, 50, 0);
	    ReportControlBlock_create("events02", lln0, "events02", false, NULL, 1, TRG_OPT_DATA_CHANGED, rptOptions, 50, 0);
*/
		// -------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------
		iedServer = IedServer_create(&iedModel);								// �������� IED ����������� ����������

		// �������� ��������� ������
		//AcseAuthenticationParameter auth = calloc(1, sizeof(struct sAcseAuthenticationParameter));
		//auth->mechanism = AUTH_PASSWORD;	// 0x52 0x03 0x01 ��. acse.� auth_mech_password_oid[]
		//auth->value.password.string = "testpw";						//������
		IsoServer isoServer = IedServer_getIsoServer(iedServer);
		//IsoServer_setAuthenticationParameter(isoServer, auth);
		//USART_TRACE("�������� ��������� ������, ������: %s ��������: 0x52, 0x03, 0x01 \n",auth->value.password.string);

		// MMS ������ �������� ������� ��������
		IedServer_start(iedServer, 102);										// ������� �� �����, ������ �������� �������� ���
		// -------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------------------------------------------
		// �������� ������� �����������
        IedServer_observeDataAttribute(iedServer, IEDMODEL_GenericIO_GGIO1_NamPlt_vendor, observerCallback);			// ������� ������ ��� ������ �� �������
        IedServer_observeDataAttribute(iedServer, IEDMODEL_GenericIO_GGIO1_NamPlt_swRev, observerCallback);				// ������� ������ ��� ������ �� �������


    	//IedServer_setPerformCheckHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1,(ControlPerformCheckHandler) performCheckHandler, IEDMODEL_GenericIO_GGIO1_SPCSO1);
/*
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind1);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind2);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind3, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind3);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind4, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind4);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind5, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind5);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind6, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind6);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind7, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind7);
    	IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_Ind8, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_Ind8);
*/
        //IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1, (ControlHandler)controlListener, IEDMODEL_GenericIO_GGIO1_SPCSO1);

        // -------------------------------------------------------------------------------------------------------------------

		// ����� ���� ������ � ��������� � �����
        while (1){																	// ����� ������� �  ����������� ����������
        	accept_err850 = netconn_accept(conn850, &newconn850);
        	if (accept_err850 == ERR_OK){											// ���� �������, �� ���������� ���
        		USART_TRACE("a new connection has been received...\n");

        		Port_On(LED1);

        		Connection_create(newconn850,self,data);							// �������� ��������� ��� ������
        		AcseConnection_init(&acseConnection);
        		AcseConnection_setAuthenticationParameter(&acseConnection, IsoServer_getAuthenticationParameter(isoServer));

        		MmsServerConnection* mmsCon = MmsServerConnection_init(0,IedServer_getMmsServer(iedServer), self);



        		while (netconn_recv(newconn850, &buf850) == ERR_OK)					// ��������� ������ � �����
                {
                    USART_TRACE_GREEN(" ---------------------- netbuf Receive data ---------------------- \n");

                    t += 0.1f;
                    STvol = !STvol;

//            	    uint64_t currentTime = Hal_getTimeInMs();

/*
                    IedServer_lockDataModel(iedServer);																	// ����������� ���������� mmsServer'��
            	    IedServer_updateFloatAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_mag_f, t);			// ������� � AnIn1_mag_f ����� ��������
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_t, currentTime);	// ������� � AnIn1_t ����� ��������
            	    IedServer_updateQuality(iedServer, IEDMODEL_GenericIO_GGIO1_AnIn1_q, QUALITY_VALIDITY_GOOD | QUALITY_SOURCE_SUBSTITUTED);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind1_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind2_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind3_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind3_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind4_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind4_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind5_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind5_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind6_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind6_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind7_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind7_t, currentTime);

            	    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind8_stVal, STvol);
            	    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_Ind8_t, currentTime);

            	    IedServer_unlockDataModel(iedServer);																// ����� ���������� mmsServer'��
*/

//                    IedServer_setControlHandler(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1, (ControlHandler) controlHandlerForBinaryOutput,IEDMODEL_GenericIO_GGIO1_SPCSO1);

                    do
                    {
                    netbuf_data(buf850,(void *)&data, &len);						// ��������� ������� ����� ��. ������ � ��� ������
                    self->cotpConnection->readBuffer->buffer = data;
                    self->cotpConnection->readBuffer->maxSize = len;

                    //USART_TRACE("Receive data from a netconn. %u bytes \n",len);

                    ByteBuffer_wrap(&responseBuffer, self->send_buf_1, 0, SEND_BUF_SIZE);	// ����� ���������� ������ ��� �������� � TCP/IP
                	//USART_TRACE("������ ������ responseBuffer ����� ���������� ������� send_buf_1 � ���� ��������:%u\n",SEND_BUF_SIZE);

                	ByteBuffer_wrap(&responseFineBuffer, self->send_buf_2, 0, SEND_BUF_SIZE);
                	//USART_TRACE("������ ������ responseFineBuffer ����� ���������� ������� send_buf_2 � ���� ��������:%u\n",SEND_BUF_SIZE);



                    // ���� ������ ������������� � Payload �����
                    cotpIndication = CotpConnection_parseIncomingMessage(self->cotpConnection);	// ������ ����� � ���������� ��� ����������, ���� ������ ������������� � Payload �����
                    switch (cotpIndication) {
                        case CONNECT_INDICATION:
                        	USART_TRACE("ISO 8073 COTP connection indication\n");
                        	CotpConnection_sendConnectionResponseMessage(self->cotpConnection);
                        break;

                        case DATA_INDICATION:
                        {
                    		Port_On(LED2);
                        	//USART_TRACE("ISO 8073 COTP data indication\n");

                            ByteBuffer* 			cotpPayload = CotpConnection_getPayload(self->cotpConnection);		// �������� ����� ���������
                            IsoSessionIndication	sIndication = IsoSession_parseMessage(self->session, cotpPayload);	// ������ Session ���������
                            ByteBuffer*				sessionUserData = IsoSession_getUserData(self->session);			// iso8823 OSI Presentation protocol � sessionUserData ��������� �� ������� � ISO 8823

                        	ByteBuffer_wrap(self->cotpConnection->writeBuffer, self->send_buf_2, 0, SEND_BUF_SIZE);
                        	//USART_TRACE("������ ������ writeBuffer ����� ���������� ������� send_buf_2 � ���� ��������:%u\n",SEND_BUF_SIZE);

                            switch (sIndication) {
                            IsoPresentationIndication 	pIndication;

                        		case SESSION_OK:
                        			//USART_TRACE("iso_connection: session ok\n");
                        			break;

                            	case SESSION_CONNECT:
                            		//USART_TRACE("iso_connection: session connect indication\n");
                            		pIndication = IsoPresentation_parseConnect(self->presentation, sessionUserData);	// ������ Presentation ���������
                            		// ����� � self->presentation-nextPayload ������ ������� � ISO 8650-1 (AARP) ��������� ������� �� �������.

                            		if (pIndication == PRESENTATION_OK) {
                                        ByteBuffer* 	acseBuffer;
                                        AcseIndication 	aIndication;
                                        ByteBuffer 		mmsRequest;

                            			//USART_TRACE("iso_connection: presentation ok\n");

                            			acseBuffer = &(self->presentation->nextPayload);								// ���� ��������� �� �������� ISO 8650-1 (AARP) � ������
                            			aIndication = AcseConnection_parseMessage(&acseConnection, acseBuffer);			// ���������� ��������� �� ���������� � acseConnection->userDataBuffer

                            			if (aIndication == ACSE_ASSOCIATE) {
                            				//USART_TRACE("cotp_server: acse associate\n");

                            				ByteBuffer_wrap(&mmsRequest, acseConnection.userDataBuffer,acseConnection.userDataBufferSize, acseConnection.userDataBufferSize);

                            				// �������� ������ Request, ����� ��� ���������������� � ������������ ����� Response
//TODO: ����� ������� ������� Request � ������������ Response.

                            				//MmsServerConnection	mmsServercon;
                            				//iedServer->mmsServer->isoServer->connectionHandler(iedServer->mmsServer->isoServer->connectionHandlerParameter);

                            				//isoConnectionIndicationHandler();

                            				ByteBuffer_setcurrPos(&responseBuffer,0);
                                        	MmsServerConnection_parseMessage(mmsCon,  &mmsRequest, &responseBuffer);

                            				//mmsRequest.buffer[0]++;	//������� response


                            				if (responseBuffer.currPos > 0) {
                            					//USART_TRACE("iso_connection: application payload size: %i\n", responseBuffer.currPos);


                            					// ����� � writeBuffer ��������� ISO 8650-1 (AARQ 24 �����) + ���������� mmsRequest (user infirmation = MMS)
                            					AcseConnection_createAssociateResponseMessage(&acseConnection,ACSE_RESULT_ACCEPT,/*self->cotpConnection->writeBuffer*/&responseFineBuffer, &responseBuffer);
                            					//USART_TRACE("add ISO 8650-1(AARQ) total size: %i\n",/* self->cotpConnection->writeBuffer->currPos*/responseFineBuffer.currPos);

                            					//  ����� � payload ISO 8823 + ���������� writeBuffer (AARQ + user infirmation)
                            					ByteBuffer_setcurrPos(self->cotpConnection->payload,0);
                            					IsoPresentation_createCpaMessage(self->presentation, self->cotpConnection->payload,/*self->cotpConnection->writeBuffer*/&responseFineBuffer);
                            					//USART_TRACE("add ISO 8823(ANS.1) total size: %i\n", self->cotpConnection->payload->currPos);

                            					// � ����� ������� � payload->buffer � ����� ����: ISO 8823 + ISO 8650-1 + MMS
                            					// �� TCP ������ �������� ISO 8327-1 (SPDU), ISO 8073 (COTP), TPKT

                            					ByteBuffer_setcurrPos(/*self->cotpConnection->writeBuffer*/&responseFineBuffer,0);		// ������� ��������� ��� ������������ ������ ������� � ISO 8327-1
                            					ByteBuffer_setcurrPos(&responseBuffer,0);

                            					// ��������� � ������ writeBuffer ����������� AcceptSpdu ( ISO 8327-1 OSI Session protocol )
                            					IsoSession_createAcceptSpdu(self->session,&responseBuffer,self->cotpConnection->payload->currPos);
                            					//USART_TRACE_MAGENTA("create new buffer ISO 8327-1(SPDU) total size: %i\n", responseBuffer.currPos);

                            					// ��������� ��� ������� payload->buffer (ISO 8823 + ISO 8650-1 + MMS)
                            					ByteBuffer_append(&responseBuffer, self->cotpConnection->payload->buffer,self->cotpConnection->payload->currPos);
                            					//USART_TRACE_MAGENTA("add ISO 8823 + ISO 8650-1 + ISO 9506 (MMS) total size: %i\n", responseBuffer.currPos);

                            					// ��������� TPKT � ISO 8073 (COTP) � ���������� �� � ���� TCP/IP
                            					CotpConnection_sendDataMessage(self->cotpConnection, &responseBuffer);
                            					//USART_TRACE_MAGENTA("add TPKT & ISO 8073 (COTP) total size: %i\n",/*self->cotpConnection->writeBuffer->currPos*/responseFineBuffer.currPos);

                            				}
                            				else {
                            					USART_TRACE_RED("iso_connection: association error. No response from application!\n");
                            				}
                            			}
                            			else {
                            				USART_TRACE_RED("iso_connection: acse association failed\n");
                                            self->state = ISO_CON_STATE_STOPPED;
                            			}
                            		}
                            		break;

                            	case SESSION_DATA:
                            		//USART_TRACE("iso_connection: session data indication\n");

                            		pIndication = IsoPresentation_parseUserData(self->presentation, sessionUserData);
                                    if (pIndication == PRESENTATION_ERROR) {
                                        USART_TRACE_RED("cotp_server: presentation error\n");
                                        self->state = ISO_CON_STATE_STOPPED;
                                        break;
                                    }

                                    if (self->presentation->nextContextId == 3) {
                                    	ByteBuffer* mmsRequest;

                                    	USART_TRACE("iso_connection: mms message\n");

                                    	mmsRequest = &(self->presentation->nextPayload);

                        				// �������� ������ Request, ����� ��� ���������������� � ������������ ����� Response
                            			ByteBuffer_setcurrPos(&responseBuffer,0);
                                    	ByteBuffer_setcurrPos(/*self->cotpConnection->writeBuffer*/&responseFineBuffer,0);

                                        MmsServerConnection_parseMessage(mmsCon,  mmsRequest, &responseBuffer);


                                    	// �������� MMS ��������� � ������ writeBuffer � ����������� �� payload
                                    	IsoPresentation_createUserData(self->presentation,/*self->cotpConnection->writeBuffer*/&responseFineBuffer, &responseBuffer);

                                    	ByteBuffer_setcurrPos(&responseBuffer,0);						// ������� ��������� ��� ������������

                                    	// ������� GiveToken Data SPDU � ��������� ����� ��� ��������
                                    	IsoSession_createDataSpdu(self->session, &responseBuffer);
                                    	//USART_TRACE_MAGENTA("create new buffer ISO 8327-1(SPDU) total size: %i\n", responseBuffer.currPos);

                                    	// ��������� ��� ������� writeBuffer->buffer (ISO 8823 + ISO 8650-1 + MMS)
                                    	ByteBuffer_append(&responseBuffer,/*self->cotpConnection->writeBuffer->buffer*/responseFineBuffer.buffer,/*self->cotpConnection->writeBuffer->currPos*/responseFineBuffer.currPos);
                                    	//USART_TRACE_MAGENTA("add ISO 8823 + ISO 8650-1 + ISO 9506 (MMS) total size: %i\n", responseBuffer.currPos);

                                    	ByteBuffer_setcurrPos(/*self->cotpConnection->writeBuffer*/&responseFineBuffer,0);

                    					// ��������� TPKT � ISO 8073 (COTP) � ���������� �� � ���� TCP/IP
                                    	CotpConnection_sendDataMessage(self->cotpConnection, &responseBuffer);
                                    	//USART_TRACE_MAGENTA("add TPKT & ISO 8073 (COTP) total size: %i\n", /*self->cotpConnection->writeBuffer->currPos*/responseFineBuffer.currPos);

                                    }else{
                                    	USART_TRACE_RED(" iso_connection: unknown presentation layer context!\n");
                                    }
                            		break;

                            	case SESSION_GIVE_TOKEN:
                            		USART_TRACE("iso_connection: session give token\n");
                            		USART_TRACE_RED("TODO: iso_connection: session give token\n");
                            		break;

                            	case SESSION_ERROR:
                            		Port_On(LED4);
                            		USART_TRACE_RED("iso_connection: session error\n");
                            		self->state = ISO_CON_STATE_STOPPED;
                            		break;

                            }// switch (sIndication)
                    	Port_Off(LED2);
                        }// DATA_INDICATION
                        break;

                        case ERR:
                        	USART_TRACE_RED("ISO 8073 COTP protocol error\n");
                        	self->state = ISO_CON_STATE_STOPPED;
                        break;

                        default:
                        	USART_TRACE_RED("COTP Unknown Indication: %i\n", cotpIndication);
                            self->state = ISO_CON_STATE_STOPPED;
                        break;
                    }

                    }while (netbuf_next(buf850) >= 0);

                    //free(self->send_buf_1);										// ��������� ������
                    //USART_TRACE_RED("free(self->send_buf_1)\n");

                netbuf_delete(buf850);
                USART_TRACE_GREEN("netbuf_delete ...\n");
                }
        	Port_Off(LED1);
            }
        	USART_TRACE_GREEN("Close netconn connection ...\n");
            netconn_close(newconn850);											// ��������� ����������
            netconn_delete(newconn850);											// ����������� ������
        }
		//! ���� ������ � ���������
		IedServer_destroy(iedServer);
	}
}

/*************************************************************************
 * ByteBuffer_send(ByteBuffer* self)
 * ��������
 *************************************************************************/
int		ByteBuffer_send(ByteBuffer* self)
{

	if (ERR_OK == netconn_write(newconn850, self->buffer, (size_t)self->currPos, NETCONN_COPY))
		return 1;
    else
        return -1;

}

/*************************************************************************
 * Connection_create
 *
 *************************************************************************/
IsoConnection	Connection_create(struct netconn *conn, IsoConnection self, uint8_t* Inbuffer)
{

	self->cotpConnection = calloc(1, sizeof(CotpConnection));
	self->cotpConnection->readBuffer = ByteBuffer_create(NULL,RECEIVE_BUF_SIZE);		// �������� ������ ������ �������� RECEIVE_BUF_SIZE
   	self->cotpConnection->payload = ByteBuffer_create(NULL,SEND_BUF_SIZE);				// �������� ������ ������ �������� RECEIVE_BUF_SIZE

   	self->socket = conn;

    self->session = calloc(1, sizeof(IsoSession));
    IsoSession_init(self->session);

    self->presentation = calloc(1, sizeof(IsoPresentation));
    IsoPresentation_init(self->presentation);

	self->receive_buf = Inbuffer;

	self->send_buf_1 = malloc(SEND_BUF_SIZE);
	USART_TRACE("������� ������ ��� send_buf_1 = %u \n",SEND_BUF_SIZE);

	self->send_buf_2 = malloc(SEND_BUF_SIZE);
	USART_TRACE("������� ������ ��� send_buf_2 = %u \n",SEND_BUF_SIZE);

	self->msgRcvdHandler = NULL;
	self->msgRcvdHandlerParameter = NULL;

	self->state = ISO_CON_STATE_RUNNING;
	self->clientAddress = Socket_getPeerAddress(conn);							// ����� �������

	USART_TRACE("���������������� IsoConnection\n");

	return self;
}
/*************************************************************************
 * Socket_getPeerAddress
 *
 *************************************************************************/
char*	Socket_getPeerAddress(struct netconn *conn)
{
    //struct sockaddr_storage addr;
    ip_addr_t 	addr;
    uint16_t	port;
    uint8_t		ip0,ip1,ip2,ip3;

    char* clientConnection = malloc(24);

    netconn_peer(conn,&addr,&port);

    ip0 = addr.addr & 0xff;
    ip1 = (addr.addr>>8) & 0xff;
    ip2 = (addr.addr>>16) & 0xff;
    ip3 = (addr.addr>>24) & 0xff;

    sprintf(clientConnection, "%d.%d.%d.%d[%i]",ip0, ip1, ip2, ip3, port);
    USART_TRACE("PeerAddress: IP:%d.%d.%d.%d[%i]\n",ip0, ip1, ip2, ip3,port);

   return clientConnection;
}
/*************************************************************************
 * observerCallback
 * ������ �������� �� ����������
 * ���� ������ ������� �� ������� �� ������/��������� ��������
 * �� �������� �������
 *************************************************************************/
void	observerCallback(DataAttribute* dataAttribute){
    if (dataAttribute == IEDMODEL_GenericIO_GGIO1_NamPlt_vendor) {
    	USART_TRACE("GGIO.NamPlt.vendor changed to %s\n", MmsValue_toString(dataAttribute->mmsValue));
    }
    else if (dataAttribute == IEDMODEL_GenericIO_GGIO1_NamPlt_swRev) {
    	USART_TRACE("GGIO.NamPlt.swRef changed to %s\n", MmsValue_toString(dataAttribute->mmsValue));
    }
}
/*************************************************************************
 * controlListener
 *************************************************************************/
void	controlListener(void* parameter, MmsValue* value)
{

	 if (MmsValue_getType(value) == MMS_BOOLEAN) {
	        printf("������� ������� �������� MMS_BOOLEAN: ");

	        if (MmsValue_getBoolean(value) == MMS_BOOLEAN)
	            printf("on\n");
	        else
	            printf("off\n");
	    }
	 else{
	        printf("������� �� MMS_BOOLEAN\n");
	 }

	    uint64_t timeStamp = Hal_getTimeInMs();

    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1){
        IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timeStamp);
        IedServer_updateAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, value);
    }

}

static void	updateLED4stVal(bool newLedState, uint64_t timeStamp) {

	if (newLedState) Port_On(LED4); else Port_Off(LED4);

    IedServer_updateUTCTimeAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_t, timeStamp);
    IedServer_updateBooleanAttributeValue(iedServer, IEDMODEL_GenericIO_GGIO1_SPCSO1_stVal, newLedState);
}

/*************************************************************************
 * controlHandlerForBinaryOutput
 *************************************************************************/
void	controlHandlerForBinaryOutput(void* parameter, MmsValue* value, bool test)
{

	 if (MmsValue_getType(value) == MMS_BOOLEAN) {
	        printf("������� ������� �������� MMS_BOOLEAN: ");

	        if (MmsValue_getBoolean(value) == MMS_BOOLEAN)
	            printf("on\n");
	        else
	            printf("off\n");
	    }
	 else{
	        printf("������� �� MMS_BOOLEAN\n");
	 }

	    uint64_t timeStamp = Hal_getTimeInMs();

	    bool newState = MmsValue_getBoolean(value);

	    if (parameter == IEDMODEL_GenericIO_GGIO1_SPCSO1)	updateLED4stVal(newState, timeStamp);

}

/*************************************************************************
 * Hal_getTimeInMs
 *
 *************************************************************************/
void sigint_handler(int signalId)
{

}
/*************************************************************************
 * Hal_getTimeInMs
 * �������� �� ����� ����� � ms �� ��������� 70-�� ����.
 * ��� ����� ������������.... ������ ������� .
 *************************************************************************/
uint64_t 	Hal_getTimeInMs (void){

	RTC_TimeTypeDef sTime;
	RTC_DateTypeDef sDate;
	uint64_t	sectmp,den;

	HAL_RTC_GetTime(&hrtc, &sTime, FORMAT_BIN);			// ������ �����
	HAL_RTC_GetDate(&hrtc, &sDate, FORMAT_BIN);			// ������ ����

	den 	= (30+sDate.Year) * 365 + (30+sDate.Year+2)/4 - 2;

	switch(sDate.Month) {
	    case 2 : den +=31; break;	//31	31
	    case 3 : den +=59; break;	//59	28
	    case 4 : den +=90; break;	//90	31
	    case 5 : den +=120; break;	//120	30
	    case 6 : den +=151; break;	//151	31
	    case 7 : den +=181; break;	//181	30
	    case 8 : den +=212; break;	//212	31
	    case 9 : den +=243; break;	//243	31
	    case 10 : den +=273; break;	//273	30
	    case 11 : den +=304; break;	//304	31
	    case 12 : den +=334; break;	//334	30
	   };

	if (!((30+sDate.Year+2)%4) && sDate.Month>2) den++;

	 den += sDate.Date;

	 sectmp = den * 86400;
	 sectmp += sTime.Hours * 3600;
	 sectmp += sTime.Minutes * 60;
	 sectmp += sTime.Seconds;

	 sectmp -= Timezone*60*60;			//������� ������� ����, ��� ��������� �������� UTC ���������.

	 sectmp *= 1000;
	 sectmp += sTime.SubSeconds/10;

	return sectmp;
}
