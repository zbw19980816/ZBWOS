#include "ZBWOS.h"
#include "usart.h"

extern unsigned char BUG;

ZBWOS_PrioTbl   ZBWOSPrioTbl[(Prio_Max-1)/32+1];   //优先级位映射表
ZBWOS_Rdy_List  RdyList[Prio_Max];   			   //就绪链表
unsigned char   ZBWOS_Runing = 0;         		   //系统运行标志
ZBWOS_TCB*      ZBWOS_TCBCurPtr = (ZBWOS_TCB*)0;   //当前运行任务TCB
ZBWOS_TCB*      ZBWOS_HightRdyPtr = (ZBWOS_TCB*)0; //就绪优先级最高任务
ZBWOS_Prio	    ZBWOSPrioCur = 0;				   //当前优先级
ZBWOS_Prio      ZBWOSPrioHighRdy = 0;              //最高优先级
ZBWOS_Tick      ZBWOSTickTime = 0;                 //系统运行时间：ZBWOSTickTime*Task_Timer(ms)
ZBWOS_Tick_List TickList[TickListSize];
ZBWOS_Sem_List  SemList;						   //暂时只支持单信号量链表

#define IdleTask_STK_Size 128
ZBWOS_STK IdleTask_STK[IdleTask_STK_Size];
ZBWOS_TCB IdleTask_TCB;
unsigned int uiCount = 0;


/***********优先级位映射表初始化************/
void ZBWOS_PrioTblInit(void)
{
	unsigned char i = (Prio_Max-1)/32+1;
	do
	{
		ZBWOSPrioTbl[i-1] = 0;	
	}while (--i);
}


/***********优先级位映射表清除************/
void ZBWOS_PrioRemove(ZBWOS_Prio ucPrio)
{
	unsigned char ucArrayNum = 0;
	unsigned char ucArrayBit = 0;
	ucArrayNum = ucPrio/32;
	ucArrayBit = ucPrio & 0x1F;
	ZBWOSPrioTbl[ucArrayNum] &= (~(0x01 << (0x1F-ucArrayBit)));
}


/*************暂时提升优先级*************/
ZBWOS_PromotPrio(ZBWOS_Sem* sem, ZBWOS_TCB* tcb)
{
	if(sem->TCB->Prio > ZBWOS_TCBCurPtr->Prio)
	{
		sem->TCB->PrioRem = sem->TCB->Prio;
		sem->TCB->Prio = ZBWOS_TCBCurPtr->Prio;	
	}
}


/***********优先级位映射表置位************/
void ZBWOS_PrioSet(ZBWOS_Prio ucPrio)
{
	unsigned char ucArrayNum = 0;
	unsigned char ucArrayBit = 0;
	ucArrayNum = ucPrio/32;
	ucArrayBit = ucPrio & 0x1F;
	ZBWOSPrioTbl[ucArrayNum] |= (0x01 << (0x1F-ucArrayBit));
}


/***********就绪链表初始化*************/
void ZBWOS_RdyListInit(void)    
{
	ZBWOS_Rdy_List *point;
	unsigned char i = 0; 
	for(point = RdyList; i < Prio_Max; point++, i++)
	{
		point->HeadPoint = (ZBWOS_TCB*)0;
		point->TailPoint = (ZBWOS_TCB*)0;	
		point->TaskNum   = 0;
	}
}


/**************就绪列表头部插入节点**************/
void ZBWOS_RdyListHeadInsert(ZBWOS_TCB* TCB)
{
	if(RdyList[TCB->Prio].TaskNum == 0)
	{
		RdyList[TCB->Prio].HeadPoint = TCB;
		RdyList[TCB->Prio].TailPoint = TCB;
		RdyList[TCB->Prio].TaskNum += 1;
		TCB->NextPtr = (ZBWOS_TCB*)0;
		TCB->PrevPtr = (ZBWOS_TCB*)0;
	}
	else
	{
		TCB->NextPtr = RdyList[TCB->Prio].HeadPoint;
		TCB->PrevPtr = (ZBWOS_TCB*)0;
		RdyList[TCB->Prio].HeadPoint->PrevPtr = TCB;
		RdyList[TCB->Prio].HeadPoint = TCB;
		RdyList[TCB->Prio].TaskNum += 1;
	}
	TCB->TaskState = ZBWOS_RdyState;
}


