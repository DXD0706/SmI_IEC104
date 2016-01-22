/*
 *  byte_buffer.h
 *
 *  Copyright 2016 Alex Godulevich
 *
 */

#ifndef BYTE_BUFFER_H_
#define BYTE_BUFFER_H_

#include "stdlib.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "stdbool.h"
#include "stdint.h"

typedef struct {
	uint8_t* buffer;			// ����� ������
	int maxSize;				// ������ ������
	int currPos;				// ������� ��� ������/������
} ByteBuffer;

ByteBuffer*	ByteBuffer_create(ByteBuffer* self, int maxSize);								//��������� ������ ��� ������ � �������� maxSize
void		ByteBuffer_destroy(ByteBuffer* self);											//������������ ������ �� ������
void		ByteBuffer_wrap(ByteBuffer* self, uint8_t* buf, int size, int maxSize);			//������ ������ � ���������

int			ByteBuffer_append(ByteBuffer* self, uint8_t* data, int dataSize);				//������ � ����� ����� ������� � ������� ������� currPos �������� dataSize
int			ByteBuffer_appendByte(ByteBuffer* self, uint8_t byte);							//������ � ����� ����� ������� � ������� ������� currPos
int			ByteBuffer_readByteUint8(ByteBuffer* self, uint8_t* byte);						//������ �� ������ ����� ������� � ������� ������� currPos
int			ByteBuffer_readByteUint16(ByteBuffer* self, uint16_t* word);					//������ �� ������ ����� ������� � ������� ������� currPos

uint8_t*	ByteBuffer_getBuffer(ByteBuffer* self);											//��������� ������ ������
int			ByteBuffer_getcurrPos(ByteBuffer* self);										//��������� ������� �������
int			ByteBuffer_getMaxSize(ByteBuffer* self);										//��������� ������� ������

int			ByteBuffer_setcurrPos(ByteBuffer* self, int size);								//������� ������� � ������� size

int			ByteBuffer_send(ByteBuffer* self);												//�����. �������� � ethernet ����� TCP/IP


#endif /* BYTE_BUFFER_H_ */
