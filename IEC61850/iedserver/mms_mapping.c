/*
 *  mms_mapping.c
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

#include "libiec61850_platform_includes.h"
#include "mms_mapping.h"
#include "mms_mapping_internal.h"
#include "array_list.h"
#include "stack_config.h"

#include "mms_goose.h"
#include "reporting.h"

#include "main.h"

typedef struct sAttributeObserver {
    DataAttribute* attribute;
    void (*handler) (DataAttribute* dataAttribute);
} AttributeObserver;

void /* Create PHYCOMADDR ACSI type instance */
MmsMapping_createPhyComAddrStructure(MmsTypeSpecification* namedVariable)
{
    namedVariable->type = MMS_STRUCTURE;
    namedVariable->typeSpec.structure.elementCount = 4;
    namedVariable->typeSpec.structure.elements = calloc(4,
            sizeof(MmsTypeSpecification*));

    MmsTypeSpecification* element;

    element = calloc(1, sizeof(MmsTypeSpecification));
    element->name = copyString("Addr");
    element->type = MMS_OCTET_STRING;
    element->typeSpec.octetString = 6;
    namedVariable->typeSpec.structure.elements[0] = element;

    element = calloc(1, sizeof(MmsTypeSpecification));
    element->name = copyString("PRIORITY");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 8;
    namedVariable->typeSpec.structure.elements[1] = element;

    element = calloc(1, sizeof(MmsTypeSpecification));
    element->name = copyString("VID");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 16;
    namedVariable->typeSpec.structure.elements[2] = element;

    element = calloc(1, sizeof(MmsTypeSpecification));
    element->name = copyString("APPID");
    element->type = MMS_UNSIGNED;
    element->typeSpec.unsignedInteger = 16;
    namedVariable->typeSpec.structure.elements[3] = element;
}

static MmsTypeSpecification*
createNamedVariableFromDataAttribute(DataAttribute* attribute)
{
    MmsTypeSpecification* origNamedVariable = calloc(1,
            sizeof(MmsTypeSpecification));
    origNamedVariable->name = copyString(attribute->name);

    MmsTypeSpecification* namedVariable = origNamedVariable;

    if (attribute->elementCount > 0) {
        namedVariable->type = MMS_ARRAY;
        namedVariable->typeSpec.array.elementCount = attribute->elementCount;
        namedVariable->typeSpec.array.elementTypeSpec = calloc(1,
                sizeof(MmsTypeSpecification));
        namedVariable = namedVariable->typeSpec.array.elementTypeSpec;
    }

    if (attribute->firstChild != NULL ) {
        namedVariable->type = MMS_STRUCTURE;

        int componentCount = ModelNode_getChildCount((ModelNode*) attribute);

        namedVariable->typeSpec.structure.elements = calloc(componentCount,
                sizeof(MmsTypeSpecification*));

        DataAttribute* subDataAttribute = (DataAttribute*) attribute->firstChild;

        int i = 0;
        while (subDataAttribute != NULL ) {
            namedVariable->typeSpec.structure.elements[i] =
                    createNamedVariableFromDataAttribute(subDataAttribute);

            subDataAttribute = (DataAttribute*) subDataAttribute->sibling;
            i++;
        }

        namedVariable->typeSpec.structure.elementCount = i;
    } else {
        switch (attribute->type) {
        case BOOLEAN:
            namedVariable->type = MMS_BOOLEAN;
            break;
        case INT8:
            namedVariable->typeSpec.integer = 8;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT16:
            namedVariable->typeSpec.integer = 16;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT32:
            namedVariable->typeSpec.integer = 32;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT64:
            namedVariable->typeSpec.integer = 64;
            namedVariable->type = MMS_INTEGER;
            break;
        case INT8U:
            namedVariable->typeSpec.unsignedInteger = 8;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT16U:
            namedVariable->typeSpec.unsignedInteger = 16;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT24U:
            namedVariable->typeSpec.unsignedInteger = 24;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case INT32U:
            namedVariable->typeSpec.unsignedInteger = 32;
            namedVariable->type = MMS_UNSIGNED;
            break;
        case FLOAT32:
            namedVariable->typeSpec.floatingpoint.formatWidth = 32;
            namedVariable->typeSpec.floatingpoint.exponentWidth = 8;
            namedVariable->type = MMS_FLOAT;
            break;
        case FLOAT64:
            namedVariable->typeSpec.floatingpoint.formatWidth = 64;
            namedVariable->typeSpec.floatingpoint.exponentWidth = 11;
            namedVariable->type = MMS_FLOAT;
            break;
        case ENUMERATED:
            namedVariable->typeSpec.integer = 32; // TODO fixme
            namedVariable->type = MMS_INTEGER;
            break;
        case CHECK:
        case CODEDENUM:
            namedVariable->typeSpec.bitString = 2;
            namedVariable->type = MMS_BIT_STRING;
            break;
        case OCTET_STRING_6:
            namedVariable->typeSpec.octetString = -6;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case OCTET_STRING_8:
            namedVariable->typeSpec.octetString = 8;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case OCTET_STRING_64:
            namedVariable->typeSpec.octetString = -64;
            namedVariable->type = MMS_OCTET_STRING;
            break;
        case VISIBLE_STRING_32:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_64:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_65:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_129:
            namedVariable->typeSpec.visibleString = -129;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case VISIBLE_STRING_255:
            namedVariable->typeSpec.visibleString = -255;
            namedVariable->type = MMS_VISIBLE_STRING;
            break;
        case UNICODE_STRING_255:
            namedVariable->typeSpec.mmsString = -255;
            namedVariable->type = MMS_STRING;
            break;
        case GENERIC_BITSTRING:
            namedVariable->type = MMS_BIT_STRING;
            break;
        case TIMESTAMP:
            namedVariable->type = MMS_UTC_TIME;
            break;
        case QUALITY:
            namedVariable->typeSpec.bitString = -13; // -13 = up to 13 bits
            namedVariable->type = MMS_BIT_STRING;
            break;
        case ENTRY_TIME:
            namedVariable->type = MMS_BINARY_TIME;
            namedVariable->typeSpec.binaryTime = 6;
            break;
        case PHYCOMADDR:
            MmsMapping_createPhyComAddrStructure(namedVariable);
            break;
        default:
            printf("MMS-MAPPING: type cannot be mapped %i\n", attribute->type);
            break;
        }
    }

    return origNamedVariable;
}

