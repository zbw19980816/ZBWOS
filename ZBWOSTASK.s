 IMPORT ZBWOS_TCBCurPtr 
 IMPORT ZBWOS_HightRdyPtr
 IMPORT ZBWOSPrioHighRdy
 IMPORT ZBWOSPrioCur
 
 EXPORT OSStartHighRdy
 EXPORT PendSV_Handler
 EXPORT ZBWOS_Enter_CRITICAL
 EXPORT ZBWOS_Exit_CRITICAL
 EXPORT LeadZeros
NVIC_INT_CTRL   EQU     0xE000ED04  ; 中断控制寄存器       (内核外设)   ;;;;中断控制及状态寄存器 SCB_ICSR。
NVIC_SYSPRI14   EQU     0xE000ED22  ; 系统优先级寄存器(14)      ;;;;系统优先级寄存器 SCB_SHPR3：
NVIC_PENDSV_PRI EQU     	  0xFF  ; PendSV中断和系统节拍中断;;;;PendSV 优先级的值(最低)。
NVIC_PENDSVSET  EQU     0x10000000  ; 触发软件中断的值.;;;;触发 PendSV 异常的值 Bit28： PENDSVSET 

;*******************************************************************
; 							开始第一次上下文切换
; 1、配置 PendSV 异常的优先级为最低
; 2、在开始第一次上下文切换之前，设置 psp=0
; 3、触发 PendSV 异常，开始上下文切换
;*******************************************************************

 PRESERVE8
 THUMB
 
 AREA CODE, CODE, READONLY

OSStartHighRdy

	LDR R0, = NVIC_SYSPRI14         ; 设置 PendSV 异常优先级为最低 (1)
	LDR R1, = NVIC_PENDSV_PRI
	STRB R1, [R0]                   ;将R1中低8位数据放到[R0]内存处

	MOVS R0, #0 					;设置 psp 的值为 0，开始第一次上下文切换 (2)
	MSR PSP, R0

	LDR R0, =NVIC_INT_CTRL 			; 触发 PendSV 异常 (3)
	LDR R1, =NVIC_PENDSVSET
	STR R1, [R0]

	CPSIE I 						; 使能总中断， NMI 和 HardFault 除外 (4)

OSStartHang
	B OSStartHang 					; 程序应永远不会运行到这里


;***********************************************************************
; PendSVHandler 异常
;***********************************************************************

PendSV_Handler

; 关中断， NMI 和 HardFault 除外，防止上下文切换被中断
	CPSID I ;(1)
	
; 将 psp 的值加载到 R0
	MRS R0, PSP ;(2)

; 判断 R0，如果值为 0 则跳转到 OS_CPU_PendSVHandler_nosave
; 进行第一次任务切换的时候， R0 肯定为 0
	CBZ R0, OS_CPU_PendSVHandler_nosave ;(3)

;-----------------------一、保存上文-----------------------------
; 任务的切换，即把下一个要运行的任务的堆栈内容加载到 CPU 寄存器中
;--------------------------------------------------------------
; 在进入 PendSV 异常的时候，当前 CPU 的 xPSR， PC（任务入口地址），
; R14， R12， R3， R2， R1， R0 会自动存储到当前任务堆栈，
; 同时递减 PSP 的值，随便通过 代码： MRS R0, PSP 把 PSP 的值传给 R0

; 手动存储 CPU 寄存器 R4-R11 的值到当前任务的堆栈
	STMDB R0!, {R4-R11} ;(15)
	
; 加载 OSTCBCurPtr 指针的地址到 R1，这里 LDR 属于伪指令
	LDR R1, = ZBWOS_TCBCurPtr ;(16)
; 加载 OSTCBCurPtr 指针到 R1，这里 LDR 属于 ARM 指令
	LDR R1, [R1] ;(17)
; 存储 R0 的值到 OSTCBCurPtr->OSTCBStkPtr，这个时候 R0 存的是任务空闲栈的栈顶
	STR R0, [R1] ;(18)

;-----------------------二、切换下文-----------------------------
; 实现 OSTCBCurPtr = OSTCBHighRdyPtr
; 把下一个要运行的任务的堆栈内容加载到 CPU 寄存器中
;--------------------------------------------------------------
OS_CPU_PendSVHandler_nosave ;(4)

	LDR R0, =ZBWOSPrioCur
	LDR R1, =ZBWOSPrioHighRdy
	LDRB R2, [R1]
	STRB R2, [R0]

; 加载 OSTCBCurPtr 指针的地址到 R0，这里 LDR 属于伪指令
	LDR R0, = ZBWOS_TCBCurPtr  ;(5)
; 加载 OSTCBHighRdyPtr 指针的地址到 R1，这里 LDR 属于伪指令
	LDR R1, = ZBWOS_HightRdyPtr;(6)
; 加载 OSTCBHighRdyPtr 指针到 R2，这里 LDR 属于 ARM 指令
	LDR R2, [R1] ;(7) ;取TCB控制块的地址
; 存储 OSTCBHighRdyPtr 到 OSTCBCurPtr
	STR R2, [R0] ;(8)
	
; 加载 OSTCBHighRdyPtr 到 R0
 	LDR R0, [R2] ;(9)
; 加载需要手动保存的信息到 CPU 寄存器 R4-R11
	LDMIA R0!, {R4-R11} ;(10)
	
; 更新 PSP 的值，这个时候 PSP 指向下一个要执行的任务的堆栈的栈底
;（这个栈底已经加上刚刚手动加载到 CPU 寄存器 R4-R11 的偏移）
	MSR PSP, R0 ;(11)
	
; 确保异常返回使用的堆栈指针是 PSP，即 LR 寄存器的位 2 要为 1
	ORR LR, LR, #0x04 ;(12)
	
; 开中断
	CPSIE I ;(13)
	
; 异常返回，这个时候任务堆栈中的剩下内容将会自动加载到 xPSR，
; PC（任务入口地址）， R14， R12， R3， R2， R1， R0（任务的形参）
; 同时 PSP 的值也将更新，即指向任务堆栈的栈顶。
; 在 STM32 中，堆栈是由高地址向低地址生长的。
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
