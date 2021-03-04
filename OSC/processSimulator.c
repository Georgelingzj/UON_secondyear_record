/**
 * COMP2007 UNUK 20-21 Operating systems and concurrency coursework
 * 
 * author: Zijian Ling
 * student id: 20123725
 * email: scyzl4@nottingham.ac.uk
 * last modified date: 2020-12-18
 * 
 * compile my code: gcc processSimulator.c coursework.c linkedlist.c  -o processSimulator -std=gnu99 -lpthread
 */

#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "linkedlist.h"
#include "coursework.h"

//define basetime when the main process begin to run
struct timeval oBaseTime;

typedef struct process myProcess;
typedef struct element myElement;


const int EXIT_MALLOC_FAILURE = -1;
const int EXIT_OTHER_FAILURE = -2;
const int RAN_PROCESS_PRIORITY = MAX_PRIORITY + 1;
int running_process_priority = MAX_PRIORITY + 1;//current running process priority
int running_process_priority_record = MAX_PRIORITY + 1;//process priority just finished running
myProcess *running_process = NULL;//current running process

//initialize process number of each function will processed
int terminationNum_of_process = 0;
int produced_of_process = 0;
int addmitted_of_process = 0;
int runned_of_process = 0;
//initialize time variable
int response_time = 0;
int turnaround_time = 0;

//method initialization
void ProcessTableInit();
void InitEmptyPIDList();
void ProcessGenerator();
void LongtermScheduler();
void readyPriorityTableInit();
void ShorttermScheduler();
void Booster();
void Termination();
void threadCheck(int number);
int getHighestPriority();
void printHeadersSVG();
void printPrioritiesSVG();
void printRasterSVG();
void printFootersSVG();
void printProcessSVG(int iCPUId, struct process * pProcess, struct timeval oStartTime, struct timeval oEndTime);

//initialize the process table with null pointer
myProcess *ProcessTable[SIZE_OF_PROCESS_TABLE];
//initialize the ready priority table
myElement *readyPriorityTablehead[MAX_PRIORITY];
myElement *readyPriorityTabletail[MAX_PRIORITY];

//initialize the empty PID list
myElement *ePIDhead = NULL;
myElement *ePIDtail = NULL;
myElement *newPHead = NULL;
myElement *newPTail = NULL;
myElement *readyPHead = NULL;
myElement *readyPTail = NULL;
myElement *terPHead = NULL;
myElement *terPTail = NULL;

//initialize semaphore
sem_t sem_processproducer;
sem_t sem_longterm;
sem_t sem_shortterm;
sem_t sem_ter;
sem_t sem_booster;

//initialize lock
pthread_mutex_t mutex_freepid = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_processtable = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_newprocess = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_readyprocess = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_termination = PTHREAD_MUTEX_INITIALIZER;

int main()
{
    int ret = 1; // for thread create
    //create array to store pthread and thread if
    pthread_t cputhreadArray[NUMBER_OF_CPUS];
    int cpuidArray[NUMBER_OF_CPUS];
    for(int j = 0; j<NUMBER_OF_CPUS; j++){
        cpuidArray[j] = j+1;
    }
    //initialize pthread_t
    pthread_t p1_PGenerator;
    pthread_t p2_LongScheduler;
    pthread_t p4_termination;
    pthread_t p5_booster;
    //initialize semaphore
    sem_init(&sem_processproducer, 0, SIZE_OF_PROCESS_TABLE);
    sem_init(&sem_longterm,0,0);
    sem_init(&sem_shortterm,0,0);
    sem_init(&sem_ter,0,0);
    sem_init(&sem_booster,0,0);
    //initialize basetime
    gettimeofday(&oBaseTime,NULL);
    //initialize processtabel, ready queue and free pid list
    ProcessTableInit();
    readyPriorityTableInit();
    InitEmptyPIDList();

    //begin output
    printHeadersSVG();
    printPrioritiesSVG();
    printRasterSVG();
    //create thread
    ret = pthread_create(&p1_PGenerator,NULL,(void *)ProcessGenerator, NULL);
    threadCheck(ret);

    ret = pthread_create(&p2_LongScheduler, NULL, (void *)LongtermScheduler, NULL);
    threadCheck(ret);

    //create thread array for mutiple cpu
    for(int k = 0; k<NUMBER_OF_CPUS; k++){
       ret = pthread_create(&cputhreadArray[k],NULL,(void*)ShorttermScheduler,(void*)&cpuidArray[k]); 
       threadCheck(ret);
    }

    ret = pthread_create(&p4_termination,NULL,(void*)Termination,NULL);
    threadCheck(ret);
    
    ret = pthread_create(&p5_booster,NULL,(void *)Booster,NULL);
    threadCheck(ret);

    pthread_join(p1_PGenerator,NULL);
    pthread_join(p2_LongScheduler, NULL);
    pthread_join(p4_termination, NULL);

    printFootersSVG();
    //destory semaphore
    sem_destroy(&sem_processproducer);
    sem_destroy(&sem_longterm);
    sem_destroy(&sem_shortterm);
    sem_destroy(&sem_ter);
    sem_destroy(&sem_booster);
    //destory mutex lock
    pthread_mutex_destroy(&mutex_freepid);
    pthread_mutex_destroy(&mutex_processtable);
    pthread_mutex_destroy(&mutex_newprocess);
    pthread_mutex_destroy(&mutex_readyprocess);
    pthread_mutex_destroy(&mutex_termination);
}