static int
countChildrenWithFc(DataObject* dataObject, FunctionalConstraint fc)
{
    int elementCount = 0;

    ModelNode* child = dataObject->firstChild;

    while (child != NULL ) {
        if (child->modelType == DataAttributeModelType) {
            DataAttribute* dataAttribute = (DataAttribute*) child;

            if (dataAttribute->fc == fc)
                elementCount++;
        } else if (child->modelType == DataObjectModelType) {
            DataObject* subDataObject = (DataObject*) child;

            if (DataObject_hasFCData(subDataObject, fc))
                elementCount++;
        }

        child = child->sibling;
    }

    return elementCount;
}

static MmsTypeSpecification*
createFCNamedVariableFromDataObject(DataObject* dataObject,
        FunctionalConstraint fc)
{
    MmsTypeSpecification* namedVariable = calloc(1,
            sizeof(MmsTypeSpecification));
    namedVariable->name = copyString(dataObject->name);
    namedVariable->type = MMS_STRUCTURE;

    int elementCount = countChildrenWithFc(dataObject, fc);

    /* Allocate memory for components */
    namedVariable->typeSpec.structure.elements = calloc(elementCount,
            sizeof(MmsTypeSpecification*));

    int i = 0;
    ModelNode* component = dataObject->firstChild;

    while (component != NULL ) {
        if (component->modelType == DataAttributeModelType) {
            DataAttribute* dataAttribute = (DataAttribute*) component;

            if (dataAttribute->fc == fc) {
                namedVariable->typeSpec.structure.elements[i] =
                        createNamedVariableFromDataAttribute(dataAttribute);
                i++;
            }
        } else if (component->modelType == DataObjectModelType) {
            DataObject* subDataObject = (DataObject*) component;

            if (DataObject_hasFCData(subDataObject, fc)) {
                namedVariable->typeSpec.structure.elements[i] =
                        createFCNamedVariableFromDataObject(subDataObject, fc);
                i++;
            }

        }

        component = component->sibling;
    }

    namedVariable->typeSpec.structure.elementCount = elementCount;
    return namedVariable;
}
/*************************************************************************
 * createFCNamedVariable
 * �������� FC
 *************************************************************************/
static MmsTypeSpecification*	createFCNamedVariable(LogicalNode* logicalNode, FunctionalConstraint fc)
{
    MmsTypeSpecification* namedVariable = calloc(1, sizeof(MmsTypeSpecification));

    namedVariable->name = copyString(FunctionalConstrained_toString(fc));			// ��� ���������� � ��������� ����
    namedVariable->type = MMS_STRUCTURE;											// ��� ����������

    int dataObjectCount = 0;

    DataObject* dataObject = (DataObject*) logicalNode->firstChild;

    while (dataObject != NULL ) {
        if (DataObject_hasFCData(dataObject, fc)) dataObjectCount++;
        dataObject = (DataObject*) dataObject->sibling;
    }

    namedVariable->typeSpec.structure.elementCount = dataObjectCount;
    namedVariable->typeSpec.structure.elements = calloc(dataObjectCount, sizeof(MmsTypeSpecification*));

    dataObjectCount = 0;

    dataObject = (DataObject*) logicalNode->firstChild;

    while (dataObject != NULL ) {
        if (DataObject_hasFCData(dataObject, fc)) {

            namedVariable->typeSpec.structure.elements[dataObjectCount] = createFCNamedVariableFromDataObject(dataObject, fc);

            dataObjectCount++;
        }

        dataObject = (DataObject*) dataObject->sibling;
    }

    return namedVariable;
}
/*************************************************************************
 * determineLogicalNodeComponentCount
 * �������������� �����
 *************************************************************************/
static int	determineLogicalNodeComponentCount(LogicalNode* logicalNode)
{
    int componentCount = 0;

    if (LogicalNode_hasFCData(logicalNode, ST))		//logicalNode->firstChild;
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, MX))		// ���������  ->sibling
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SP))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SV))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, CF))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, DC))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SG))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SE))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, SR))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, OR))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, BL))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, EX))
        componentCount++;

    if (LogicalNode_hasFCData(logicalNode, CO))
        componentCount++;

    return componentCount;
}

