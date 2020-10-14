/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (C) 2012 - 2018 Xilinx, Inc. All Rights Reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. If you wish to use our Amazon
    FreeRTOS name, please do so in a fair use way that does not cause confusion.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/
/*
 * time_slice_sechedule.c
 *
 *  Created on: 	13 October 2020
 *      Author: 	Leomar Duran
 *     Version:		1.2
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************
*	v1.2 - 13 October 2020
*		Finished using for loops and priority context switching.
*
*	v1.1 - 13 October 2020
*		Attempted to solve using semaphores.
*
*	v1.0 - 13 October 2020
*		First version created.
*******************************************************************************************/
/*
 * Each task T(n+1) activates LED LDn for 5 seconds, then deactivates
 * it, and performs task handling operations as follows:
 *
 * T1-P4
 *     after running 2x, suspend
 * T2-P4
 *     after running 4x, delay until 60 sec
 *     after running 5x, priority 2
 * T3-P3
 *     after running 4x, create task4
 *     after running 6x, resume T1
 *     after running 6x + 3, delay 20 sec
 *     after running 8x, delete T2
 * T4-P1
 *     after running 3x, yield
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
/* semaphore include */
#define	configUSE_MUTEXES	1
#include	"semphr.h"
/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"

/* Delay definitions */
#define DELAY_200_MS	 	  	200UL
/* for loop 12 * 406919936UL = 60.04 sec, 12 * 406309962UL = 59.95 sec */
#define FOR_DELAY_05_SECONDS	406309962UL
#define DELAY_20_SECONDS		20000UL
#define DELAY_60_SECONDS		60000UL
/*-----------------------------------------------------------*/

/* Task mile stones */
#define configMAX_PRIORITIES	(tskIDLE_PRIORITY + 5)	/* higher priority than any task */
#define	T1_SUSPEND			2	/* suspend after 2 turns = 20 sec */
#define T2_DELAY_UNTIL		4	/* delay until 60 seconds after 4 turns = 45 sec */
#define T2_P2				5	/* go priority 2 after 5 turns = 65 sec */
#define T3_T4				4	/* create task 4 after 4 turns = 50 sec */
#define T3_RESUME_T1		6	/* resume task 1 after 6 turns = 75 sec */
#define T3_START_DELAYS		6	/* start delaying after 6 turns = 75 sec */
#define T3_DELAY			3	/* delay every 3 turns after T3_START_DELAYS */
#define T3_DELETE_T2		8	/* delete T2 after 8 turns = 110 sec */
#define T4_YIELD			3	/* yield after 3 turns = 90 sec */
/*-----------------------------------------------------------*/

/* GPIO definitions */
#define GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define LED_CHANNEL 1								/* GPIO port for LEDs */
/*-----------------------------------------------------------*/

/* Callback functions for the tasks, as described at the top of this
file. */
static void prvT1( void *pvParameters );
static void prvT2( void *pvParameters );
static void prvT3( void *pvParameters );
static void prvT4( void *pvParameters );
/*-----------------------------------------------------------*/

/* Task handles for the tasks, as described at the top of this
file. */
static TaskHandle_t xT1;
static TaskHandle_t xT2;
static TaskHandle_t xT3;
static TaskHandle_t xT4;
/* priorities for each task */
static UBaseType_t priorityT1;
static UBaseType_t priorityT2;
static UBaseType_t priorityT3;
static UBaseType_t priorityT4;
char shouldT3CleanUp;	/* should task 3 perform final task cleanup? */
/*-----------------------------------------------------------*/

XGpio OutInst;	/* GPIO Device driver instance for the LEDs */
/*-----------------------------------------------------------*/