void InitEmptyPIDList()
{
    //initialize free pid list
    for(int i = 0;i<SIZE_OF_PROCESS_TABLE;i++)
    {   
        int *index = (int*)malloc(sizeof(int*));
        *index = i;
        addLast(index,&ePIDhead,&ePIDtail);
    }
}

void readyPriorityTableInit()
{ 
    /**
     * initialize ready queue
     * my ready queue: list of pointer,
     * store head pointer of specific priority ready queue in readyPriorityTablehead[]
     * store tail pointer of specific priority ready queue in readyPriorityTabletail[]
    */
    for (int i = 0; i < MAX_PRIORITY; i++)
    {
        //myElement *head = NULL;
        readyPriorityTablehead[i] = NULL;
        readyPriorityTabletail[i] = NULL;
    }
}
void ProcessTableInit()
{
    //initialize processtable 
    for(int i = 0;i<SIZE_OF_PROCESS_TABLE;i++)
    {
        myProcess* new = NULL;
        ProcessTable[i] = new;
    }
}

void ProcessGenerator()
{
    /**
     * Process generator
     * run times = NUMBER_OF_PROCESSES
     */
    while(produced_of_process < NUMBER_OF_PROCESSES)
    {
        //if has free pid, then run
        sem_wait(&sem_processproducer);

        pthread_mutex_lock(&mutex_processtable);
        //create new process based on free pid
        int p = *(int*)ePIDhead->pData;
        myProcess *newprocess = generateProcess(ePIDhead->pData);
        ProcessTable[p] = newprocess;
        produced_of_process++;
        printf("TXT: Generated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *newprocess->pPID,newprocess->iPriority, newprocess->iPreviousBurstTime, newprocess->iRemainingBurstTime);
        pthread_mutex_unlock(&mutex_processtable);

        pthread_mutex_lock(&mutex_newprocess);
        //add node to new process queue
        addLast(newprocess->pPID,&newPHead,&newPTail);
        pthread_mutex_unlock(&mutex_newprocess);

        pthread_mutex_lock(&mutex_freepid);
        //delete node in free pid 
        int pdata = *(int*)removeFirst(&ePIDhead,&ePIDtail);
        pthread_mutex_unlock(&mutex_freepid);
    }
}

void LongtermScheduler()
{
    /**
     * Long-term scheduler
     * run times = NUMBER_OF_PROCESSES
     */
    while (addmitted_of_process < NUMBER_OF_PROCESSES)
    {
        if (newPHead != NULL)
        {
            pthread_mutex_lock(&mutex_newprocess);
            //remove node in new process queue
            int p = *(int*)removeFirst(&newPHead,&newPTail);
            pthread_mutex_unlock(&mutex_newprocess);
                
            pthread_mutex_lock(&mutex_readyprocess);
            pthread_mutex_lock(&mutex_processtable);
            //add node in corresponding priority queue
            addLast(ProcessTable[p]->pPID, &readyPriorityTablehead[ProcessTable[p]->iPriority],&readyPriorityTabletail[ProcessTable[p]->iPriority]);
            addmitted_of_process++;
            printf("TXT: Admitted: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", *(int*)ProcessTable[p]->pPID,ProcessTable[p]->iPriority,ProcessTable[p]->iPreviousBurstTime, ProcessTable[p]->iRemainingBurstTime);
            pthread_mutex_unlock(&mutex_processtable);
            pthread_mutex_unlock(&mutex_readyprocess);  
            
            //if new addmitted process has higher priority then current running process
            if (ProcessTable[p]->iPriority < running_process_priority && running_process != NULL)
            {
                if (running_process->iRemainingBurstTime != 0)
                {
                    //interrup current running process
                    preemptJob(ProcessTable[*(int*)running_process->pPID]);
                }
            }
            sem_post(&sem_shortterm);   
        }
        usleep(LONG_TERM_SCHEDULER_INTERVAL);
    }
}

void ShorttermScheduler(void* id)
{
    /**
     * Short-term scheduler
     * run times = NUMBER_OF_PROCESSES
     */
    int shorttermid = *(int*)id;//get id of shorttermscheduler thread
    while (runned_of_process < NUMBER_OF_PROCESSES)
    {
        sem_wait(&sem_shortterm);    
        
        pthread_mutex_lock(&mutex_readyprocess);
        int i = getHighestPriority();
        int index = *(int*)removeFirst(&readyPriorityTablehead[i],&readyPriorityTabletail[i]);
        pthread_mutex_unlock(&mutex_readyprocess);
        //initialize time variable
        struct timeval starttime, endtime;
        gettimeofday(&starttime,NULL);
        gettimeofday(&endtime,NULL);
        //get pre running process
        myProcess *p = ProcessTable[index];
        running_process = ProcessTable[index];
        running_process_priority = p->iPriority;
        //divide higher and lower into different
        if (i<(MAX_PRIORITY/2))
        {
            //higher priority(FCFS)
            runNonPreemptiveJob(ProcessTable[index],&starttime,&endtime);
            running_process_priority_record = running_process_priority;
            running_process_priority = RAN_PROCESS_PRIORITY;
        }
        else
        {
            //lower priority(RR)
            runPreemptiveJob(ProcessTable[index],&starttime,&endtime);
            running_process_priority_record = running_process_priority;
            running_process_priority = RAN_PROCESS_PRIORITY;
        }
                
        //if one process has no remainingburstime
        if (ProcessTable[index]->iRemainingBurstTime == 0)
        {  
            if (p->iInitialBurstTime == p->iPreviousBurstTime)
            {
                //last run as well as first run
                if (i < (MAX_PRIORITY/2))
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime, getDifferenceInMilliSeconds(p->oTimeCreated, p->oFirstTimeRunning), getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
                else
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld, Turnaround Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime, getDifferenceInMilliSeconds(p->oTimeCreated, p->oFirstTimeRunning), getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
            }
            else
            {
                //only last run
                if (i < (MAX_PRIORITY/2))
                {
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime,getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
                else
                {
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Turnaround Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime, getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
            }
            printProcessSVG(shorttermid, ProcessTable[index],starttime,endtime);
            
            pthread_mutex_lock(&mutex_termination);
            //add the process to termination queue 
            addLast(p->pPID,&terPHead,&terPTail);
            runned_of_process++;
            pthread_mutex_unlock(&mutex_termination);
        }
        else if (ProcessTable[index]->iRemainingBurstTime != 0 )
        {
            if (p->iInitialBurstTime == p->iRemainingBurstTime)
            {
                if (i < (MAX_PRIORITY/2))
                {
                    //first run
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime, getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
                else
                {
                    //first run
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d, Response Time = %ld\n",shorttermid, *p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime, getDifferenceInMilliSeconds(p->oTimeCreated, p->oLastTimeRunning));
                }
            }
            else
            {
                if (i < (MAX_PRIORITY/2))
                {
                    //not first run
                    printf("TXT: Consumer %d, Process Id = %d (FCFS), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n",shorttermid,*p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime);
                }
                else
                {
                    //not first run
                    printf("TXT: Consumer %d, Process Id = %d (RR), Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n",shorttermid,*p->pPID, p->iPriority, p->iPreviousBurstTime, p->iRemainingBurstTime);
                }
            }
            printProcessSVG(shorttermid, ProcessTable[index],starttime,endtime);
            //if the process be interrupted

            /**
             * the efficiency of if-statement below is roughly same as written one if-statament for iPreempt and 
             * add one if-statament to determine the priority
             */
            if (ProcessTable[index]->iPreempt == 1 && i >= (MAX_PRIORITY/2))
            {
                pthread_mutex_lock(&mutex_readyprocess);
                //add this process to the first of it's priority readyqueue
                addFirst(p->pPID,&readyPriorityTablehead[running_process_priority_record],&readyPriorityTabletail[running_process_priority_record]);                        
                pthread_mutex_unlock(&mutex_readyprocess);

                pthread_mutex_lock(&mutex_processtable);
                //set iPreempt back to 0
                ProcessTable[index]->iPreempt == 0;
                pthread_mutex_unlock(&mutex_processtable);
                sem_post(&sem_shortterm);  
            }
            else if (ProcessTable[index]->iPreempt == 1 && i >= (MAX_PRIORITY/2))
            {
                pthread_mutex_lock(&mutex_readyprocess);
                //add runned one to the last of its priority list
                addLast(p->pPID,&readyPriorityTablehead[i],&readyPriorityTabletail[i]);
                pthread_mutex_unlock(&mutex_readyprocess);

                pthread_mutex_lock(&mutex_processtable);
                //set iPreempt back to 0
                ProcessTable[index]->iPreempt == 0;
                pthread_mutex_unlock(&mutex_processtable);
                sem_post(&sem_shortterm);
            }
            else if (ProcessTable[index]->iPreempt == 0 && ProcessTable[index]->iRemainingBurstTime != 0)
            {
                pthread_mutex_lock(&mutex_readyprocess);
                //add runned one to the last of its priority list
                addLast(p->pPID,&readyPriorityTablehead[i],&readyPriorityTabletail[i]);
                pthread_mutex_unlock(&mutex_readyprocess);
                sem_post(&sem_shortterm);
            }                    
                                        
        }
    }
}

void Booster()
{
    /**
     * Booster Daemon
     * run times = NUMBER_OF_PROCESSES
     */
    while (terminationNum_of_process < NUMBER_OF_PROCESSES)
    {
        pthread_mutex_lock(&mutex_readyprocess);
        //get pointer of middle priority ready queue
        myElement *highertail = readyPriorityTablehead[MAX_PRIORITY/2];
        /**
         * Add all lower priority process to the middle priotiry ready queue
         * 
         * Way1 of add: if add from middle to lowest -> get downtrend in the latter part of lower priority SVG in output.html(like output sample)
         * 
         * Way2 of add: if add from lowerst to middle -> get uptrend in the latter part of lower priority SVG in output.html(like output sample)
         * 
         * myChoose: way1
         */
        for(int i = (MAX_PRIORITY/2)+1;i < MAX_PRIORITY ; i++)
        {
            if (readyPriorityTablehead[i] == NULL)
            {
                continue;
            }
            else
            {
                while (readyPriorityTablehead[i] != NULL)
                {
                    //delete node in original priority list
                    int index = *(int*)removeFirst(&readyPriorityTablehead[i],&readyPriorityTabletail[i]);
                    
                    pthread_mutex_lock(&mutex_processtable);
                    //add node to the tail of highertail list
                    addLast(ProcessTable[index]->pPID,&readyPriorityTablehead[MAX_PRIORITY/2],&readyPriorityTabletail[MAX_PRIORITY/2]);
                    printf("TXT: Boost: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", index, ProcessTable[index]->iPriority, ProcessTable[index]->iPreviousBurstTime, ProcessTable[index]->iRemainingBurstTime);
                    pthread_mutex_unlock(&mutex_processtable);    
                }
                
            }
        }
        pthread_mutex_unlock(&mutex_readyprocess);
        usleep(BOOST_INTERVAL);    
    }
}

void Termination()
{   
    /**
     * Termination Daemon
     * run times = NUMBER_OF_PROCESSES
     */
    while(terminationNum_of_process < NUMBER_OF_PROCESSES)
    {
        
        if (terPHead != NULL)
        {
            pthread_mutex_lock(&mutex_termination);
            //remove node from the terminatedQueue 
            int pdata = *(int*)removeFirst(&terPHead,&terPTail);
            terminationNum_of_process++;
            pthread_mutex_unlock(&mutex_termination);

            
            pthread_mutex_lock(&mutex_freepid);
            //add time variable
            response_time =  response_time + getDifferenceInMilliSeconds(ProcessTable[pdata]->oTimeCreated, ProcessTable[pdata]->oFirstTimeRunning);
            turnaround_time = turnaround_time + getDifferenceInMilliSeconds(ProcessTable[pdata]->oTimeCreated, ProcessTable[pdata]->oLastTimeRunning);
            //add node in freepid list
            addLast(ProcessTable[pdata]->pPID,&ePIDhead,&ePIDtail);
            pthread_mutex_unlock(&mutex_freepid);

            
            pthread_mutex_lock(&mutex_processtable);
            //delete it from the process table
            printf("TXT: Terminated: Process Id = %d, Priority = %d, Previous Burst Time = %d, Remaining Burst Time = %d\n", pdata, ProcessTable[pdata]->iPriority, ProcessTable[pdata]->iPreviousBurstTime, ProcessTable[pdata]->iRemainingBurstTime);
            free(ProcessTable[pdata]);
            pthread_mutex_unlock(&mutex_processtable);
            sem_post(&sem_processproducer);
        }
        usleep(TERMINATION_INTERVAL);
        
    }
    //calculate process statistics after last run
    float avgResponse = (float)(response_time)/NUMBER_OF_PROCESSES;
    float avgTurn = (float)(turnaround_time)/NUMBER_OF_PROCESSES;
    printf("TXT: Average response time = %f, Average turnaround time = %f\n", avgResponse, avgTurn);
}

void threadCheck(int number)
{
    //check whether thread allocate is normal
    if (number != 0)
    {
        printf("Create thread error");
        exit(EXIT_OTHER_FAILURE);
    }
    
}

int getHighestPriority(){
    //get the current highest priority in ready queue
    int localpriority = RAN_PROCESS_PRIORITY;
    for(int i = 0;i<MAX_PRIORITY;i++){
        if (readyPriorityTablehead[i] != NULL)
        {
            localpriority = i;
            break;
        }
    }
    return localpriority;
}

void printHeadersSVG()
{
	printf("SVG: <!DOCTYPE html>\n");
	printf("SVG: <html>\n");
	printf("SVG: <body>\n");
	printf("SVG: <svg width=\"10000\" height=\"1100\">\n");
}

void printPrioritiesSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS;iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 4;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <text x=\"0\" y=\"%d\" fill=\"black\">%d</text>\n", iYOffsetCPU + iYOffsetPriority, iPriority);
		}
	}
}