/*************************************************************************
 * createNamedVariableFromLogicalNode
 * ��������� ���������� RCB � ���������� ����
 * REPORT-CONTROL-BLOCK (���� ���������� �������)
 *
 * bool buffered �������������� ��� ��� BRCB ��� URCB
 *************************************************************************/
static int		countReportControlBlocksForLogicalNode(MmsMapping* self, LogicalNode* logicalNode, bool buffered)
{
    int rcbCount = 0;

    ReportControlBlock** reportControlBlocks = (ReportControlBlock**) self->model->rcbs;

    int i = 0;

    while (reportControlBlocks[i] != NULL ) {
        if (reportControlBlocks[i]->parent == logicalNode) {
            if (reportControlBlocks[i]->buffered == buffered)
                rcbCount++;
        }

        i++;
    }

    return rcbCount;
}

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

static int
countGSEControlBlocksForLogicalNode(MmsMapping* self, LogicalNode* logicalNode)
{
    int gseCount = 0;

    GSEControlBlock** gseControlBlocks = (GSEControlBlock**) self->model->gseCBs;

    int i = 0;

    while (gseControlBlocks[i] != NULL) {
        if (gseControlBlocks[i]->parent == logicalNode) {
            gseCount++;
        }

        i++;
    }

    return gseCount;
}

#endif

/*************************************************************************
 * createNamedVariableFromLogicalNode
 * �������� �� ���������� ����� (LN) ����������� ���������� (namedVariables)
 *************************************************************************/
static MmsTypeSpecification*	createNamedVariableFromLogicalNode(MmsMapping* self, MmsDomain* domain, LogicalNode* logicalNode)
{
    MmsTypeSpecification* namedVariable = malloc(sizeof(MmsTypeSpecification));
    USART_TRACE("0x%X:\n",namedVariable);

    USART_TRACE("�������� �� ���������� �����(LN) ����������(namedVariables)\n");


    namedVariable->name = copyString(logicalNode->name);							// ��� MMS ����������� ���������� = ��� LN
    namedVariable->type = MMS_STRUCTURE;											// ��� ���������� = MMS_STRUCTURE

    int componentCount = determineLogicalNodeComponentCount(logicalNode);			// �������� �������������� ����� ���������� ����� FC �����������

//    USART_TRACE("LogicalNode %s has %i fc components\n", logicalNode->name,componentCount);
    USART_TRACE("	���������� ����(LN) %s ����� %i �������������� ����� (fc components)\n", logicalNode->name,componentCount);

    int brcbCount = countReportControlBlocksForLogicalNode(self, logicalNode, true); // ��������� ���������� ���� ���������� ������� BRCB(true) � LN

    if (brcbCount > 0) {
//    	USART_TRACE("  and %i buffered RCBs\n", brcbCount);
    	USART_TRACE("  � %i ������ ���������� �������������� ������� BRCBs\n", brcbCount);
        componentCount++;
    }

    int urcbCount = countReportControlBlocksForLogicalNode(self, logicalNode, false); // ��������� ���������� ���� ���������� ������� URCB(false) � LN

    if (urcbCount > 0) {
//    	USART_TRACE("  and %i unbuffered RCBs\n", urcbCount);
    	USART_TRACE("  � %i ������ ���������� ���������������� ������� URCBs\n", urcbCount);
        componentCount++;
    }

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

    int gseCount = countGSEControlBlocksForLogicalNode(self, logicalNode);

    if (gseCount > 0) {
    	USART_TRACE("   and %i GSE control blocks\n", gseCount);
        componentCount++;
    }

#endif

    namedVariable->typeSpec.structure.elements = calloc(componentCount, sizeof(MmsTypeSpecification*));

    /* Create a named variable of type structure for each functional constrained */
    // �������� ������������ ���������� ��������� ���� ��� ������� FC ��������������� �����������
    int currentComponent = 0;

    if (LogicalNode_hasFCData(logicalNode, ST)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, ST);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, MX)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, MX);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, SP)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, SP);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, SV)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, SV);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, CF)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, CF);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, DC)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, DC);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, SG)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, SG);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, SE)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, SE);
        currentComponent++;
    }
    if (LogicalNode_hasFCData(logicalNode, SR)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, SR);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, OR)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, OR);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, BL)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, BL);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, EX)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, EX);
        currentComponent++;
    }

    if (LogicalNode_hasFCData(logicalNode, CO)) {
        namedVariable->typeSpec.structure.elements[currentComponent] = createFCNamedVariable(logicalNode, CO);
        currentComponent++;
    }

    if (brcbCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] = Reporting_createMmsBufferedRCBs(self, domain, logicalNode, brcbCount);
        currentComponent++;
    }

    if (urcbCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] = Reporting_createMmsUnbufferedRCBs(self, domain, logicalNode, urcbCount);
        currentComponent++;
    }

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1
    if (gseCount > 0) {
        namedVariable->typeSpec.structure.elements[currentComponent] = GOOSE_createGOOSEControlBlocks(self, domain, logicalNode, gseCount);

        currentComponent++;
    }
#endif

    namedVariable->typeSpec.structure.elementCount = currentComponent;

    return namedVariable;
}
/*************************************************************************
 * createMmsDomainFromIedDevice
 * �������� ����� �� ����������� ���������� LD
 * ����� ��� ������ ����� �� ��� � ��� LD
 * ���������� ��������� = ���������� ����� LN
 *
 *************************************************************************/
