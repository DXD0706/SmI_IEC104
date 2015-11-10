/*
 * iec104.h
 *
 *  Created on: 25.06.2015
 *      Author: sagok
 */

#ifndef __IEC104_H
#define __IEC104_H

#ifndef __IEC104_TYPES_H
#include "iec104_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#define IEC_OBJECT_MAX		127
#define IEC_TYPEID_LEN		3
#define IEC104_BUF_LEN		255

#define DEFAULT_W			8
#define DEFAULT_K			12
#define DEFAULT_T0			30
#define DEFAULT_T1			15
#define DEFAULT_T2			10
#define DEFAULT_T3			20
#define IEC_APDU_MAX		253
#define IEC_APDU_MIN		4

enum {
	IEC_SLAVE,
	IEC_MASTER
};

enum frame_type {
	FRAME_TYPE_I,
	FRAME_TYPE_S,
	FRAME_TYPE_U
};

/* Information object */
struct iec_object {
	u_short		ioa;	/* information object address */
	u_char		ioa2;	/* information object address */
	union {
		// ���������� � �������� � ����������� ��������
		struct iec_type1	type1;			//M_SP_NA_1 - �������������� ����������
		struct iec_type3	type3;			//M_DP_NA_1 - �������������� ����������
		struct iec_type5	type5;			//M_ST_NA_1 - ���������� � ��������� ������
		struct iec_type7	type7;			//M_BO_NA_1 - ������ �� 32 ���
		struct iec_type9	type9;			//M_ME_NA_1 - �������� ���������� ��������, ��������������� ��������
		struct iec_type11	type11;			//M_ME_NB_1 - �������� ���������� ��������, ���������������� ��������
		struct iec_type13	type13;			//M_ME_NC_1 - �������� ���������� ��������, �������� ������ � ����.�����.
		struct iec_type15	type15;			//M_IT_NA_1 - ������������ �����.
		struct iec_type30	type30;			//M_SP_TB_1 - �������������� ���������� � ������ ������� CP56Time2a
		struct iec_type31	type31;			//M_DP_TB_1 - �������������� ���������� � ������ ������� CP56Time2a
		struct iec_type32	type32;			//M_ST_TB_1 - ���������� � ��������� ������ � ������ ������� CP56Time2a
		struct iec_type33	type33;			//M_BO_TB_1 - ������ �� 32 ��� � ������ ������� CP56Time2a
		struct iec_type34	type34;			//M_ME_TD_1 - �������� ���������� ��������, ��������������� �������� � ������ ������� CP56Time2a
		struct iec_type35	type35;			//M_ME_TE_1 - �������� ���������� ��������, ���������������� �������� � ������ ������� CP56Time2a
		struct iec_type36	type36;			//M_ME_TF_1 - �������� ���������� ��������, �������� ������ � ����.�����. � ������ ������� CP56Time2a
		struct iec_type37	type37;			//M_IT_TB_1 - ������������ ����� � ������ ������� CP56Time2a
		struct iec_type38	type38;			//M_EP_TD_1 - �������� ��������� ������ � ������ ������� CP56Time2a
		struct iec_type39	type39;			//M_EP_TE_1 - ����������� ���������� � ������������ �������� ������� ������ � ������ ������� CP56Time2a
		struct iec_type40	type40;			//M_EP_TF_1 - ����������� ���������� � ������������ �������� ����� ��������� ������ � ������ ������� CP56Time2a

		// ���������� � ������� � ����������� ����������
		struct iec_type100	type100;		//C_IC_NA_1 - ������� ������
		struct iec_type101	type101;		//C_CI_NA_1 - ������� ������ ��������
		struct iec_type103	type102;		//C_RD_NA_1 - ������� ������
		struct iec_type103	type103;		//C_CS_NA_1 - ������� ������������� �������
//		struct iec_type104	type104;		//C_TS_NA_1 - Test Command							��� � 104
		struct iec_type105	type105;		//C_RP_NA_1 - Reset Process Command
		struct iec_type107	type107;		//C_TS_TA_1 - Test Command with Time Tag (104 only)
	} o;	
}__attribute__((__packed__));


struct iec_buf {
	//TAILQ_ENTRY(iec_buf) head;
	u_char	data_len;	/* actual ASDU length */

	// ----- APCI -----------------------------
	struct iechdr {
		u_char	start;
		u_char	length;
		u_char	raw[0];
		union {
			struct iec_i {
				u_char	ft:1;
				u_short	ns:15;
				u_char	res:1;
				u_short	nr:15;
			} ic;
			struct iec_s {
				u_char	ft:1;
				u_short	res1:15;
				u_char	res2:1;
				u_short	nr:15;
			} sc;
			struct iec_u {
				u_char	ft:2;
				u_char	start_act:1;
				u_char	start_con:1;
				u_char	stop_act:1;
				u_char	stop_con:1;
				u_char	test_act:1;
				u_char	test_con:1;
				u_char	res1;
				u_short	res2;
			} uc;
		};
	} h;
	//! ----- APCI -----------------------------

