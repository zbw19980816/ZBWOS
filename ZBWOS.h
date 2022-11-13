#ifndef __ZBWOS_H
#define __ZBWOS_H	 

#define Prio_Max 32     //优先级最大值
#define Task_Timer 10   //任务时间片 10ms
#define TickListSize 10 //时基链表最多插入任务数量
//#define SemListSize  10 //信号量链表最多插入任务数量

#define NVIC_INT_CTRL *((unsigned int *)0xE000ED04)  //中断控制及状态寄存器 SCB_ICSR
#define NVIC_PENDSVSET 0x10000000                    //触发 PendSV 异常的值 Bit28： PENDSVSET
#define ZBWOS_Task_SW() NVIC_INT_CTRL=NVIC_PENDSVSET //触发 PendSV 异常


#define SysTick_RELOAD *((unsigned int*)0xE000E014)
#define SysTick_CTRL   *((unsigned int*)0xE000E010)
#define SysTick_PRIO   *((unsigned int*)0xE000ED23)

/////////////////////
/*    类型声明         */
/////////////////////

typedef unsigned int  ZBWOS_STK;  	
typedef unsigned int  ZBWOS_STK_Size;
typedef unsigned int  ZBWOS_Tick;
typedef unsigned int  ZBWOS_PrioTbl;
typedef unsigned char ZBWOS_Prio;
typedef unsigned char ZBWOS_timeslice;
typedef unsigned int ZBWOS_SemNum;

typedef struct ZBWOS_tcb ZBWOS_TCB;


typedef enum{
	ZBWOS_ERR_NONE = 0u,
	ZBWOS_Runing_ERRA  = 10000u,
	ZBWOS_Outing_ERRA  = 10001u
}ZBWOS_ERR;

typedef enum{
	ZBWOS_SpdState  = 0u,    //挂起
	ZBWOS_RdyState  = 1u,    //就绪
	ZBWOS_TickState = 2u,    //延时
	ZBWOS_SemState  = 3u     //等待信号量产生延时
}ZBWOS_TaskState;


typedef struct{
	ZBWOS_TCB 	 *TCBPtr;
	unsigned int TaskNum;
}ZBWOS_Tick_List;


typedef struct{
	ZBWOS_TCB 	 *TCBPtr;
	unsigned int SemNum;
}ZBWOS_Sem_List;            //等待信号量列表


struct ZBWOS_tcb{
	ZBWOS_STK*     	 STKPTR;
	ZBWOS_STK_Size 	 STKSize;
	ZBWOS_Prio     	 Prio;
	ZBWOS_Prio		 PrioRem;
	ZBWOS_TCB*     	 NextPtr;
	ZBWOS_TCB*     	 PrevPtr;

	ZBWOS_Tick 	 	 SysTime;       //延时到时的系统时间
	ZBWOS_TCB*    	 TickNextPtr;
	ZBWOS_TCB*     	 TickPrevPtr;
	ZBWOS_Tick_List* TickListPtr;

	ZBWOS_timeslice  timeslice;
	ZBWOS_timeslice  timesliceRemain;

	ZBWOS_TaskState  TaskState;

	ZBWOS_TCB*    	 SemNextPtr;
	ZBWOS_TCB*     	 SemPrevPtr;
	ZBWOS_Tick 	 	 SemTime;
};


typedef struct{
	ZBWOS_TCB 	 *HeadPoint;
	ZBWOS_TCB 	 *TailPoint;
	unsigned int TaskNum;    //记录当前优先级任务数量
}ZBWOS_Rdy_List;


typedef struct{
	ZBWOS_SemNum  SemNum;
	ZBWOS_TCB*    TCB;
	//unsigned char TCBNum;
}ZBWOS_Sem;


/////////////////////////
/*       api函数         */
/////////////////////////

void ZBWOSTaskCreat(ZBWOS_STK 				*stk,
						ZBWOS_STK_Size 		stksize,
						ZBWOS_TCB 			*tcb,
						void 				(*task)(void* parg),
						void*               parg,
						ZBWOS_Prio          prio,
						ZBWOS_timeslice     timeslice,
						ZBWOS_ERR 			*err);

void ZBWOSInit(ZBWOS_ERR *err); 
void OSStartHighRdy(void);
void ZBWOSStart(ZBWOS_ERR *err);
void ZBWOSSched(void);
void ZBWOS_CPU_SysTickInit(unsigned int time);
void ZBWOSDelay_Ms(ZBWOS_Tick Delay_Ms);
unsigned char ZBWOS_Enter_CRITICAL(void);
void ZBWOS_Exit_CRITICAL(unsigned char a);
unsigned char LeadZeros(unsigned int a);
void ZBWOSTaskSuspend(ZBWOS_TCB* tcb);
void ZBWOSTaskResume(ZBWOS_TCB* tcb);
void ZBWOS_SemCreat(ZBWOS_Sem* sem, unsigned int num);
void ZBWOSSemPost(ZBWOS_Sem* sem);
void ZBWOSSemGet(ZBWOS_Sem* sem, ZBWOS_Tick time);

void ZBWOS_SemDelayListRemove(ZBWOS_TCB* tcb);
#endif
