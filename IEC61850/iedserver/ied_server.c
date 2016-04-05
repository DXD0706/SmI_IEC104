/*
 *  ied_server.c
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

#include "main.h"
#include "iec850.h"

#include "iec61850_server.h"
#include "mms_mapping.h"
#include "control.h"
#include "stack_config.h"

#include "mms_server.h"

struct sIedServer {
	IedModel* 		model;				// ������ IED (intellegent electronic device) ����������� ����������
	MmsDevice* 		mmsDevice;			// MMS ����������.
	MmsServer 		mmsServer;			// MMS ������, ����������, TCP ���������, ��������� �� ������� ������ � ������ ����������.
	IsoServer 		isoServer;			//  ������ ������� ISO, ��������� �����������, ��������� �� ������� �������� �������.
	MmsMapping* 	mmsMapping;
};

/*************************************************************************
 * checkForUpdateTrigger
 *  ���� ������� ������ �������, �� MmsMapping->reportControls->data ������� ����
 *  �� ���������� ������. REPORT_CONTROL_VALUE_UPDATE
 *************************************************************************/
static inline void	checkForUpdateTrigger(IedServer self, DataAttribute* dataAttribute)
{
#if (CONFIG_IEC61850_REPORT_SERVICE== 1)
    if (dataAttribute->triggerOptions & TRG_OPT_DATA_UPDATE) {
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_VALUE_UPDATE);
    }
#endif
}
/*************************************************************************
 * checkForChangedTriggers
 *  ���� ������� ������ �������, �� MmsMapping->reportControls->data ������� ����
 *  � ��������� ������. REPORT_CONTROL_VALUE_CHANGED
 *************************************************************************/
static inline void	checkForChangedTriggers(IedServer self, DataAttribute* dataAttribute)
{
#if (CONFIG_IEC61850_REPORT_SERVICE == 1) || (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
    if (dataAttribute->triggerOptions & TRG_OPT_DATA_CHANGED) {

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);
#endif

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_VALUE_CHANGED);
#endif
    }

    else if (dataAttribute->triggerOptions & TRG_OPT_QUALITY_CHANGED) {

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);
#endif

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_QUALITY_CHANGED);
#endif
    }
#endif /* (CONFIG_IEC61850_REPORT_SERVICE== 1) || (CONFIG_INCLUDE_GOOSE_SUPPORT == 1) */


}
/*************************************************************************
 * createControlObjects
 *
 *************************************************************************/
static void		createControlObjects(IedServer self, MmsDomain* domain, char* lnName, MmsTypeSpecification* typeSpec)
{
    MmsMapping* mapping = self->mmsMapping;

    if (typeSpec->type == MMS_STRUCTURE) {
        int coCount = typeSpec->typeSpec.structure.elementCount;
        int i;
        for (i = 0; i < coCount; i++) {
            MmsTypeSpecification* coSpec = typeSpec->typeSpec.structure.elements[i];

            int coElementCount = coSpec->typeSpec.structure.elementCount;

            MmsValue* operVal = NULL;
            MmsValue* sboVal = NULL;
            MmsValue* sbowVal = NULL;
            MmsValue* cancelVal = NULL;

            ControlObject* controlObject = ControlObject_create(self->mmsServer, domain, lnName, coSpec->name);

            MmsValue* structure = MmsValue_newDefaultValue(coSpec);

            ControlObject_setMmsValue(controlObject, structure);

            int j;
            for (j = 0; j < coElementCount; j++) {
                MmsTypeSpecification*  coElementSpec = coSpec->typeSpec.structure.elements[j];

                if (strcmp(coElementSpec->name, "Oper") == 0) {
                    operVal = MmsValue_getElement(structure, j);

                    ControlObject_setOper(controlObject, operVal);
                }
                else if (strcmp(coElementSpec->name, "Cancel") == 0) {
                    cancelVal = MmsValue_getElement(structure, j);
                    ControlObject_setCancel(controlObject, cancelVal);
                }
                else if (strcmp(coElementSpec->name, "SBO") == 0) {
                    sboVal = MmsValue_getElement(structure, j);
                    ControlObject_setSBO(controlObject, sboVal);
                }
                else if (strcmp(coElementSpec->name, "SBOw") == 0) {
                    sbowVal = MmsValue_getElement(structure, j);
                    ControlObject_setSBOw(controlObject, sbowVal);
                }
                else {
                	USART_TRACE_RED("createControlObjects: Unknown element in CO!\n");
                }
            }

            MmsMapping_addControlObject(mapping, controlObject);
        }
    }
}