int main( void )
{
	int Status;


	xil_printf( "Starting tasks 1 - 3. . .\n" );

	/* Create 3 of four tasks. */
	xTaskCreate( 	prvT1, 						/* The function that implements the task. */
					( const char * ) "Task1", 	/* Text name for the task, provided to assist debugging only. */
					configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
					NULL, 						/* The task parameter is not used, so set to NULL. */
					priorityT1 = tskIDLE_PRIORITY + 4,	/* The task runs at the idle priority. */
					&xT1 );

	xTaskCreate( prvT2,
				 ( const char * ) "Task2",
				 configMINIMAL_STACK_SIZE,
				 NULL,
				 priorityT2 = tskIDLE_PRIORITY + 4,
				 &xT2 );

	xTaskCreate( prvT3,
				 ( const char * ) "Task3",
				 configMINIMAL_STACK_SIZE,
				 NULL,
				 priorityT3 = tskIDLE_PRIORITY + 3,
				 &xT3 );

	/* task 3 cleans up until task 4 is running */
	shouldT3CleanUp = TRUE;

	xil_printf( "Initializing GPIO for LEDs. . .\n" );

	/* GPIO driver initialization */
	Status = XGpio_Initialize(&OutInst, GPIO_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*Set the direction for the LEDs to output. */
	XGpio_SetDataDirection(&OutInst, LED_CHANNEL, 0x0);

	///* Start the tasks and timer running. */
	vTaskStartScheduler();

	xil_printf( "Good to go!\n" );

	/* If all is well, the scheduler will now be running, and the following line
	will never be reached.  If the following line does execute, then there was
	insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	to be created.  See the memory management section on the FreeRTOS web site
	for more details. */
	for( ;; ) {}
}

/*-----------------------------------------------------------*/
static void prvT1( void *pvParameters )
{
	int unsigned iLedDelay;
	int nT1 = 0;	/* number of times task has run */

	/* LED values */
	int ledOn  = 1 << 0;

	xil_printf( "\tTask1 started!\n" );

	for( ;; )
	{
		/* raise priority to max */
		vTaskPrioritySet(NULL, configMAX_PRIORITIES);
		xil_printf( "\tTask1 on!\n" );

		/* Write output to the LEDs. */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledOn);
		/* update count */
		nT1++;
		/* keep LED on */
		for (iLedDelay = FOR_DELAY_05_SECONDS; --iLedDelay; ) {}

		/* milestones */
		switch (nT1) {
			/* suspend self after 20 seconds */
			case T1_SUSPEND:
				xil_printf( "\tTask1 suspended.\n" );
				vTaskSuspend(xT1);
				break;
			default:
				// no-op
				break;
		} /* end switch (nT1) */

		/* context switch */
		/* lower priority of self to idle */
		vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
	}
}

/*-----------------------------------------------------------*/
static void prvT2( void *pvParameters )
{
	int unsigned iLedDelay;
	const TickType_t xDelayUntilTicks = pdMS_TO_TICKS( DELAY_60_SECONDS );
	TickType_t xPreviousWakeTime;
	int nT2 = 0;	/* number of times task has run */

	/* LED values */
	int ledOn  = 1 << 1;

	xil_printf( "\t\tTask2 started!\n" );

	for( ;; )
	{
		/* raise priority to max */
		vTaskPrioritySet(NULL, configMAX_PRIORITIES);
		xil_printf( "\t\tTask2 on!\n" );

		/* Write output to the LEDs. */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledOn);
		/* update count */
		nT2++;
		/* keep LED on */
		for (iLedDelay = FOR_DELAY_05_SECONDS; --iLedDelay; ) {}

		/* once-in-a-lifetime milestones */
		switch (nT2) {
			/* change priority to P2 after 65 sec */
			case T2_P2:
				xil_printf( "\t\tTask2 priority 2.\n" );
				priorityT2 = tskIDLE_PRIORITY + 2;
				break;
			default:
				// no-op
				break;
		} /* end switch (nT2) */

		/* repeated milestones */
		if (nT2 >= T2_DELAY_UNTIL) {
			xil_printf( "\t\tTask2 delayed until 60 sec.\n" );
			vTaskDelayUntil(&xPreviousWakeTime, xDelayUntilTicks);
		} /* end if (nT2 >= T2_DELAY_UNTIL) */

		/* context switch */
		/* lower priority of self to idle */
		vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
	}
}

