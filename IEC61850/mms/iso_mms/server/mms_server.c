/*
 *  mms_server.c
 *
 *  Copyright 2013 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#include "mms_server.h"
#include "mms_server_connection.h"
#include "mms_value_cache.h"
#include "map.h"
#include "thread.h"

#include "main.h"

struct sMmsServer {
    IsoServer 				isoServer;							// ��������� �������, ��������� ������, TCP ����, ����.....
    MmsDevice* 				device;								// MMS ����������, � ����������
    ReadVariableHandler 	readHandler;						// ������� ������ ����������
    void* 					readHandlerParameter;
    WriteVariableHandler 	writeHandler;						// ������� ������ ����������
    void* 					writeHandlerParameter;
    MmsConnectionHandler 	connectionHandler;
    void* 					connectionHandlerParameter;
    Map 					openConnections;
    Map 					valueCaches;
    bool 					isLocked;
    Semaphore 				modelMutex;							// ������� ��� �������� ���������� MmsServer'��
};

/*************************************************************************
 * createValueCachesForDomains
 * ��������
 *************************************************************************/
static Map		createValueCachesForDomains(MmsDevice* device)
{
    Map valueCaches = Map_create();

    int i;
    for (i = 0; i < device->domainCount; i++) {
        MmsValueCache valueCache = MmsValueCache_create(device->domains[i]);
        Map_addEntry(valueCaches, device->domains[i], valueCache);
    }

    return valueCaches;
}
/*************************************************************************
 * MmsServer_create
 * ��������
 *************************************************************************/
MmsServer	MmsServer_create(IsoServer isoServer, MmsDevice* device)
{
    MmsServer self = calloc(1, sizeof(struct sMmsServer));
	USART_TRACE("IedServer->mmsServer  - �������� ������ ��� ��������� �� ������:0x%X ��������:%u\n",&self,sizeof(struct sMmsServer));

    self->isoServer = isoServer;
    self->device = device;

    self->openConnections = Map_create();
    self->valueCaches = createValueCachesForDomains(device);
    self->isLocked = false;
//TODO Semaphore_create ������
    self->modelMutex = Semaphore_create(1);

    return self;
}

/*************************************************************************
 * MmsServer_lockModel
 * ����������� ���������� ������� (�������� �������)
 *************************************************************************/
void	MmsServer_lockModel(MmsServer self)
{
    Semaphore_wait(self->modelMutex);
}
/*************************************************************************
 * MmsServer_unlockModel
 * ����� ���������� ������� (����� �������)
 *************************************************************************/
void	MmsServer_unlockModel(MmsServer self)
{
    Semaphore_post(self->modelMutex);
}

void
MmsServer_installReadHandler(MmsServer self, ReadVariableHandler readHandler, void* parameter)
{
    self->readHandler = readHandler;
    self->readHandlerParameter = parameter;
}

void
MmsServer_installWriteHandler(MmsServer self, WriteVariableHandler writeHandler, void* parameter)
{
    self->writeHandler = writeHandler;
    self->writeHandlerParameter = parameter;
}

void
MmsServer_installConnectionHandler(MmsServer self, MmsConnectionHandler connectionHandler, void* parameter)
{
    self->connectionHandler = connectionHandler;
    self->connectionHandlerParameter = parameter;
}


static void
closeConnection(void* con)
{
    MmsServerConnection* connection = (MmsServerConnection*) con;

    MmsServerConnection_destroy(connection);
}

static void
deleteSingleCache(MmsValueCache cache)
{
    MmsValueCache_destroy(cache);
}

void
MmsServer_destroy(MmsServer self)
{
    Map_deleteDeep(self->openConnections, false, closeConnection);
    Map_deleteDeep(self->valueCaches, false, deleteSingleCache);
    Semaphore_destroy(self->modelMutex);
    free(self);
}

/*************************************************************************
 * MmsServer_getValueFromCache
 * ������ ��������� �� ��� � ���� ��������� �� MmsValue*
 *************************************************************************/