/**************就绪列表尾部插入节点**************/
void ZBWOS_RdyListTailInsert(ZBWOS_TCB* TCB)
{
	if(RdyList[TCB->Prio].TaskNum == 0)
	{
		RdyList[TCB->Prio].HeadPoint = TCB;
		RdyList[TCB->Prio].TailPoint = TCB;
		RdyList[TCB->Prio].TaskNum += 1;
		TCB->NextPtr = (ZBWOS_TCB*)0;
		TCB->PrevPtr = (ZBWOS_TCB*)0;
	}
	else
	{
		TCB->NextPtr = (ZBWOS_TCB*)0;
		TCB->PrevPtr = RdyList[TCB->Prio].TailPoint;
		RdyList[TCB->Prio].TailPoint->NextPtr = TCB;
		RdyList[TCB->Prio].TailPoint = TCB;
		RdyList[TCB->Prio].TaskNum += 1;
	}
	TCB->TaskState = ZBWOS_RdyState;
}


/**************就绪列表插入任务TCB？？？？？？？？？？**************/
void ZBWOS_RdyListInsert(ZBWOS_TCB* TCB)
{
	ZBWOS_PrioSet(TCB->Prio);
	if(TCB->Prio == ZBWOSPrioCur)   //如果是当前优先级则插入到链表尾部??????
	{
		ZBWOS_RdyListTailInsert(TCB);
	}
	else
	{
		ZBWOS_RdyListHeadInsert(TCB);
	}
}


/**************就绪列表头部节点移至尾部**************/
void ZBWOS_RdyListHeadToTail(ZBWOS_Rdy_List* List)
{
	if(List->TaskNum >= 2)
	{
		List->HeadPoint->PrevPtr = List->TailPoint;  //原头节点前指针指向原尾结点
		List->TailPoint->NextPtr = List->HeadPoint;	 //原尾结点后指针指向原头结点
		List->TailPoint = List->HeadPoint;			 //链表尾指针指向原头结点
		List->HeadPoint->NextPtr->PrevPtr = (ZBWOS_TCB*)0;	 //原第2个节点前指针为空
		List->HeadPoint = List->HeadPoint->NextPtr;  //链表头指针指向原第2个结点
		List->TailPoint->NextPtr = (ZBWOS_TCB*)0;			 //原头节点后指针为空
	}
}


/**************就绪列表移除节点**************/
void ZBWOS_RdyListRemove(ZBWOS_TCB* TCB)
{
	if(RdyList[TCB->Prio].TaskNum == 1)
	{
		RdyList[TCB->Prio].TaskNum = 0;
		RdyList[TCB->Prio].HeadPoint = (ZBWOS_TCB*)0;
		RdyList[TCB->Prio].TailPoint = (ZBWOS_TCB*)0;
		ZBWOS_PrioRemove(TCB->Prio);
	}
	else if(RdyList[TCB->Prio].TaskNum >= 2)
	{
		if(RdyList[TCB->Prio].HeadPoint == TCB)  //如果是头结点
		{
			RdyList[TCB->Prio].HeadPoint = RdyList[TCB->Prio].HeadPoint->NextPtr;
			RdyList[TCB->Prio].HeadPoint->PrevPtr = (ZBWOS_TCB*)0;
			TCB->NextPtr = (ZBWOS_TCB*)0;
		}
		else if(RdyList[TCB->Prio].TailPoint == TCB)  //如果是尾结点
		{
			RdyList[TCB->Prio].TailPoint = RdyList[TCB->Prio].TailPoint->PrevPtr;
			RdyList[TCB->Prio].TailPoint->NextPtr = (ZBWOS_TCB*)0;
			TCB->PrevPtr = (ZBWOS_TCB*)0;
		}
		else
		{
			TCB->NextPtr->PrevPtr = TCB->PrevPtr;
			TCB->PrevPtr->NextPtr = TCB->NextPtr;
			TCB->NextPtr = (ZBWOS_TCB*)0;
			TCB->PrevPtr = (ZBWOS_TCB*)0;
		}
	}
}