/*-----------------------------------------------------------*/
static void prvT3( void *pvParameters )
{
	int unsigned iLedDelay;
	const TickType_t xDelayTicks = pdMS_TO_TICKS( DELAY_20_SECONDS );
	int nT3 = 0;	/* number of times task has run */

	/* LED values */
	int ledOn  = 1 << 2;

	xil_printf( "\t\t\tTask3 started!\n" );

	for( ;; )
	{
		/* raise priority to max */
		vTaskPrioritySet(NULL, configMAX_PRIORITIES);
		xil_printf( "\t\t\tTask3 on!\n" );

		/* Write output to the LEDs. */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledOn);
		/* update count */
		nT3++;
		/* keep LED on */
		for (iLedDelay = FOR_DELAY_05_SECONDS; --iLedDelay; ) {}

		/* once-in-a-lifetime milestones */
		switch (nT3) {
			/* create T4 after 40 sec */
			case T3_T4:
				xil_printf( "\t\t\tTask3 created Task4.\n" );
				xTaskCreate( prvT4,
							 ( const char * ) "Task4",
							 configMINIMAL_STACK_SIZE,
							 NULL,
							 priorityT4 = tskIDLE_PRIORITY + 1,
							 &xT4 );
				break;
			/* resume T1 after 75 sec, and start delaying */
			case T3_RESUME_T1:
				xil_printf( "\t\t\tTask3 resumed Task1.\n" );
				vTaskResume(xT1);
				break;
			/* delete task T2 after 110 sec */
			case T3_DELETE_T2:
				xil_printf( "\t\t\tTask3 deleted Task2.\n" );
				vTaskDelete(xT2);
				break;
			default:
				// no-op
				break;
		} /* end switch (nT3) */

		/* repeated milestones */
		/* after every 6x + 3 turns, delay for 20 seconds */
		if ((nT3 >= T3_START_DELAYS)
				&& (((nT3 - T3_START_DELAYS) % T3_DELAY) == 0))
		{
			xil_printf( "\t\t\tTask3 delayed for 20 sec.\n" );
			vTaskDelay(xDelayTicks);
		} /* end if (!((nT3 - T3_RESUME_T1) % T3_DELAY)) */

		/* context switch */
		/* if task 4 does not exist yet or is yielding */
		/* at time T3_T4, task 4 will be created, so let task 4 handle it */
		if ((shouldT3CleanUp == TRUE) || (nT3 != T3_T4)) {
			/* restore all priorities */
			vTaskPrioritySet(xT1, priorityT1);
			vTaskPrioritySet(xT2, priorityT2);
			vTaskPrioritySet(NULL, priorityT3);
		}
		/* if task 4 exists */
		else {
			/* lower priority of self to idle */
			vTaskPrioritySet(NULL, tskIDLE_PRIORITY);
		}
	}
}

/*-----------------------------------------------------------*/
static void prvT4( void *pvParameters )
{
	int unsigned iLedDelay;
	int nT4 = 0;	/* number of times task has run */

	/* LED values */
	int ledOn  = 1 << 3;

	xil_printf( "\t\t\t\tTask4 started!\n" );

	for( ;; )
	{
		shouldT3CleanUp = FALSE;	/* take over for T3 in cleaning up */
		/* raise priority to max */
		vTaskPrioritySet(NULL, configMAX_PRIORITIES);
		xil_printf( "\t\t\t\tTask4 on!\n" );

		/* Write output to the LEDs. */
		XGpio_DiscreteWrite(&OutInst, LED_CHANNEL, ledOn);
		/* update count */
		nT4++;
		/* keep LED on */
		for (iLedDelay = FOR_DELAY_05_SECONDS; --iLedDelay; ) {}

		/* milestones */
		switch (nT4) {
			/* suspend self after 20 seconds */
			case T4_YIELD:
				xil_printf( "\t\t\t\tTask4 yields.\n" );
				shouldT3CleanUp = TRUE;	/* let T3 clean up again for now */
				taskYIELD();
				break;
			default:
				// no-op
				break;
		} /* end switch (nT4) */

		/* context switch */
		/* restore all priorities */
		vTaskPrioritySet(xT1, priorityT1);
		vTaskPrioritySet(xT2, priorityT2);
		vTaskPrioritySet(xT3, priorityT3);
		vTaskPrioritySet(NULL, priorityT4);
	}

}