static MmsDomain*	createMmsDomainFromIedDevice(MmsMapping* self, LogicalDevice* logicalDevice)
{
    MmsDomain* domain = MmsDomain_create(logicalDevice->name);						// �������� ����� � ������ logicalDevice->name

USART_TRACE("\n");
USART_TRACE("-------------------------------------------------------------\n");
USART_TRACE("�������� ����� �� ����������� ���������� LD\n");
USART_TRACE("-------------------------------------------------------------\n");
    int nodesCount = LogicalDevice_getLogicalNodeCount(logicalDevice);				// ������� ����� ���������� ����� LN � ���������� ���������� LD.

    // LN ������ ������� ������
    domain->namedVariablesCount = nodesCount;										// ���������� ����� LN
    domain->namedVariables = malloc(nodesCount * sizeof(MmsTypeSpecification*));	// ������� ������ ��� ���� ��������� ������ LN

    USART_TRACE("	����� ���������� ����� LN ��� �� (namedVariables): %u\n",nodesCount);

    USART_TRACE("������ ��� ���:0x%X\n",domain->namedVariables);

    LogicalNode* logicalNode = logicalDevice->firstChild;

    int i = 0;
    while (logicalNode != NULL ) {													// ����������� ��� LN
        domain->namedVariables[i] = createNamedVariableFromLogicalNode(self, domain, logicalNode);	// � �������� �� LN ���������� namedVariables

        logicalNode = (LogicalNode*) logicalNode->sibling;
        i++;
    }

    USART_TRACE("-------------------------------------------------------------\n");
    USART_TRACE("\n");
    return domain;
}

/*************************************************************************
 * createMmsDataModel
 * �������� ������
 *************************************************************************/
static void	createMmsDataModel(MmsMapping* self, int iedDeviceCount, MmsDevice* mmsDevice, IedModel* iedModel)
{
    mmsDevice->domains = malloc((iedDeviceCount) * sizeof(MmsDomain*));
    mmsDevice->domainCount = iedDeviceCount;

    LogicalDevice* logicalDevice = iedModel->firstChild;

    int i = 0;
    while (logicalDevice != NULL ) {
        mmsDevice->domains[i] = createMmsDomainFromIedDevice(self, logicalDevice);	//  �������� ������
        i++;
        logicalDevice = logicalDevice->sibling;
    }
}

/*************************************************************************
 * createDataSets
 *
 *************************************************************************/
static void	createDataSets(MmsDevice* mmsDevice, IedModel* iedModel)
{
    DataSet** datasets = (DataSet**) iedModel->dataSets;

    int i = 0;

    while (datasets[i] != NULL ) {
        MmsDomain* dataSetDomain = MmsDevice_getDomain(mmsDevice, datasets[i]->logicalDeviceName);

        MmsNamedVariableList varList = MmsNamedVariableList_create(datasets[i]->name, false);

        int fcdaCount = datasets[i]->elementCount;
        int fcdaIdx = 0;

        DataSetEntry** fcdas = datasets[i]->fcda;

        for (fcdaIdx = 0; fcdaIdx < fcdaCount; fcdaIdx++) {
            MmsAccessSpecifier accessSpecifier;
            accessSpecifier.domain = MmsDevice_getDomain(mmsDevice, fcdas[fcdaIdx]->logicalDeviceName);
            accessSpecifier.variableName = fcdas[fcdaIdx]->variableName;
            accessSpecifier.arrayIndex = fcdas[fcdaIdx]->index;
            accessSpecifier.componentName = fcdas[fcdaIdx]->componentName;

            MmsNamedVariableListEntry variableListEntry = MmsNamedVariableListEntry_create(accessSpecifier);

            MmsNamedVariableList_addVariable(varList, variableListEntry);
        }

        MmsDomain_addNamedVariableList(dataSetDomain, varList);

        i++;
    }
}
/*************************************************************************
 * createMmsModelFromIedModel
 * �������� MMS ���������� �� IED ��������� ����������
 *************************************************************************/
static MmsDevice*	createMmsModelFromIedModel(MmsMapping* self, IedModel* iedModel)
{
    MmsDevice* mmsDevice = NULL;

    if (iedModel->firstChild != NULL ) {

        mmsDevice = MmsDevice_create(iedModel->name);						// ����� ��� ������ mmsDevice->deviceName =  iedModel->name

        int iedDeviceCount = IedModel_getLogicalDeviceCount(iedModel);		// ������� ���������� LD ���������� ���������

        createMmsDataModel(self, iedDeviceCount, mmsDevice, iedModel);		// �������� MmsData �� LD

        createDataSets(mmsDevice, iedModel);
    }

    return mmsDevice;
}
/*************************************************************************
 * MmsMapping_create
 * �������� ��������� � ������ model->name
 *************************************************************************/