/***********获取最高优先级************/
unsigned char ZBWOS_GetHighPrio(void)
{
	unsigned char i = 0;
	ZBWOS_Prio HighPrio = 0;
	while(ZBWOSPrioTbl[i] == 0 && i < ((Prio_Max-1)/32+1))
	{
		i++;
		HighPrio += 32;
	}
	HighPrio += LeadZeros(ZBWOSPrioTbl[i]);    //计算前导0
	return HighPrio;
}


/***********时基列表初始化*************/
void ZBWOS_TickListInit(void)
{
	ZBWOS_Tick_List *point;
	unsigned char i = 0; 
	for(point = TickList; i < TickListSize; point++, i++)
	{
		point->TCBPtr  = (ZBWOS_TCB*)0;
		point->TaskNum = 0;
	}
}


/***********时基列表插入节点*************/
void ZBWOS_TickListInsert(ZBWOS_TCB* tcb, ZBWOS_Tick time) 
{
	unsigned char ListNum = 0; 
	unsigned char i = 0;
	ZBWOS_Tick_List* TickListptr;
	ZBWOS_TCB* TCB_Ptr;
	tcb->SysTime    = time + ZBWOSTickTime;
	ListNum         = tcb->SysTime % TickListSize;
	TickListptr     = &TickList[ListNum];
	tcb->TickListPtr 	 = TickListptr;
	if(BUG>=5){printf("666666\r\n");}
	if(tcb->SemTime)
	{
		tcb->TaskState = ZBWOS_SemState;
	}
	else
	{
		tcb->TaskState = ZBWOS_TickState;
	}
	if(BUG>=5){printf("777777\r\n");}
	if(TickListptr->TaskNum == 0)
	{
		tcb->TickNextPtr          = (ZBWOS_TCB*)0;
		tcb->TickPrevPtr          = (ZBWOS_TCB*)0;
		TickListptr->TCBPtr  = tcb;
		if(BUG>=5){printf("D1\r\n");}
	}
	else
	{
		
		if(BUG>=5){printf("D2\r\n");}
		TCB_Ptr = TickListptr->TCBPtr;
		for(i = 0 ; i < TickListptr->TaskNum ; i++)
		{
			if(TCB_Ptr->SysTime <= tcb->SysTime) 
			{
				if(BUG>=5){printf("D3\r\n");}
				if(TCB_Ptr->TickNextPtr == (ZBWOS_TCB*)0)   //最后一个节点
				{
					if(BUG>=5){printf("D4\r\n");}
					tcb->TickNextPtr 	 = (ZBWOS_TCB*)0;
					tcb->TickPrevPtr	 = TCB_Ptr;
					TCB_Ptr->TickNextPtr = tcb;
					break;
				}
				else
				{
					if(BUG>=5){printf("D5\r\n");}
					TCB_Ptr = TCB_Ptr->TickNextPtr;
				}
			}
			else
			{
				if(BUG>=5){printf("D6\r\n");}
				tcb->TickNextPtr 			   	  = TCB_Ptr;
				if(BUG>=5){printf(".\r\n");}
				tcb->TickPrevPtr				  = TCB_Ptr->TickPrevPtr;
				if(BUG>=5){printf("..\r\n");}
				TCB_Ptr->TickPrevPtr->TickNextPtr = tcb; /////
				printf("%d", (unsigned int)TCB_Ptr->TickPrevPtr->TickNextPtr);
				if(BUG>=5){printf("...\r\n");}
				TCB_Ptr->TickPrevPtr  			  = tcb;
				if(BUG>=5){printf("....\r\n");}
				if(i == 0)
				{
					if(BUG>=5){printf("D7\r\n");}
					if(BUG>=5){printf("D7\r\n");}
					TickListptr->TCBPtr  = tcb;
				}
				break;
			}
		}
	}
	if(BUG>=5){printf("D8\r\n");}
	TickListptr->TaskNum ++;
}