MmsValue*	MmsServer_getValueFromCache(MmsServer self, MmsDomain* domain, char* itemId)
{
    MmsValueCache cache = Map_getEntry(self->valueCaches, domain);

    if (cache != NULL) {
 //       USART_TRACE_GREEN("		MmsServer_getValueFromCache();\n");
        return MmsValueCache_lookupValue(cache, itemId);
    } else{
        USART_TRACE_RED("		MmsServer_getValueFromCache(); = NULL\n");
    }

    return NULL ;
}
/*************************************************************************
 * MmsServer_insertIntoCache
 * ������� ���������� � ���
 *************************************************************************/
void		MmsServer_insertIntoCache(MmsServer self, MmsDomain* domain, char* itemId, MmsValue* value)
{
    MmsValueCache cache = Map_getEntry(self->valueCaches, domain);

    if (cache != NULL) {
  //      USART_TRACE_GREEN("		MmsValueCache_insertValue();\n");
        MmsValueCache_insertValue(cache, itemId, value);
    } else{
    	USART_TRACE_RED("		MmsValueCache_insertValue(); = NULL\n");
    }
}

MmsValueIndication
mmsServer_setValue(MmsServer self, MmsDomain* domain, char* itemId, MmsValue* value,
        MmsServerConnection* connection)
{
    MmsValueIndication indication;

    if (self->writeHandler != NULL) {
        indication = self->writeHandler(self->writeHandlerParameter, domain,
                itemId, value, connection);
    } else {
        MmsValue* cachedValue;

        cachedValue = MmsServer_getValueFromCache(self, domain, itemId);

        if (cachedValue != NULL) {
            MmsValue_update(cachedValue, value);
            indication = MMS_VALUE_OK;
        } else
            indication = MMS_VALUE_ACCESS_DENIED;
    }

    return indication;
}

MmsValue*
mmsServer_getValue(MmsServer self, MmsDomain* domain, char* itemId)
{
    MmsValue* value = NULL;

    value = MmsServer_getValueFromCache(self, domain, itemId);

    if (value == NULL)
        if (self->readHandler != NULL)
            value = self->readHandler(self->readHandlerParameter, domain,
                    itemId);

    return value;
}


MmsDevice*
MmsServer_getDevice(MmsServer self)
{
    return self->device;
}

void
MmsServer_setDevice(MmsServer server, MmsDevice* device)
{
    server->device = device;
}

static void /* will be called by ISO server stack */
isoConnectionIndicationHandler(IsoConnectionIndication indication, void* parameter, IsoConnection connection)
{
    MmsServer mmsServer = (MmsServer) parameter;

    if (indication == ISO_CONNECTION_OPENED) {
        MmsServerConnection* mmsCon = MmsServerConnection_init(0, mmsServer, connection);
        Map_addEntry(mmsServer->openConnections, connection, mmsCon);

        if (mmsServer->connectionHandler != NULL)
            mmsServer->connectionHandler(mmsServer->connectionHandlerParameter, mmsCon, MMS_SERVER_NEW_CONNECTION);

    } else if (indication == ISO_CONNECTION_CLOSED) {
        MmsServerConnection* mmsCon = (MmsServerConnection*) Map_removeEntry( mmsServer->openConnections, connection, false);

        if (mmsServer->connectionHandler != NULL)
            mmsServer->connectionHandler(mmsServer->connectionHandlerParameter, mmsCon, MMS_SERVER_CONNECTION_CLOSED);

        if (mmsCon != NULL)
            MmsServerConnection_destroy(mmsCon);
    }
}

/*************************************************************************
 * MmsServer_startListening
 *
 *************************************************************************/
void	MmsServer_startListening(MmsServer server, int tcpPort)
{
	// �������� �������
    IsoServer_setConnectionHandler(server->isoServer, isoConnectionIndicationHandler, (void*) server);
    // �������� ���� TCP
    IsoServer_setTcpPort(server->isoServer, tcpPort);		// �������� ���� ISO �������
//TODO: IsoServer_startListening �� �������
  //  IsoServer_startListening(server->isoServer);
}

void
MmsServer_stopListening(MmsServer server)
{
    IsoServer_stopListening(server->isoServer);
}