MmsMapping*	MmsMapping_create(IedModel* model)
{

    MmsMapping* self = calloc(1, sizeof(struct sMmsMapping));
   // MmsMapping* self = pvPortMalloc(sizeof(struct sMmsMapping));

	USART_TRACE("IedServer->MmsMapping  -  �������� ������ ��� ��������� �� ������:0x%X\n",self);

    self->model = model;

    self->reportControls = LinkedList_create();					//
	USART_TRACE("	MmsMapping->reportControls - �������� ������ ��� ��������� �� ������:0x%X\n",self->reportControls);


    if (CONFIG_INCLUDE_GOOSE_SUPPORT)							// ���� ����� ������������ GOOSE ��������
        self->gseControls = LinkedList_create();

    self->controlObjects = LinkedList_create();					//
	USART_TRACE("	MmsMapping->controlObjects - �������� ������ ��� ���������  �� ������:0x%X\n",self->controlObjects);

    self->observedObjects = LinkedList_create();				//
	USART_TRACE("	MmsMapping->observedObjects - �������� ������ ��� ��������� �� ������:0x%X\n",self->observedObjects);

    self->mmsDevice = createMmsModelFromIedModel(self, model);	// �������� MMS ������ �� Ied ��������� ����������
	USART_TRACE("	MmsMapping->mmsDevice -  ������� MMS ������ �� IED ������ �� ������:0x%X\n",self->mmsDevice);


    return self;
}
/*************************************************************************
 * MmsMapping_create
 * ��������� ���������
 *************************************************************************/
void	MmsMapping_destroy(MmsMapping* self)
{

    if (self->reportWorkerThread != NULL) {
        self->reportThreadRunning = false;
        Thread_destroy(self->reportWorkerThread);
    }

    if (self->mmsDevice != NULL )
        MmsDevice_destroy(self->mmsDevice);

    LinkedList_destroyDeep(self->reportControls, ReportControl_destroy);

    if (CONFIG_INCLUDE_GOOSE_SUPPORT)
        LinkedList_destroyDeep(self->gseControls, MmsGooseControlBlock_destroy);

    LinkedList_destroyDeep(self->controlObjects, ControlObject_destroy);

    LinkedList_destroy(self->observedObjects);

    free(self);
}

/*************************************************************************
 * MmsMapping_getMmsDeviceModel
 *
 *************************************************************************/
MmsDevice*		MmsMapping_getMmsDeviceModel(MmsMapping* mapping)
{
    return mapping->mmsDevice;
}



static bool
isReportControlBlock(char* separator)
{
    if (strncmp(separator + 1, "RP", 2) == 0)
        return true;

    if (strncmp(separator + 1, "BR", 2) == 0)
        return true;

    return false;
}

static bool
isWritableFC(char* separator)
{
    if (strncmp(separator + 1, "CF", 2) == 0)
        return true;

    if (strncmp(separator + 1, "DC", 2) == 0)
        return true;

    if (strncmp(separator + 1, "SP", 2) == 0)
        return true;

    if (strncmp(separator + 1, "SV", 2) == 0)
        return true;

    return false;
}


static bool
isControllable(char* separator)
{
    if (strncmp(separator + 1, "CO", 2) == 0)
        return true;
    else
        return false;
}

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

static bool
isGooseControlBlock(char* separator)
{
    if (strncmp(separator + 1, "GO", 2) == 0)
        return true;
    else
        return false;
}

#endif


char*
MmsMapping_getNextNameElement(char* name)
{
    char* separator = strchr(name, '$');

    if (separator == NULL)
        return NULL;

    separator++;

    if (*separator == 0)
        return NULL;

    return separator;
}


#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

static MmsGooseControlBlock
lookupGCB(MmsMapping* self, MmsDomain* domain, char* lnName, char* objectName)
{
    LinkedList element = LinkedList_getNext(self->gseControls);

    while (element != NULL) {
        MmsGooseControlBlock mmsGCB = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_getDomain(mmsGCB) == domain) {
            if (strcmp(MmsGooseControlBlock_getLogicalNodeName(mmsGCB), lnName) == 0) {
                if (strcmp(MmsGooseControlBlock_getName(mmsGCB), objectName) == 0) {
                    return mmsGCB;
                }
            }
        }

        element = LinkedList_getNext(element);
    }

    return NULL;
}

static MmsValueIndication
writeAccessGooseControlBlock(MmsMapping* self, MmsDomain* domain, char* variableIdOrig,
                         MmsValue* value)
{
    MmsValueIndication indication = MMS_VALUE_ACCESS_DENIED;

    char* variableId = copyString(variableIdOrig);

    char* separator = strchr(variableId, '$');

    *separator = 0;

    char* lnName = variableId;

    if (lnName == NULL )
        goto free_and_return;

    char* objectName = MmsMapping_getNextNameElement(separator + 1);

    if (objectName == NULL )
        goto free_and_return;

    char* varName = MmsMapping_getNextNameElement(objectName);

    if (varName != NULL)
        *(varName - 1) = 0;
    else
        goto free_and_return;

    MmsGooseControlBlock mmsGCB = lookupGCB(self, domain, lnName, objectName);

    if (mmsGCB == NULL) {
        indication = MMS_VALUE_ACCESS_DENIED;
        goto free_and_return;
    }

    if (strcmp(varName, "GoEna") == 0) {
        if (MmsValue_getType(value) != MMS_BOOLEAN) {
            indication = MMS_VALUE_VALUE_INVALID;
            goto free_and_return;
        }

        if (MmsValue_getBoolean(value))
            MmsGooseControlBlock_enable(mmsGCB);
        else
            MmsGooseControlBlock_disable(mmsGCB);

        indication = MMS_VALUE_OK;
    }


free_and_return:
    free(variableId);

    return indication;
}

#endif