/***********时基列表删除节点*************/
void ZBWOS_TickListRemove(ZBWOS_TCB* tcb)
{
	ZBWOS_Tick_List* ListPtr;
	ListPtr = tcb->TickListPtr;
	if(ListPtr == (ZBWOS_Tick_List*)0)
	{
		return;
	}
	if(ListPtr->TCBPtr == tcb)
	{
		if(ListPtr->TaskNum > 1)
		{
			tcb->TickNextPtr->TickPrevPtr = (ZBWOS_TCB*)0;
			ListPtr->TCBPtr               = tcb->TickNextPtr; 
			tcb->TickNextPtr			  = (ZBWOS_TCB*)0;
		}
		else
		{
			ListPtr->TCBPtr  = (ZBWOS_TCB*)0;
		}
	}
	else
	{
		if(tcb->NextPtr == (ZBWOS_TCB*)0)
		{
			tcb->TickPrevPtr->TickNextPtr = (ZBWOS_TCB*)0;
			tcb->TickPrevPtr              = (ZBWOS_TCB*)0;
		}
		else
		{
			tcb->TickNextPtr->TickPrevPtr = tcb->TickPrevPtr;
			tcb->TickPrevPtr->TickNextPtr = tcb->TickNextPtr;
			tcb->TickPrevPtr              = (ZBWOS_TCB*)0;
			tcb->TickNextPtr              = (ZBWOS_TCB*)0;
		}
	}
	ListPtr->TaskNum--;
	tcb->TickListPtr = (ZBWOS_Tick_List*)0;
}


/***********时基列表更新*************/
void ZBWOS_TickListUpdate(void)
{
	ZBWOS_TCB* TCBPtr;
	unsigned char CRITICAL_Flag;
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL(); 
	ZBWOSTickTime ++;
	TCBPtr = TickList[ZBWOSTickTime % TickListSize].TCBPtr;
	while(TCBPtr != (ZBWOS_TCB*)0)
	{
		if(TCBPtr->SysTime == ZBWOSTickTime)
		{
			TCBPtr->SysTime = 0;
			if(TCBPtr->TaskState == ZBWOS_SemState)
			{
				ZBWOS_SemDelayListRemove(TCBPtr); 	
			}
			ZBWOS_TickListRemove(TCBPtr);
			ZBWOS_RdyListInsert(TCBPtr);
			TCBPtr = TickList[ZBWOSTickTime % TickListSize].TCBPtr;
		}
		else
		{
			break;
		}
	}
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);	
}


/***********时间片更新*************/
void ZBWOS_TimesliceUpdate(void)
{
	ZBWOS_TCB* TCBptr;
	unsigned char CRITICAL_Flag = 0;
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL();
	TCBptr = RdyList[ZBWOSPrioCur].HeadPoint;
	if(RdyList[ZBWOSPrioCur].TaskNum < 2)
	{
		ZBWOS_Exit_CRITICAL(CRITICAL_Flag);
		return;
	}
	if(TCBptr->timesliceRemain != 1)
	{
		TCBptr->timesliceRemain --;
		ZBWOS_Exit_CRITICAL(CRITICAL_Flag);
		return;
	}
	ZBWOS_RdyListHeadToTail(&RdyList[ZBWOSPrioCur]);
	TCBptr = RdyList[ZBWOSPrioCur].HeadPoint;
	TCBptr->timesliceRemain = TCBptr->timeslice;
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);
}


/***********信号量列表初始化*************/
void ZBWOS_SemListInit(void)
{
	SemList.TCBPtr = (ZBWOS_TCB*)0;
	SemList.SemNum = 0;
}


/***********空闲任务*************/
void IdleTask(void *parg)
{
	parg = parg;
	while(1)
		uiCount++;
}

/***********空闲任务初始化*************/
void ZBWOS_IdleTaskInit(ZBWOS_ERR *err)
{
	ZBWOSTaskCreat(IdleTask_STK, IdleTask_STK_Size, &IdleTask_TCB, IdleTask, (void*) 0,31,0,err);
}