	u_char	data[0];
} __attribute__((__packed__));

struct iecsock;

struct iechooks {
	void (*connect_indication)(struct iecsock *s);
	void (*activation_indication)(struct iecsock *s);
	void (*deactivation_indication)(struct iecsock *s);
	void (*disconnect_indication)(struct iecsock *s, short reason);
	void (*data_indication)(struct iecsock *s, struct iec_buf *b);
	void (*transmit_wakeup)(struct iecsock *s);
};

extern struct iechooks default_hooks;


struct iecsock {
	int					sock;						/* socket descriptor */
	uint8_t				buf[IEC104_BUF_LEN];
	uint16_t			len;
	uint8_t				left;
	uint8_t				type;
	uint8_t				stopdt:1;					/* monitor direction 0=active 1=inactive */
	uint8_t				testfr:1;					/* test function 1=active 0=inactive */
	u_short				w, k;						// w - ����� ������ ��� �������������
													// k - ����� �������� ��� ��������� �������������
													// ���������� ��������:
	u_short				va, vr, vs, va_peer;		// va - ����� ��������� APDU (I) �������.
													// vr - ���������� ��������� ������.
													// vs - ���������� ��������� ��������.
													// va_peer - ����� ���������� ����������� (S) �������.

	struct iec_typeTimers	t0, t1, t2, t3;			// t0 - ������� ��� ������������ ����������.				��� u_short
													// t1 - ������� ��� ������� ��� ������������ APDU.
													// t2 - ������� �������������.
													// t3 - ������� ��� ������� ������ ������������ � ������ ������� �������.
	struct iechooks 	hooks;
	u_long 				recv_cnt, xmit_cnt;
};

enum uframe_func {
	STARTACT,
	STARTCON,
	STOPACT,
	STOPCON,
	TESTACT,
	TESTCON
};

struct iecsock_options {
	u_short		w;
	u_short		k;
	u_short		t0;
	u_short		t1;
	u_short		t2;
	u_short		t3;
};

int iecasdu_parse(struct iec_object *obj, u_char *type, u_short *com_addr, 
	int *cnt, u_char *cause, u_char *test, u_char *pn, u_char *str_ioa,
	u_char *buf, size_t buflen);

/*
 * New functions
 */
void time_t_to_cp56time2a (cp56time2a *tm);
//void current_cp56time2a (cp56time2a *tm);
time_t cp56time2a_to_tm (cp56time2a *tm);

void iecasdu_create_header_all (uint8_t *buf, size_t *buflen, uint8_t type, uint8_t num,
		uint8_t sq, uint8_t cause, uint8_t t, uint8_t pn, uint8_t initaddr, uint16_t ma, uint32_t ca);

//void iecasdu_create_header_all (u_char *buf, size_t *buflen, u_char type, u_char num,
//	u_char sq, u_char cause, u_char t, u_char pn, u_char ma, u_short ca );

// buf, buflen, type(������������� ����), num(����� ��������), sq(������������������ ��� ���������), cause(�������), t(����), pn(������������� ���������), ma(����� �����), ca(����� �������)
#define iecasdu_create_header(buf, buflen, type, num, cause, ca) \
	iecasdu_create_header_all(buf, buflen, type, num, 0, cause, 0, 0, 0, ca);

void iecasdu_add_APCI(u_char *buf, size_t buflen);

void iecasdu_create_type_1 (u_char *buf, size_t *buflen, uint8_t sp);
void iecasdu_create_type_9 (u_char *buf, size_t *buflen,u_short mv);

void iecasdu_create_type_36 (u_char *buf, size_t *buflen, int num, float *mv);
void iecasdu_create_type_100 (u_char *buf, size_t *buflen);
void iecasdu_create_type_101 (u_char *buf, size_t *buflen);
void iecasdu_create_type_103 (u_char *buf, size_t *buflen);

void IEC104_IncTimers (struct iecsock *s);

void t0_timer_start( struct iecsock *s);

void t0_timer_stop( struct iecsock *s);
void t1_timer_start( struct iecsock *s);
void t1_timer_stop( struct iecsock *s);
void t2_timer_start( struct iecsock *s);
void t2_timer_stop( struct iecsock *s);
void t3_timer_start( struct iecsock *s);
void t3_timer_stop( struct iecsock *s);

int8_t t0_timer_pending ( struct iecsock *s);
int8_t t1_timer_pending ( struct iecsock *s);
int8_t t2_timer_pending ( struct iecsock *s);
int8_t t3_timer_pending ( struct iecsock *s);

void t1_timer_run(struct iecsock *s);
void t2_timer_run(struct iecsock *s);
void t3_timer_run(struct iecsock *s);

void iec104_sframe_send(struct iecsock *s);
void iec104_uframe_send(struct iecsock *s, enum uframe_func func);
/* end of New functions */

#ifdef __cplusplus
}
#endif

#endif	/* __IEC104_H */