bool
checkIfValueBelongsToModelNode(DataAttribute* dataAttribute, MmsValue* value)
{
    if (dataAttribute->mmsValue == value) return true;

    DataAttribute* child = dataAttribute->firstChild;

    while (child != NULL) {
        if (checkIfValueBelongsToModelNode(child, value))
            return true;
        else
            child = child->sibling;
    }

    if (MmsValue_getType(value) == MMS_STRUCTURE) {
        int elementCount = value->value.structure.size;

        int i = 0;
        for (i = 0; i < elementCount; i++) {
            MmsValue* childValue = value->value.structure.components[i];

            if (checkIfValueBelongsToModelNode(dataAttribute, childValue))
                return true;
        }
    }

    return false;
}

/*************************************************************************
 * mmsWriteHandler
 * ������� ������ ������ �������� �� �������.
 * variableId - ���������� � ������� ����� ������
 * value - ������ ��� ������ � ���� ���������
 *************************************************************************/
static MmsValueIndication	mmsWriteHandler(void* parameter, MmsDomain* domain, char* variableId, MmsValue* value, MmsServerConnection* connection)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (DEBUG) printf("Write requested %s\n", variableId);

    USART_TRACE_RED("Write requested: %s\n", variableId);

    int variableIdLen = strlen(variableId);

    /* Access control based on functional constraint */

    char* separator = strchr(variableId, '$');					// ���� ������ $

    USART_TRACE("�������: %.3s\n",separator);

    if (separator == NULL )   return MMS_VALUE_ACCESS_DENIED;	// ���� ��� ��� �� ���������� � �������

    /* Controllable objects - CO */								// ��������� �������� CO - ����������
    if (isControllable(separator)) {
    	USART_TRACE_BLUE("�������� CO - ����������: %u \n", value->value);
        return Control_writeAccessControlObject(self, domain, variableId, value);
    }

    /* Goose control block - GO */								// ��������� �������� GO - ����������
    if (CONFIG_INCLUDE_GOOSE_SUPPORT) {
        if (isGooseControlBlock(separator)) {
            return writeAccessGooseControlBlock(self, domain, variableId, value);
        }
    }

    /* Report control blocks - BR, RP */						// ��������� �������� BR, RP - ������
    if (isReportControlBlock(separator)) {
    	USART_TRACE_BLUE("�������� BR, RP - ������\n");

        LinkedList reportControls = self->reportControls;

        LinkedList nextElement = reportControls;

        while ((nextElement = LinkedList_getNext(nextElement)) != NULL ) {
            ReportControl* rc = (ReportControl*) nextElement->data;

            if (strstr(variableId, rc->name) != NULL ) {
                char* elementName = variableId + strlen(rc->name) + 1;

                return Reporting_RCBWriteAccessHandler(self, rc, elementName, value, connection);
            }
        }

        return MMS_VALUE_ACCESS_DENIED;
    }

    /* writable data model elements - SP, SV, CF, DC */			// ��������� �������� SP, SV, CF, DC ������������
    if (isWritableFC(separator)) {

    	USART_TRACE_BLUE("�������� SP, SV, CF, DC ������������\n");

        MmsValue* cachedValue;

        cachedValue = MmsServer_getValueFromCache(self->mmsServer, domain, variableId);



        if (cachedValue != NULL ) {								// ���� ���� ����� ��� ���������� � ������, �� �����

        	USART_TRACE_BLUE("���� ���������� � ������, ������� ������.\n");
           	USART_TRACE_BLUE("value->type: %u, value: %s\n",value->type,value->value);

        	MmsValue_update(cachedValue, value);				// ����� ���������� value  � ��������� cachedValue

            LinkedList element = LinkedList_getNext(self->observedObjects);

            while (element != NULL) {
                AttributeObserver* observer = (AttributeObserver*) element->data;
                DataAttribute* dataAttribute = observer->attribute;

                if (checkIfValueBelongsToModelNode(dataAttribute, cachedValue)) {
                   observer->handler(dataAttribute);								// �������� callback ������� ���� ������� �� �� �������. ��. observerCallback
                }

                element = LinkedList_getNext(element);
            }

            return MMS_VALUE_OK;
        } else{
           	USART_TRACE_BLUE("�� ����� ���������� � ������!!!\n");

            return MMS_VALUE_VALUE_INVALID;						// ���� ��� ������ ����� ���������� � ������, �� ���
        }
    }

    return MMS_VALUE_ACCESS_DENIED;
}

void
MmsMapping_addObservedAttribute(MmsMapping* self, DataAttribute* dataAttribute, void* handler)
{
    AttributeObserver* observer = malloc(sizeof(struct sAttributeObserver));
    //AttributeObserver* observer = pvPortMalloc(sizeof(struct sAttributeObserver));

    observer->attribute = dataAttribute;
    observer->handler = handler;

    LinkedList_add(self->observedObjects, observer);
}

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

MmsValue*
readAccessGooseControlBlock(MmsMapping* self, MmsDomain* domain, char* variableIdOrig)
{
    MmsValue* value = NULL;

    char* variableId = copyString(variableIdOrig);

    char* separator = strchr(variableId, '$');

    *separator = 0;

    char* lnName = variableId;

    if (lnName == NULL )
        return NULL;

    char* objectName = MmsMapping_getNextNameElement(separator + 1);

    if (objectName == NULL )
        return NULL;

    char* varName = MmsMapping_getNextNameElement(objectName);

    if (varName != NULL)
        *(varName - 1) = 0;

    MmsGooseControlBlock mmsGCB = lookupGCB(self, domain, lnName, objectName);

    if (mmsGCB != NULL) {

        if (varName != NULL) {
            value = MmsGooseControlBlock_getGCBValue(mmsGCB, varName);
        }
        else {
            value = MmsGooseControlBlock_getMmsValues(mmsGCB);
        }
    }

    free(variableId);

    return value;
}