/***********系统初始化*************/
void ZBWOSInit(ZBWOS_ERR *err)
{

	ZBWOS_Runing      = 0;
	ZBWOS_TCBCurPtr   = (ZBWOS_TCB*)0;
	ZBWOS_HightRdyPtr = (ZBWOS_TCB*)0;
	ZBWOS_RdyListInit();
	ZBWOS_TickListInit();
	ZBWOS_SemListInit();
	ZBWOS_PrioTblInit();
	ZBWOS_IdleTaskInit(err);
	*err = ZBWOS_ERR_NONE;
}


/***********堆栈初始化*************/
ZBWOS_STK *ZBWOS_StackInit(			ZBWOS_STK    		*stk,  
							  		ZBWOS_STK_Size  	stk_size,
							  		void 				(*task)(void *parg),
							 		void				*parg)
{  //堆栈初始化之后point指向0x200001e4
	ZBWOS_STK   *point;
	point 		= &stk[stk_size];							/* Load stack pointer									  */
															/* Registers stacked as if auto-saved on exception		  */
//	*--point = (ZBWOS_STK)0x01000000u;						/* xPSR 												  */
//	*--point = (ZBWOS_STK)task; 							/* Entry Point											  */
//	*--point = (ZBWOS_STK)0x14141414u;						/* R14 (LR) 											  */
//	*--point = (ZBWOS_STK)0x12121212u;						/* R12													  */
//	*--point = (ZBWOS_STK)0x03030303u;						/* R3													  */
//	*--point = (ZBWOS_STK)0x02020202u;						/* R2													  */
//	*--point = (ZBWOS_STK)0x01010101u;						/* R1													  */
//	*--point = (ZBWOS_STK)parg;								/* R0 : argument										  */
//															/* Remaining registers saved on process stack			  */
//	*--point = (ZBWOS_STK)0x11111111u;						/* R11													  */
//	*--point = (ZBWOS_STK)0x10101010u;						/* R10													  */
//	*--point = (ZBWOS_STK)0x09090909u;						/* R9													  */
//	*--point = (ZBWOS_STK)0x08080808u;						/* R8													  */
//	*--point = (ZBWOS_STK)0x07070707u;						/* R7													  */
//	*--point = (ZBWOS_STK)0x06060606u;						/* R6													  */
//	*--point = (ZBWOS_STK)0x05050505u;						/* R5													  */
//	*--point = (ZBWOS_STK)0x04040404u;						/* R4													  */
	
	*--point = (ZBWOS_STK)0x01000000u;						
	*--point = (ZBWOS_STK)task; 									
	*--point = (ZBWOS_STK)0u;					
	*--point = (ZBWOS_STK)0u;				
	*--point = (ZBWOS_STK)0u;					
	*--point = (ZBWOS_STK)0u;					
	*--point = (ZBWOS_STK)0u;					
	*--point = (ZBWOS_STK)parg;						
														
	*--point = (ZBWOS_STK)0u;				
	*--point = (ZBWOS_STK)0u;				
	*--point = (ZBWOS_STK)0u;					
	*--point = (ZBWOS_STK)0u;				
	*--point = (ZBWOS_STK)0u;				
	*--point = (ZBWOS_STK)0u;
	*--point = (ZBWOS_STK)0u;
	*--point = (ZBWOS_STK)0u;
	return (point);
}


/*****************TCB初始化****************/
void ZBWOS_TCBInit(ZBWOS_TCB* tcb)
{
	tcb->NextPtr   = (ZBWOS_TCB*)0;
	tcb->PrevPtr   = (ZBWOS_TCB*)0;
	tcb->Prio      = 0;
	tcb->STKPTR    = 0;
	tcb->STKSize   = 0;

	tcb->SysTime     = 0;  
	tcb->TickNextPtr = (ZBWOS_TCB*)0;
	tcb->TickPrevPtr = (ZBWOS_TCB*)0;
	tcb->TickListPtr = (ZBWOS_Tick_List*)0;

	tcb->timeslice       = 0;
	tcb->timesliceRemain = 0;

	tcb->SemNextPtr = (ZBWOS_TCB*)0;
	tcb->SemPrevPtr = (ZBWOS_TCB*)0;
	tcb->SemTime	= 0;
}


