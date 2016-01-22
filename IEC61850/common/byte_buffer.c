/*
 *  byte_buffer.c
 *
 *  Copyright 2016 Alex Godulevich
 *
 *  ����� � �������
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "ethernetif.h"
#include "lwip/tcpip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/api.h"
#include "lwip/sys.h"

#include "byte_buffer.h"

/*************************************************************************
 * ByteBuffer_create(ByteBuffer* self, int maxSize)
 * ��������� ������ ��� ������ � �������� maxSize
 *************************************************************************/
ByteBuffer*	ByteBuffer_create(ByteBuffer* self, int maxSize)
{
	if (self == NULL) {
		self = calloc(1, sizeof(ByteBuffer));
	}

	self->buffer = calloc(maxSize, sizeof(uint8_t));
	self->maxSize = maxSize;
	self->currPos = 0;

	return self;
}
/*************************************************************************
 * ByteBuffer_destroy(ByteBuffer* self)
 * ������������ ������ �� ������
 *************************************************************************/
void	ByteBuffer_destroy(ByteBuffer* self)
{
	free(self->buffer);
	free(self);
}
/*************************************************************************
 * ByteBuffer_wrap(ByteBuffer* self, uint8_t* buf, int size, int maxSize)
 * ������ ������ � ���������
 *************************************************************************/
void	ByteBuffer_wrap(ByteBuffer* self, uint8_t* buf, int size, int maxSize)
{
	self->buffer = buf;
	self->currPos = size;
	self->maxSize = maxSize;
}
/*************************************************************************
 * ByteBuffer_append(ByteBuffer* self, uint8_t* data, int dataSize)
 * ������ � ����� ����� ������� � ������� ������� currPos �������� dataSize
 *************************************************************************/
int	ByteBuffer_append(ByteBuffer* self, uint8_t* data, int dataSize)
{
	if (self->currPos + dataSize <= self->maxSize) {
		memcpy(self->buffer + self->currPos, data, dataSize);
		self->currPos += dataSize;
		return 1;
	}
	else
		return 0;
}
/*************************************************************************
 * ByteBuffer_appendByte(ByteBuffer* self, uint8_t byte)
 * ������ � ����� ����� ������� � ������� ������� currPos
 *************************************************************************/
int	ByteBuffer_appendByte(ByteBuffer* self, uint8_t byte)
{
	if (self->currPos  < self->maxSize) {
		self->buffer[self->currPos] = byte;
		self->currPos ++;
		return 1;
	}
	else
		return 0;
}
/*************************************************************************
 * ByteBuffer_readByteUint8(ByteBuffer* self, uint8_t* byte)
 * ������ �� ������ ����� ������� � ������� ������� currPos
 *************************************************************************/
int	ByteBuffer_readByteUint8(ByteBuffer* self, uint8_t* byte)
{
	if (self->currPos  < self->maxSize) {
		*byte = self->buffer[self->currPos];
		self->currPos ++;
		return 1;
	}
	else
		return 0;
}
/*************************************************************************
 * ByteBuffer_readByteUint8(ByteBuffer* self, uint8_t* byte)
 * ������ �� ������ ����� ������� � ������� ������� currPos
 *************************************************************************/
int	ByteBuffer_readByteUint16(ByteBuffer* self, uint16_t* word){
	uint8_t	byte;

	if ((self->currPos+1)  < self->maxSize) {
		byte = self->buffer[self->currPos ++];
		*word = (uint16_t)byte<<8;
		byte = self->buffer[self->currPos ++];
		*word += byte;//(uint16_t)byte<<8 + self->buffer[self->currPos];
//		self->currPos ++;
		return 2;
	}
	else
		if(self->currPos+1  == self->maxSize){
		return 1;
	}
	else
		return 0;
}
/*************************************************************************
 * ByteBuffer_getBuffer(ByteBuffer* self)
 * ��������� ������ ������
 *************************************************************************/
uint8_t*	ByteBuffer_getBuffer(ByteBuffer* self)
{
	return self->buffer;
}
/*************************************************************************
 * ByteBuffer_getMaxSize(ByteBuffer* self)
 * ��������� ������� ������
 *************************************************************************/
int	ByteBuffer_getMaxSize(ByteBuffer* self)
{
	return self->maxSize;
}
/*************************************************************************
 * ByteBuffer_getcurrPos(ByteBuffer* self)
 * ��������� ������� �������
 *************************************************************************/
int	ByteBuffer_getcurrPos(ByteBuffer* self)
{
	return self->currPos;
}
/*************************************************************************
 * ByteBuffer_setSize(ByteBuffer* self, int size)
 * ������� ������� � ������� size
 *************************************************************************/
int	ByteBuffer_setcurrPos(ByteBuffer* self, int size)
{
	if (size <= self->maxSize)
		self->currPos = size;

	return self->currPos;
}