/*************************************************************************
 * createMmsServerCache
 * ����������� ��� MMSnamedvariables �������� ������ � ��� MMS �������
 *************************************************************************/
static void		createMmsServerCache(IedServer self)
{

	int domain = 0;

	for (domain = 0; domain < self->mmsDevice->domainCount; domain++) {

		/* Install all top level MMS named variables (=Logical nodes) in the MMS server cache */
		MmsDomain* logicalDevice = self->mmsDevice->domains[domain];

		int i;

		for (i = 0; i < logicalDevice->namedVariablesCount; i++) {
			char* lnName = logicalDevice->namedVariables[i]->name;

			USART_TRACE("������� � ��� ������� %s - %s\n", logicalDevice->domainName, lnName);

			int fcCount = logicalDevice->namedVariables[i]->typeSpec.structure.elementCount;
			int j;

			for (j = 0; j < fcCount; j++) {
				MmsTypeSpecification* fcSpec = logicalDevice->namedVariables[i]->typeSpec.structure.elements[j];

				char* fcName = fcSpec->name;

				if (strcmp(fcName, "CO") == 0) {
				    int controlCount = fcSpec->typeSpec.structure.elementCount;

				    createControlObjects(self, logicalDevice, lnName, fcSpec);
				}
				else if ((strcmp(fcName, "BR") != 0) && (strcmp(fcName, "RP") != 0) && (strcmp(fcName, "GO") != 0))
				{

					char* variableName = createString(3, lnName, "$", fcName);

					MmsValue* defaultValue = MmsValue_newDefaultValue(fcSpec);

					USART_TRACE("		IedServer->mmsServer->valueCaches = %s - %s\n",logicalDevice->domainName,  variableName);

					MmsServer_insertIntoCache(self->mmsServer, logicalDevice, variableName, defaultValue);

					free(variableName);
				}
			}// !for (j = 0; j < fcCount; j++) {
		}
	}
}
/*************************************************************************
 * installDefaultValuesForDataAttribute
 *
 *************************************************************************/
static void		installDefaultValuesForDataAttribute(IedServer self, DataAttribute* dataAttribute, char* objectReference, int position)
{
	sprintf(objectReference + position, ".%s", dataAttribute->name);

	char* mmsVariableName[255]; //TODO check for optimal size

	MmsValue* value = dataAttribute->mmsValue;

	MmsMapping_createMmsVariableNameFromObjectReference(objectReference, dataAttribute->fc, mmsVariableName);

	char* domainName[100]; //TODO check for optimal size

	MmsMapping_getMmsDomainFromObjectReference(objectReference, domainName);

	MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, domainName);

	if (domain == NULL) {
		printf("Error domain (%s) not found!\n", domainName);
		return;
	}

	MmsValue* cacheValue = MmsServer_getValueFromCache(self->mmsServer, domain, mmsVariableName);

	dataAttribute->mmsValue = cacheValue;

	if (value != NULL) {
		MmsValue_update(cacheValue, value);
		MmsValue_delete(value);
	}

	int childPosition = strlen(objectReference);
	DataAttribute* subDataAttribute = dataAttribute->firstChild;

	while (subDataAttribute != NULL) {
		installDefaultValuesForDataAttribute(self, subDataAttribute, objectReference, childPosition);

		USART_TRACE("					�������: %s \n", objectReference);

		subDataAttribute = subDataAttribute->sibling;
	}
}

/*************************************************************************
 * installDefaultValuesForDataObject
 *
 *************************************************************************/
static void		installDefaultValuesForDataObject(IedServer self, DataObject* dataObject, char* objectReference, int position)
{

	sprintf(objectReference + position, ".%s", dataObject->name);

	ModelNode* childNode = dataObject->firstChild;

	int childPosition = strlen(objectReference);

	while (childNode != NULL) {
		if (childNode->modelType == DataObjectModelType) {
			installDefaultValuesForDataObject(self, childNode, objectReference, childPosition);
		}
		else if (childNode->modelType == DataAttributeModelType) {
			installDefaultValuesForDataAttribute(self, childNode, objectReference, childPosition);
		}
		USART_TRACE("				������: %s \n", objectReference);
		childNode = childNode->sibling;
	}
}
/*************************************************************************
 * installDefaultValuesInCache
 *
 *************************************************************************/