/**************阻塞延时**************/									
void ZBWOSDelay_Ms(ZBWOS_Tick Delay_Ms)
{
	unsigned int dly = 0;
	unsigned char CRITICAL_Flag = 0;
	if(BUG>=5){printf("111111111\r\n");}
	dly = Delay_Ms/Task_Timer;
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL(); 	//进入临界区
	if(BUG>=5){printf("222222\r\n");}
	ZBWOS_RdyListRemove(ZBWOS_TCBCurPtr);
	if(BUG>=5){printf("333333\r\n");}
	ZBWOS_TickListInsert(ZBWOS_TCBCurPtr, dly);
	if(BUG>=5){printf("44444\r\n");}
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);			//退出临界区
	if(BUG>=5){printf("555555\r\n");}
	ZBWOSSched();
}


/***************任务切换*****************/
void ZBWOSSched(void)
{
	unsigned char CRITICAL_Flag = 0;
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL(); 	//进入临界区
	ZBWOSPrioHighRdy  = ZBWOS_GetHighPrio();
	ZBWOS_HightRdyPtr = RdyList[ZBWOSPrioHighRdy].HeadPoint;
	if(ZBWOS_HightRdyPtr == ZBWOS_TCBCurPtr)
	{
		ZBWOS_Exit_CRITICAL(CRITICAL_Flag);			//退出临界区
		return;
	}
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);			//退出临界区
	ZBWOS_Task_SW();    //触发PendSV异常，切换至ZBWOS_HightRdyPtr指向的任务(往中断及状态控制寄存器 SCB_ICSR 的位 28（PendSV 异常使能位）写入 1，从而触发 PendSV 异常)
}


/******************开启ZBWOS*******************/
void ZBWOSStart(ZBWOS_ERR *err)
{
	if(ZBWOS_Runing == 0)
	{
		ZBWOSPrioHighRdy  = ZBWOS_GetHighPrio();
		ZBWOSPrioCur      = ZBWOSPrioHighRdy;
		ZBWOS_HightRdyPtr = RdyList[ZBWOSPrioHighRdy].HeadPoint;
		ZBWOS_TCBCurPtr   = ZBWOS_HightRdyPtr;
		OSStartHighRdy();
		*err              = ZBWOS_Outing_ERRA;
	}
	else 
	{
		*err = ZBWOS_Runing_ERRA;
	}
}


/********************创建任务**********************/
void ZBWOSTaskCreat(	ZBWOS_STK 				*stk,
							ZBWOS_STK_Size 		stksize,
							ZBWOS_TCB 			*tcb,
							void 				(*task)(void* parg),
							void*               parg,
							ZBWOS_Prio          prio,
							ZBWOS_timeslice     timeslice,
							ZBWOS_ERR 			*err)

{
	ZBWOS_STK 	 *stk_point;
	stk_point	 = ZBWOS_StackInit(	stk, stksize, task, parg );
	ZBWOS_TCBInit(tcb);
	tcb->STKPTR    = stk_point;
	tcb->STKSize   = stksize;
	tcb->Prio      = prio;
	tcb->timeslice = timeslice;
	tcb->timesliceRemain = timeslice;
	ZBWOS_PrioSet(tcb->Prio);         //置位相应优先级
	ZBWOS_RdyListTailInsert(tcb);     //插到就绪链表尾部
	*err           = ZBWOS_ERR_NONE; 
}


/******************挂起任务*******************/
void ZBWOSTaskSuspend(ZBWOS_TCB* tcb)
{
	unsigned char CRITICAL_Flag = 0;
	if(tcb == (ZBWOS_TCB*)0 || tcb->TaskState == ZBWOS_SpdState)
	{
		return;
	}
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL();
	if(tcb->TaskState == ZBWOS_RdyState)
	{
		ZBWOS_RdyListRemove(tcb);
		
	}
	else if(tcb->TaskState == ZBWOS_TickState)
	{
		ZBWOS_TickListRemove(tcb);
	}
	else if(tcb->TaskState == ZBWOS_SemState)
	{
		ZBWOS_RdyListRemove(tcb);
	}
	tcb->TaskState = ZBWOS_SpdState;
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);	
	ZBWOSSched();
}


