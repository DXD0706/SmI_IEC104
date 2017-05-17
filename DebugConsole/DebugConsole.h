
#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

/*
 * ������ ������, ������� ��������� ������� � ������� USART �����.
 */
//void DEBUGConsoleTask( void *pvParameters );
void DEBUGConsoleTask(void const * argument);
void DEBUGUSARTOUTTask(void const * argument);

void	xDEBUGRTUReceiveFSM( void );			// ������ ���������� �� ������
void	xDEBUGRTUTransmitFSM( void );			// ������ ���������� �� ��������

void vOutputString( const char * const pcMessage );
void vOutputTime (void);

#endif /* DEBUG_CONSOLE_H */