static void	installDefaultValuesInCache(IedServer self)
{
	IedModel* model = self->model;												// ������ IED � ������� IedServer

	USART_TRACE("\n");
	USART_TRACE(" ------------- installDefaultValuesInCache --------------- \n");
	USART_TRACE(" IedServer->model-> \n");
	USART_TRACE("������ IED: %s \n", model->name);

	char objectReference[255]; // TODO check for optimal buffer size;

	LogicalDevice* logicalDevice = model->firstChild;


	while (logicalDevice != NULL) {
		sprintf(objectReference, "%s", logicalDevice->name);

		USART_TRACE("	���������� ���������� LD: %s \n", objectReference);
		LogicalNode* logicalNode = logicalDevice->firstChild;

		char* nodeReference = objectReference + strlen(objectReference);

		while (logicalNode != NULL) {
			sprintf(nodeReference, "/%s", logicalNode->name);

			USART_TRACE("		���������� ���� LN: %s \n", nodeReference);
			DataObject* dataObject = logicalNode->firstChild;

			int refPosition = strlen(objectReference);

			while (dataObject != NULL) {
				USART_TRACE("			������ ������: %s \n", dataObject->name);
				installDefaultValuesForDataObject(self, dataObject, objectReference, refPosition);

				dataObject = dataObject->sibling;
			}

			logicalNode = logicalNode->sibling;
		}

		logicalDevice = logicalDevice->sibling;
	}
}

/*************************************************************************
 * updateDataSetsWithCachedValues
 * ������� ������ �������� ������ ��������, ���
 *************************************************************************/
static void		updateDataSetsWithCachedValues(IedServer self)
{
	DataSet** dataSets = self->model->dataSets;

	USART_TRACE("\n");
	USART_TRACE(" ------------- updateDataSetsWithCachedValues --------------- \n");
	int i = 0;
	while (dataSets[i] != NULL) {


		int fcdaCount = dataSets[i]->elementCount;
		USART_TRACE("\n");
		USART_TRACE("dataSets[%u]->elementCount: %u \n",i,fcdaCount);

		int j = 0;

		for (j = 0; j < fcdaCount; j++) {
			USART_TRACE("\n");
			USART_TRACE("dataSets[%u]->fcda[%u]->logicalDeviceName: %s \n",i,j, dataSets[i]->fcda[j]->logicalDeviceName);
			USART_TRACE("dataSets[%u]->fcda[%u]->variableName: %s \n",i,j, dataSets[i]->fcda[j]->variableName);

			MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, dataSets[i]->fcda[j]->logicalDeviceName);

			MmsValue* value = MmsServer_getValueFromCache(self->mmsServer, domain, dataSets[i]->fcda[j]->variableName);

			USART_TRACE("dataSets[%u]->fcda[%u]->value->type: %u \n",i,j,value->type);
			USART_TRACE("dataSets[%u]->fcda[%u]->value->value: %u \n",i,j,value->value);

			if (value == NULL)
				printf("error cannot get value from cache!\n");
			else
				dataSets[i]->fcda[j]->value = value;

		}

		i++;
	}
USART_TRACE("!updateDataSetsWithCachedValues -------------- \n");

}
/*************************************************************************
 * IedServer_create
 * �������� IED ����������� ���������� IedServer
 *
 * self->mmsMapping
 * self->mmsDevice
 * self->isoServer
 * self->mmsServer
 *
 *************************************************************************/
IedServer	IedServer_create(IedModel* iedModel)
{
	IedServer self = calloc(1, sizeof(struct sIedServer));
//	IedServer self = pvPortMalloc(sizeof(struct sIedServer));

//	USART_TRACE("\n");
//	USART_TRACE("--------------------------------------------------------------------\n");
//	USART_TRACE("�������� ������ ��� ��������� IedServer ������� �� ������:0x%X �������� %u\n",&self,sizeof(struct sIedServer));

	self->model 		= iedModel;														// ������ IED
	self->mmsMapping 	= MmsMapping_create(iedModel);									//
	self->mmsDevice 	= MmsMapping_getMmsDeviceModel(self->mmsMapping);				//
	self->isoServer 	= IsoServer_create();											// ��������� ISO_SVR_STATE_IDLE � ������ ����� �����
	self->mmsServer 	= MmsServer_create(self->isoServer, self->mmsDevice);			//

	MmsMapping_setMmsServer(self->mmsMapping, self->mmsServer);							// self->mmsMapping->mmsServer = self->mmsServer;
	MmsMapping_installHandlers(self->mmsMapping);										// �������� ������� ������ � ������ ����������.

	//MmsMapping_setIedServer(self->mmsMapping, self);

	USART_TRACE("-> IedServer_create -> createMmsServerCache(self);\n");
	createMmsServerCache(self);

	USART_TRACE("-> IedServer_create -> iedModel->initializer();   ������� ����������(���������) � ��� ��. static_model.c \n");
	iedModel->initializer();														// ������� ���������� � ���. static_model.c

	installDefaultValuesInCache(self); 												// This will also connect cached MmsValues to DataAttributes

	updateDataSetsWithCachedValues(self);
//	USART_TRACE("updateDataSetsWithCachedValues(self);\n");

	USART_TRACE("IedServer_create\n");
	USART_TRACE("--------------------------------------------------------------------\n");
	return self;
}
/*************************************************************************
 * IedServer_destroy
 * ��������� IED ����������� ����������
 *************************************************************************/