/******************解挂任务*******************/
void ZBWOSTaskResume(ZBWOS_TCB* tcb)
{
	unsigned char CRITICAL_Flag = 0;
	CRITICAL_Flag = ZBWOS_Enter_CRITICAL();
	if(tcb == (ZBWOS_TCB*)0 || tcb->TaskState != ZBWOS_SpdState)
	{
		return;
	}
	if(tcb->SysTime <= ZBWOSTickTime)
	{
		ZBWOS_RdyListTailInsert(tcb);
		ZBWOS_PrioSet(tcb->Prio);
		tcb->TaskState = ZBWOS_RdyState;
	}
	else
	{
		ZBWOS_TickListInsert(tcb, ZBWOSTickTime - tcb->SysTime);
		tcb->TaskState = ZBWOS_TickState;
	}
	ZBWOS_Exit_CRITICAL(CRITICAL_Flag);	
}


/***********信号量列表插入节点*************/
void ZBWOS_SemListInsert(void) 
{
	unsigned int i = 0;
	if(SemList.SemNum == 0)
	{
		SemList.TCBPtr = ZBWOS_TCBCurPtr;
		ZBWOS_TCBCurPtr->SemNextPtr = (ZBWOS_TCB*)0;
		ZBWOS_TCBCurPtr->SemPrevPtr = (ZBWOS_TCB*)0;
	}
	else
	{	
		ZBWOS_TCB* TCB_Ptr;
		TCB_Ptr = SemList.TCBPtr;
		for(i = 0 ; i < SemList.SemNum ; i++)
		{
			if(TCB_Ptr->Prio <= ZBWOS_TCBCurPtr->SysTime) 
			{
				if(TCB_Ptr->SemNextPtr == (ZBWOS_TCB*)0)   //最后一个节点
				{
					ZBWOS_TCBCurPtr->SemNextPtr  = (ZBWOS_TCB*)0;
					ZBWOS_TCBCurPtr->SemPrevPtr	 = TCB_Ptr;
					TCB_Ptr->SemNextPtr 		 = ZBWOS_TCBCurPtr;
					break;
				}
				else
				{
					TCB_Ptr = TCB_Ptr->SemNextPtr;
				}
			}
			else
			{
				ZBWOS_TCBCurPtr->SemNextPtr 	  = TCB_Ptr;
				ZBWOS_TCBCurPtr->SemPrevPtr		  = TCB_Ptr->SemPrevPtr;
				TCB_Ptr->TickPrevPtr->SemNextPtr  = ZBWOS_TCBCurPtr;
				TCB_Ptr->TickPrevPtr  			  = ZBWOS_TCBCurPtr;
				if(i == 0)
				{
					SemList.TCBPtr = ZBWOS_TCBCurPtr;
				}
				break;
			}
		}
	}
	SemList.SemNum++;
	//ZBWOS_TCBCurPtr->SemTime = time;
}


/***********信号量列表删除节点*************/
ZBWOS_TCB* ZBWOS_SemListRemove(ZBWOS_TCB* tcb, ZBWOS_Sem* SEM) 
{
	ZBWOS_TCB* ptr;
	ptr = SemList.TCBPtr;
	if(tcb == (ZBWOS_TCB*)0 || SemList.TCBPtr == tcb)
	{
		SemList.TCBPtr->SemNextPtr->SemPrevPtr = (ZBWOS_TCB*)0;
		SemList.TCBPtr = SemList.TCBPtr->SemNextPtr;
		ptr->SemNextPtr = (ZBWOS_TCB*)0;
	}
	else
	{
		tcb->SemNextPtr->SemPrevPtr = tcb->SemPrevPtr;
		tcb->SemPrevPtr->SemNextPtr = tcb->SemNextPtr;
		tcb->SemNextPtr = (ZBWOS_TCB*)0;
		tcb->SemPrevPtr = (ZBWOS_TCB*)0;
	}
	SemList.SemNum --;
	SEM->SemNum --;
	return ptr;
}