#endif

/*************************************************************************
 * mmsReadHandler
 * ������� ������ ������ �� ���� �������.
 *************************************************************************/
static MmsValue*	mmsReadHandler(void* parameter, MmsDomain* domain, char* variableId)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (DEBUG)
        printf("Requested %s\n", variableId);
    USART_TRACE_RED("Read Requested %s\n", variableId);

    char* separator = strchr(variableId, '$');

    USART_TRACE("�������: %.3s\n",separator);

    if (separator == NULL )
        return NULL;

    /* Controllable objects - CO */
    if (isControllable(separator)) {
        return Control_readAccessControlObject(self, domain, variableId);
    }

    /* GOOSE control blocks - GO */
    if (CONFIG_INCLUDE_GOOSE_SUPPORT) {
        if (isGooseControlBlock(separator)) {
            return readAccessGooseControlBlock(self, domain, variableId);
        }
    }

    /* Report control blocks - BR, RP */
    if (isReportControlBlock(separator)) {

        LinkedList reportControls = self->reportControls;

        LinkedList nextElement = reportControls;

        char* reportName = MmsMapping_getNextNameElement(separator + 1);

        if (reportName == NULL) return NULL;

        separator = strchr(reportName, '$');

        int reportNameLen;

        if (separator != NULL)
            reportNameLen = separator - reportName;
        else
            reportNameLen = strlen(reportName);


        while ((nextElement = LinkedList_getNext(nextElement)) != NULL ) {
            ReportControl* rc = (ReportControl*) nextElement->data;

            if (strncmp(variableId, rc->name, reportNameLen) == 0) {

                char* elementName = MmsMapping_getNextNameElement(reportName);

                MmsValue* value = NULL;

                if (elementName != NULL)
                    value = ReportControl_getRCBValue(rc, elementName);
                else
                    value = rc->rcbValues;

                return value;
            }
        }
    }

    return NULL ;
}

void MmsMapping_setMmsServer(MmsMapping* self, MmsServer server)
{
    self->mmsServer = server;
}

static void
deactivateReportsForConnection(MmsMapping* self, MmsServerConnection* connection)
{
    LinkedList reportControl = self->reportControls;

    while ((reportControl = LinkedList_getNext(reportControl)) != NULL) {
        ReportControl* rc = (ReportControl*) reportControl->data;

        if (rc->clientConnection == connection) {
            // TODO make thread safe!
            rc->enabled = false;
            rc->clientConnection = NULL;

            MmsValue* rptEna = ReportControl_getRCBValue(rc, "RptEna");
            MmsValue_setBoolean(rptEna, false);

            if (rc->inclusionField != NULL) {
                MmsValue_delete(rc->inclusionField);
                rc->inclusionField = NULL;
            }
        }
    }
}

static void
mmsConnectionHandler (void* parameter, MmsServerConnection* connection, MmsServerEvent event)
{
    MmsMapping* self = (MmsMapping*) parameter;

    if (event == MMS_SERVER_CONNECTION_CLOSED) {
        deactivateReportsForConnection(self, connection);
    }
}

void MmsMapping_installHandlers(MmsMapping* self)
{
    MmsServer_installReadHandler(self->mmsServer, mmsReadHandler, (void*) self);					// ������ ������ �� ���� �������.
    MmsServer_installWriteHandler(self->mmsServer, mmsWriteHandler, (void*) self);					// ������� ������ ������ �������� �� �������.
    MmsServer_installConnectionHandler(self->mmsServer, mmsConnectionHandler, (void*) self);

	USART_TRACE("mmsMapping->mmsServer->mmsReadHandler			������ ������� �� ������� Read\n");
	USART_TRACE("mmsMapping->mmsServer->mmsWriteHandler			������ ������� �� ������� Write\n");
	USART_TRACE("mmsMapping->mmsServer->mmsConnectionHandler	������ ������� �� ������� Connection\n");

}


static bool
isMemberValueRecursive(MmsValue* container, MmsValue* value) {
    if (container == value)
        return true;
    else {
        if ((MmsValue_getType(container) == MMS_STRUCTURE) ||
        	(MmsValue_getType(container) == MMS_ARRAY))
        {
            int compCount = container->value.structure.size;
            int i;
            for (i = 0; i < compCount; i++) {
                if (isMemberValueRecursive(container->value.structure.components[i], value))
                    return true;
            }

        }
    }

    return false;
}

static bool
DataSet_isMemberValue(DataSet* dataSet, MmsValue* value, int* index)
{
    int i;
    for (i = 0; i < dataSet->elementCount; i++) {
        MmsValue* dataSetValue = dataSet->fcda[i]->value;

        if (isMemberValueRecursive(dataSetValue, value)) {
            *index = i;
            return true;
        }
    }

    return false;
}