void	IedServer_destroy(IedServer self)
{
	MmsServer_destroy(self->mmsServer);
	IsoServer_destroy(self->isoServer);
	MmsMapping_destroy(self->mmsMapping);
	free(self);
}
/*************************************************************************
 * IedServer_getMmsServer
 * ������� MmsServer �� IED �������
 *************************************************************************/
MmsServer	IedServer_getMmsServer(IedServer self)
{
	return self->mmsServer;
}
/*************************************************************************
 * IedServer_getIsoServer
 * ������� IsoServer �� IED �������
 *************************************************************************/
IsoServer	IedServer_getIsoServer(IedServer self)
{
	return self->isoServer;
}
/*************************************************************************
 * IedServer_start
 * ����� IED �������
 *************************************************************************/
//TODO: �� �������� Thread
void	IedServer_start(IedServer self, int tcpPort)
{
	MmsServer_startListening(self->mmsServer, tcpPort);
	MmsMapping_startEventWorkerThread(self->mmsMapping);
}

/*************************************************************************
 * IedServer_isRunning
 * ������ �������� �� IED ������
 *************************************************************************/
bool	IedServer_isRunning(IedServer self)
{
	if (IsoServer_getState(self->isoServer) == ISO_SVR_STATE_RUNNING)
		return true;
	else
		return false;
}

/*************************************************************************
 * IedServer_stop
 * ��������� IED ������
 *************************************************************************/
void	IedServer_stop(IedServer self)
{
	MmsServer_stopListening(self->mmsServer);
}
/*************************************************************************
 * IedServer_lockDataModel
 * ����������� ���������� mmsServer'�� (����� ��������)
 *************************************************************************/
void	IedServer_lockDataModel(IedServer self)
{
	MmsServer_lockModel(self->mmsServer);
}
/*************************************************************************
 * IedServer_lockDataModel
 * ����� ���������� mmsServer'�� (����� ��������)
 *************************************************************************/
void	IedServer_unlockDataModel(IedServer self)
{
	MmsServer_unlockModel(self->mmsServer);
}

/*************************************************************************
 * IedServer_getDomain
 * ���� � mmsDevice ���������� ���������� (logicalDeviceName) �������
 * � ���������� ��������� MMS ������������ ����� �����. � ����������
 * ��������� �� ���� �����. self->mmsDevice->domains[i]
 *************************************************************************/
MmsDomain*	IedServer_getDomain(IedServer self, char* logicalDeviceName)
{
	return MmsDevice_getDomain(self->mmsDevice, logicalDeviceName);
}
/*************************************************************************
 * IedServer_getValue
 *
 *************************************************************************/
MmsValue*	IedServer_getValue(IedServer self, MmsDomain* domain, char* mmsItemId)
{
	return MmsServer_getValueFromCache(self->mmsServer, domain, mmsItemId);
}
/*************************************************************************
 * IedServer_setControlHandler
 * ���������� ���������� ���������� ��� ��������������� ������� ������
 *
 *************************************************************************/
void	IedServer_setControlHandler(IedServer self, DataObject* node, ControlHandler listener, void* parameter)
{

    char* objectReference[129];

    ModelNode_getObjectReference(node, objectReference);

    char* separator = strchr(objectReference, '/');

    *separator = 0;

    MmsDomain* domain = MmsDevice_getDomain(self->mmsDevice, objectReference);

    char* lnName = separator + 1;

    separator = strchr(lnName, '.');

    *separator = 0;

    char* objectName = separator + 1;

    separator = strchr(objectName, '.');

    if (separator != NULL)
        *separator = 0;

    ControlObject* controlObject = MmsMapping_getControlObject(self->mmsMapping, domain, lnName, objectName);

    if (controlObject != NULL)	ControlObject_installListener(controlObject, listener, parameter);
}

MmsValue*	IedServer_getAttributeValue(IedServer self, DataAttribute* dataAttribute)
{
    return dataAttribute->mmsValue;
}

/*************************************************************************
 * IedServer_updateBooleanAttributeValue
 * ���������� �������� (bool)value � dataAttribute
 *************************************************************************/