/***********延时到时信号量列表删除节点*************/
void ZBWOS_SemDelayListRemove(ZBWOS_TCB* tcb)
{
	if(SemList.TCBPtr == tcb)
	{
		SemList.TCBPtr = tcb->SemNextPtr;
	}
	tcb->SemNextPtr->SemPrevPtr = tcb->SemPrevPtr;
	tcb->SemPrevPtr->SemNextPtr = tcb->SemNextPtr;
	tcb->SemNextPtr = (ZBWOS_TCB*)0;
	tcb->SemPrevPtr = (ZBWOS_TCB*)0;
	tcb->SemTime    = 0;
	SemList.SemNum --;
}


/******************信号量初始化*****************/
void ZBWOS_SemCreat(ZBWOS_Sem* sem, ZBWOS_SemNum num)
{
	sem->SemNum = num;
}


/******************信号量申请*****************/
void ZBWOSSemGet(ZBWOS_Sem* sem, ZBWOS_Tick time)
{
	if(sem->SemNum > 0)
	{
		sem->SemNum --;
		sem->TCB = ZBWOS_TCBCurPtr;
		return;
	}
	if(time)
	{
		ZBWOS_SemListInsert();
		ZBWOS_PromotPrio(sem, ZBWOS_TCBCurPtr);
		ZBWOS_TCBCurPtr->SemTime = time;
		ZBWOSDelay_Ms(time * Task_Timer);
	}
	else
	{
		ZBWOS_SemListInsert();
		ZBWOS_PromotPrio(sem, ZBWOS_TCBCurPtr);
		ZBWOSTaskSuspend(ZBWOS_TCBCurPtr);  //请求不到信号量，将任务挂起
	}
}


/******************信号量发送*****************/
void ZBWOSSemPost(ZBWOS_Sem* sem)
{
	sem->SemNum ++;
	//sem->TCB = (ZBWOS_TCB*)0;
	if(SemList.SemNum)
	{
		ZBWOS_TCB* point;
		point = (ZBWOS_SemListRemove((ZBWOS_TCB*)0, sem));
		if(point->PrioRem)
		{
			point->Prio	   = point->PrioRem;
			point->PrioRem = 0;
		}
		point->SemTime = 0;
		if(point->TaskState == ZBWOS_SpdState)
		{
			ZBWOS_RdyListTailInsert(point);
			ZBWOS_PrioSet(point->Prio);
			point->TaskState = ZBWOS_RdyState;
		}
		else if(point->TaskState == ZBWOS_SemState)
		{
			ZBWOS_TickListRemove(point);
			ZBWOS_RdyListInsert(point);
			point->TaskState = ZBWOS_RdyState;
		}
	}
}


/*传入参数array[5][100]*/
void memcreat(void *base_addr, int block, int length)
{
	unsigned int i;
	void **pptr;
	char *ptr;
	pptr = (void **)base_addr;
	ptr = (char *)base_addr;
	for(i = 0; i < block; i ++)
	{
		ptr += length;
		*pptr = (void *)ptr;
		pptr = (void **)(void *)ptr;
	}
}


/********************时基初始化**********************/
void ZBWOS_CPU_SysTickInit (unsigned int ms)
{
	SysTick_RELOAD = ms * 9000 - 1;            					  //设置重装载寄存器的值  :  ms * 9000000 / 1000 - 1; 
	//NVIC_SetPriority (SysTick_IRQn, (1<<__NVIC_PRIO_BITS) - 1);   //无法直接配置抢占和子优先级，通过第二个形参传递4位优先级值，配置中断优先级为最低
	SysTick_PRIO |= 0x0F;
	SysTick_CTRL &= 0xFB;										  //使用外部时钟    频率：72/8 M
	SysTick_CTRL |= 0x01;  										  //使能 SysTick 的时钟源和启动计数器 
	SysTick_CTRL |= 0x02;  										  //使能 SysTick 的定时中断
}


/*********************时基中断***********************/
void SysTick_Handler(void)
{

	ZBWOS_TickListUpdate();
	ZBWOS_TimesliceUpdate();
	ZBWOSSched();
} 