void
MmsMapping_triggerReportObservers(MmsMapping* self, MmsValue* value, ReportInclusionFlag flag)
{
    LinkedList element = self->reportControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        ReportControl* rc = (ReportControl*) element->data;

        if (rc->enabled) {
            int index;

            if (DataSet_isMemberValue(rc->dataSet, value, &index)) {
                ReportControl_valueUpdated(rc, index, flag);
            }
        }
    }
}

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

void
MmsMapping_triggerGooseObservers(MmsMapping* self, MmsValue* value)
{
    LinkedList element = self->gseControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        MmsGooseControlBlock gcb = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_isEnabled(gcb)) {
            DataSet* dataSet = MmsGooseControlBlock_getDataSet(gcb);

            int index;

            if (DataSet_isMemberValue(dataSet, value, &index)) {
                MmsGooseControlBlock_observedObjectChanged(gcb);
            }
        }
    }
}

#endif


void
MmsMapping_enableGoosePublishing(MmsMapping* self)
{
#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

    LinkedList element = self->gseControls;

    while ((element = LinkedList_getNext(element)) != NULL) {
        MmsGooseControlBlock gcb = (MmsGooseControlBlock) element->data;

        MmsGooseControlBlock_enable(gcb);
    }

#endif
}
/*************************************************************************
 * MmsMapping_addControlObject
 *
 *************************************************************************/
void	MmsMapping_addControlObject(MmsMapping* self, ControlObject* controlObject)
{
    LinkedList_add(self->controlObjects, controlObject);
}

ControlObject*
MmsMapping_getControlObject(MmsMapping* self, MmsDomain* domain, char* lnName, char* coName)
{
    return Control_lookupControlObject(self, domain, lnName, coName);
}


char*
MmsMapping_getMmsDomainFromObjectReference(char* objectReference, char* buffer)
{
    int objRefLength = strlen(objectReference);

    //check if LD name is present
    int i;
    for (i = 0; i < objRefLength; i++) {
        if (objectReference[i] == '/') {
            break;
        }
    }

    if (i == objRefLength)
        return NULL ;

    char* domainName;

    if (buffer == NULL )
        domainName = malloc(i + 1);
    else
        domainName = buffer;

    int j;
    for (j = 0; j < i; j++) {
        domainName[j] = objectReference[j];
    }

    domainName[j] = 0;

    return domainName;
}

char*
MmsMapping_createMmsVariableNameFromObjectReference(char* objectReference,
        FunctionalConstraint fc, char* buffer)
{
    int objRefLength = strlen(objectReference);

    //check if LD name is present
    int i;
    for (i = 0; i < objRefLength; i++) {
        if (objectReference[i] == '/') {
            break;
        }
    }

    if (i == objRefLength)
        i = 0;
    else
        i++;

    char* fcString = FunctionalConstrained_toString(fc);

    if (fcString == NULL )
        return NULL ;

    char* mmsVariableName;

    if (buffer == NULL )
        mmsVariableName = malloc(objRefLength - i + 4);
    else
        mmsVariableName = buffer;

    int sourceIndex = i;
    int destIndex = 0;

    while (objectReference[sourceIndex] != '.')
        mmsVariableName[destIndex++] = objectReference[sourceIndex++];

    sourceIndex++;

    mmsVariableName[destIndex++] = '$';
    mmsVariableName[destIndex++] = fcString[0];
    mmsVariableName[destIndex++] = fcString[1];
    mmsVariableName[destIndex++] = '$';

    while (sourceIndex < objRefLength) {
        if (objectReference[sourceIndex] != '.')
            mmsVariableName[destIndex++] = objectReference[sourceIndex++];
        else {
            mmsVariableName[destIndex++] = '$';
            sourceIndex++;
        }
    }

    mmsVariableName[destIndex] = 0;

    return mmsVariableName;
}

#if CONFIG_INCLUDE_GOOSE_SUPPORT == 1

void
GOOSE_processGooseEvents(MmsMapping* self, uint64_t currentTimeInMs)
{
    LinkedList element = LinkedList_getNext(self->gseControls);

    while (element != NULL) {
        MmsGooseControlBlock mmsGCB = (MmsGooseControlBlock) element->data;

        if (MmsGooseControlBlock_isEnabled(mmsGCB)) {
            MmsGooseControlBlock_checkAndPublish(mmsGCB, currentTimeInMs);
        }

        element = LinkedList_getNext(element);
    }
}

#endif

/* single worker thread for all enabled GOOSE and report control blocks */
static void
eventWorkerThread(MmsMapping* self)
{
    bool running = true;

    while (running) {
        uint64_t currentTimeInMs = Hal_getTimeInMs();

        if (CONFIG_INCLUDE_GOOSE_SUPPORT)
            GOOSE_processGooseEvents(self, currentTimeInMs);

        Reporting_processReportEvents(self, currentTimeInMs);

        Thread_sleep(10); /* sleep for 10 ms */

        running = self->reportThreadRunning;
    }

    if (DEBUG) printf("event worker thread finished!\n");
}

/*************************************************************************
 * MmsMapping_startEventWorkerThread
 * �� ������� �������
 *************************************************************************/
void	MmsMapping_startEventWorkerThread(MmsMapping* self)
{
    self->reportThreadRunning = true;

    Thread thread = Thread_create(eventWorkerThread, self, false);
    self->reportWorkerThread = thread;
    Thread_start(thread);
}