void printRasterSVG()
{
	for(int iCPU = 1; iCPU <= NUMBER_OF_CPUS;iCPU++)
	{
		for(int iPriority = 0; iPriority < MAX_PRIORITY; iPriority++)
		{
			int iYOffsetPriority = (iPriority + 1) * 16 - 8;
			int iYOffsetCPU = (iCPU - 1) * (480 + 50);
			printf("SVG: <line x1=\"%d\" y1=\"%d\" x2=\"10000\" y2=\"%d\" style=\"stroke:rgb(125,125,125);stroke-width:1\" />\n", 16, iYOffsetCPU + iYOffsetPriority, iYOffsetCPU + iYOffsetPriority);
		}
	}
}

void printFootersSVG()
{
	printf("SVG: Sorry, your browser does not support inline SVG.\n");
	printf("SVG: </svg>\n");
	printf("SVG: </body>\n");
	printf("SVG: </html>\n");
}

void printProcessSVG(int iCPUId, struct process * pProcess, struct timeval oStartTime, struct timeval oEndTime)
{
	int iXOffset = getDifferenceInMilliSeconds(oBaseTime, oStartTime) + 30;
	int iYOffsetPriority = (pProcess->iPriority + 1) * 16 - 12;
	int iYOffsetCPU = (iCPUId - 1 ) * (480 + 50);
	int iWidth = getDifferenceInMilliSeconds(oStartTime, oEndTime);
	printf("SVG: <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"8\" style=\"fill:rgb(%d,0,%d);stroke-width:1;stroke:rgb(255,255,255)\"/>\n", iXOffset /* x */, iYOffsetCPU + iYOffsetPriority /* y */, iWidth, *(pProcess->pPID) - 1 /* rgb */, *(pProcess->pPID) - 1 /* rgb */);
}

