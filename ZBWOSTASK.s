 IMPORT ZBWOS_TCBCurPtr 
 IMPORT ZBWOS_HightRdyPtr
 IMPORT ZBWOSPrioHighRdy
 IMPORT ZBWOSPrioCur
 
 EXPORT OSStartHighRdy
 EXPORT PendSV_Handler
 EXPORT ZBWOS_Enter_CRITICAL
 EXPORT ZBWOS_Exit_CRITICAL
 EXPORT LeadZeros
NVIC_INT_CTRL   EQU     0xE000ED04  ; �жϿ��ƼĴ���       (�ں�����)   ;;;;�жϿ��Ƽ�״̬�Ĵ��� SCB_ICSR��
NVIC_SYSPRI14   EQU     0xE000ED22  ; ϵͳ���ȼ��Ĵ���(14)      ;;;;ϵͳ���ȼ��Ĵ��� SCB_SHPR3��
NVIC_PENDSV_PRI EQU     	  0xFF  ; PendSV�жϺ�ϵͳ�����ж�;;;;PendSV ���ȼ���ֵ(���)��
NVIC_PENDSVSET  EQU     0x10000000  ; ��������жϵ�ֵ.;;;;���� PendSV �쳣��ֵ Bit28�� PENDSVSET 

;*******************************************************************
; 							��ʼ��һ���������л�
; 1������ PendSV �쳣�����ȼ�Ϊ���
; 2���ڿ�ʼ��һ���������л�֮ǰ������ psp=0
; 3������ PendSV �쳣����ʼ�������л�
;*******************************************************************

 PRESERVE8
 THUMB
 
 AREA CODE, CODE, READONLY

OSStartHighRdy

	LDR R0, = NVIC_SYSPRI14         ; ���� PendSV �쳣���ȼ�Ϊ��� (1)
	LDR R1, = NVIC_PENDSV_PRI
	STRB R1, [R0]                   ;��R1�е�8λ���ݷŵ�[R0]�ڴ洦

	MOVS R0, #0 					;���� psp ��ֵΪ 0����ʼ��һ���������л� (2)
	MSR PSP, R0

	LDR R0, =NVIC_INT_CTRL 			; ���� PendSV �쳣 (3)
	LDR R1, =NVIC_PENDSVSET
	STR R1, [R0]

	CPSIE I 						; ʹ�����жϣ� NMI �� HardFault ���� (4)

OSStartHang
	B OSStartHang 					; ����Ӧ��Զ�������е�����


;***********************************************************************
; PendSVHandler �쳣
;***********************************************************************

PendSV_Handler

; ���жϣ� NMI �� HardFault ���⣬��ֹ�������л����ж�
	CPSID I ;(1)
	
; �� psp ��ֵ���ص� R0
	MRS R0, PSP ;(2)

; �ж� R0�����ֵΪ 0 ����ת�� OS_CPU_PendSVHandler_nosave
; ���е�һ�������л���ʱ�� R0 �϶�Ϊ 0
	CBZ R0, OS_CPU_PendSVHandler_nosave ;(3)

;-----------------------һ����������-----------------------------
; ������л���������һ��Ҫ���е�����Ķ�ջ���ݼ��ص� CPU �Ĵ�����
;--------------------------------------------------------------
; �ڽ��� PendSV �쳣��ʱ�򣬵�ǰ CPU �� xPSR�� PC��������ڵ�ַ����
; R14�� R12�� R3�� R2�� R1�� R0 ���Զ��洢����ǰ�����ջ��
; ͬʱ�ݼ� PSP ��ֵ�����ͨ�� ���룺 MRS R0, PSP �� PSP ��ֵ���� R0

; �ֶ��洢 CPU �Ĵ��� R4-R11 ��ֵ����ǰ����Ķ�ջ
	STMDB R0!, {R4-R11} ;(15)
	
; ���� OSTCBCurPtr ָ��ĵ�ַ�� R1������ LDR ����αָ��
	LDR R1, = ZBWOS_TCBCurPtr ;(16)
; ���� OSTCBCurPtr ָ�뵽 R1������ LDR ���� ARM ָ��
	LDR R1, [R1] ;(17)
; �洢 R0 ��ֵ�� OSTCBCurPtr->OSTCBStkPtr�����ʱ�� R0 ������������ջ��ջ��
	STR R0, [R1] ;(18)

;-----------------------�����л�����-----------------------------
; ʵ�� OSTCBCurPtr = OSTCBHighRdyPtr
; ����һ��Ҫ���е�����Ķ�ջ���ݼ��ص� CPU �Ĵ�����
;--------------------------------------------------------------
OS_CPU_PendSVHandler_nosave ;(4)

	LDR R0, =ZBWOSPrioCur
	LDR R1, =ZBWOSPrioHighRdy
	LDRB R2, [R1]
	STRB R2, [R0]

; ���� OSTCBCurPtr ָ��ĵ�ַ�� R0������ LDR ����αָ��
	LDR R0, = ZBWOS_TCBCurPtr  ;(5)
; ���� OSTCBHighRdyPtr ָ��ĵ�ַ�� R1������ LDR ����αָ��
	LDR R1, = ZBWOS_HightRdyPtr;(6)
; ���� OSTCBHighRdyPtr ָ�뵽 R2������ LDR ���� ARM ָ��
	LDR R2, [R1] ;(7) ;ȡTCB���ƿ�ĵ�ַ
; �洢 OSTCBHighRdyPtr �� OSTCBCurPtr
	STR R2, [R0] ;(8)
	
; ���� OSTCBHighRdyPtr �� R0
 	LDR R0, [R2] ;(9)
; ������Ҫ�ֶ��������Ϣ�� CPU �Ĵ��� R4-R11
	LDMIA R0!, {R4-R11} ;(10)
	
; ���� PSP ��ֵ�����ʱ�� PSP ָ����һ��Ҫִ�е�����Ķ�ջ��ջ��
;�����ջ���Ѿ����ϸո��ֶ����ص� CPU �Ĵ��� R4-R11 ��ƫ�ƣ�
	MSR PSP, R0 ;(11)
	
; ȷ���쳣����ʹ�õĶ�ջָ���� PSP���� LR �Ĵ�����λ 2 ҪΪ 1
	ORR LR, LR, #0x04 ;(12)
	
; ���ж�
	CPSIE I ;(13)
	
; �쳣���أ����ʱ�������ջ�е�ʣ�����ݽ����Զ����ص� xPSR��
; PC��������ڵ�ַ���� R14�� R12�� R3�� R2�� R1�� R0��������βΣ�
; ͬʱ PSP ��ֵҲ�����£���ָ�������ջ��ջ����
; �� STM32 �У���ջ���ɸߵ�ַ��͵�ַ�����ġ�
	BX LR ;(14)
	
ZBWOS_Enter_CRITICAL
 	MRS R0,PRIMASK
	CPSID I
 	BX LR

ZBWOS_Exit_CRITICAL
 	MSR PRIMASK,R0
 	BX LR

LeadZeros
    CLZ R0,R0
    BX LR

	NOP
    END