void	IedServer_updateBooleanAttributeValue(IedServer self, DataAttribute* dataAttribute, bool value)
{
    bool currentValue = MmsValue_getBoolean(dataAttribute->mmsValue);		// ������ ������� �������� �� ���������

    if (currentValue == value) {											// ���� �������� ����� ��� � ����, �� ������� ���� � ��������� ������� �� ����������.
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {																	// ���� �������� ����������, �� ��������� ����� ��������.
        MmsValue_setBoolean(dataAttribute->mmsValue, value);
        checkForChangedTriggers(self, dataAttribute);						// ������� ���� � ��������� ������� �� ��������� ������.
    }
}
/*************************************************************************
 * IedServer_updateAttributeValue
 * ���������� �������� (MmsValue*)value � DataAttribute
 *************************************************************************/
void	IedServer_updateAttributeValue(IedServer self, DataAttribute* node, MmsValue* value)
{
	if (node->modelType == DataAttributeModelType) {
		DataAttribute* dataAttribute = (DataAttribute*) node;

		if (MmsValue_isEqual(dataAttribute->mmsValue, value)) {
		    MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_VALUE_UPDATE);
		}
		else {
		    MmsValue_update(dataAttribute->mmsValue, value);
		    MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_VALUE_CHANGED);

		    if (CONFIG_INCLUDE_GOOSE_SUPPORT)
		        MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);

		}
	}
}

void
IedServer_attributeQualityChanged(IedServer self, ModelNode* node)
{
    if (node->modelType == DataAttributeModelType) {
        DataAttribute* dataAttribute = (DataAttribute*) node;

        MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue, REPORT_CONTROL_QUALITY_CHANGED);
    }
}

void
IedServer_enableGoosePublishing(IedServer self)
{
    MmsMapping_enableGoosePublishing(self->mmsMapping);
}


/*************************************************************************
 * IedServer_observeDataAttribute
 * ���������� ����������� ��� �������� ������
 * dataAttribute 	- ������� ��� ����������.
 * handler			- ������ ������� ��� ������ ��� ��������� ��������
 *************************************************************************/
void	IedServer_observeDataAttribute(IedServer self, DataAttribute* dataAttribute, AttributeChangedHandler handler)
{
    MmsMapping_addObservedAttribute(self->mmsMapping, dataAttribute, handler);
}

/*************************************************************************
 * IedServer_updateUTCTimeAttributeValue
 * ��������� ����� �������� (UtcTimeMs)value � ������ dataAttribute->mmsValue
 *************************************************************************/
void	IedServer_updateUTCTimeAttributeValue(IedServer self, DataAttribute* dataAttribute, uint64_t value)
{
    uint64_t currentValue = MmsValue_getUtcTimeInMs(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setUtcTimeMs(dataAttribute->mmsValue, value);
        checkForChangedTriggers(self, dataAttribute);
    }
}

/*************************************************************************
 * IedServer_updateFloatAttributeValue
 * ��������� ����� �������� (float)value � ������ dataAttribute->mmsValue
 *************************************************************************/
void	IedServer_updateFloatAttributeValue(IedServer self, DataAttribute* dataAttribute, float value)
{
    float currentValue = MmsValue_toFloat(dataAttribute->mmsValue);

    if (currentValue == value) {
        checkForUpdateTrigger(self, dataAttribute);
    }
    else {
        MmsValue_setFloat(dataAttribute->mmsValue, value);
        checkForChangedTriggers(self, dataAttribute);
    }
}
/*************************************************************************
 * IedServer_updateQuality
 * ��������� ����� �������� ��������
 *************************************************************************/
void	IedServer_updateQuality(IedServer self, DataAttribute* dataAttribute, Quality quality)
{
    uint32_t oldQuality = MmsValue_getBitStringAsInteger(dataAttribute->mmsValue);

    if (oldQuality != (uint32_t) quality) {
        MmsValue_setBitStringFromInteger(dataAttribute->mmsValue, (uint32_t) quality);

#if (CONFIG_INCLUDE_GOOSE_SUPPORT == 1)
        MmsMapping_triggerGooseObservers(self->mmsMapping, dataAttribute->mmsValue);
#endif

#if (CONFIG_IEC61850_REPORT_SERVICE == 1)
        if (dataAttribute->triggerOptions & TRG_OPT_QUALITY_CHANGED)
            MmsMapping_triggerReportObservers(self->mmsMapping, dataAttribute->mmsValue,
                                REPORT_CONTROL_QUALITY_CHANGED);
#endif
    }


}
