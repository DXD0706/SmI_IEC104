
#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

/*
 * ������ ������, ������� ��������� ������� � ������� USART �����.
 */
//void DEBUGConsoleTask( void *pvParameters );
void DEBUGConsoleTask(void const * argument);

void	xDEBUGRTUReceiveFSM( void );			// ������ ���������� �� ������
void	xDEBUGRTUTransmitFSM( void );			// ������ ���������� �� ��������

void vOutputString( const char * const pcMessage );

#endif /* DEBUG_CONSOLE_H */



