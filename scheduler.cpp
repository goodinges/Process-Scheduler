#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <set>
using namespace std;

//Control variables
int current_time = 0;
int *randvals;
int rand_count;
int ofs = 0;
bool CPU_engaged = false;
int RRquantum = 0;
int vflag = 0;

//reporting variables
int last_event_finish_time = 0;
int CPU_util = 0;
set<int> IO_util;

//The random generator (reader)
int myrandom(int burst)
	{
		if (ofs==rand_count)
		{
			ofs=0;
		}
		return 1 + (randvals[ofs++] % burst);	
	}

//Process objects hold all information about a process and it's state
class Process {
	public:
	int AT, TC, CB, IO, ID, FT, TT, IT, CW, CW_start_time, TC_remain, burst, burst_remain;
	Process(int AT, int TC, int CB, int IO, int ID)
		:AT(AT),TC(TC),CB(CB),IO(IO),ID(ID),TC_remain(TC),IT(0),CW(0),burst(0),burst_remain(0)
		{
		}
};

//List of processes in Ready mode
list<Process*> readyQueue;

//The Top parent scheduler class which all kind of schedulers can derive from it
class Scheduler {
	public:
	virtual Process *schedule_next()
	{
	}
};

//First come first served scheduler class
class FCFS_Scheduler: public Scheduler {
	public:
	Process *schedule_next()
	{
		if(readyQueue.empty())
		{
			throw exception();
		}
		Process *p = readyQueue.front();
		readyQueue.pop_front();
		return p;
	}
};

//Last come first served scheduler class
class LCFS_Scheduler: public Scheduler {
	public:
	Process *schedule_next()
	{
		if(readyQueue.empty())
		{
			throw exception();
		}
		Process *p = readyQueue.back();
		readyQueue.pop_back();
		return p;
	}
};

//A predicator used in SFJ scheduler
bool isSmaller(Process *p1, Process *p2)
{
	return (p1->TC_remain) < (p2->TC_remain);
}
//Shortest job first scheduler
class SJF_Scheduler: public Scheduler {
	public:
	Process *schedule_next()
	{
		if(readyQueue.empty())
		{
			throw exception();
		}
		list<Process*>::iterator min=
			min_element(readyQueue.begin(),readyQueue.end(),
					isSmaller);
		Process *p = *min;
		readyQueue.erase(min);
		return p;
	}
};

//RoundRobin scheduler
class RR_Scheduler: public Scheduler {
	public:
	Process *schedule_next()
	{
		if(readyQueue.empty())
		{
			throw exception();
		}
		Process *p = readyQueue.front();
		readyQueue.pop_front();
		return p;
	}
};

//The top parent event class, all kind of events are derived from this class
class Event{
	public:
	Process *process;
	Event(Process *process,int timestamp,int start_time)//start_time: create time, timestamp: event time
		:process(process),timestamp(timestamp),start_time(start_time)
		{}
	int timestamp;
	int start_time;
	virtual void perform()
	{
	}
};

//DES queue or list of events to happen
list<Event*> eventsList;

//A predecate to be used in DES queue
struct is_bigger
{
    is_bigger(int time) : time(time) {}
    int time;
    bool operator()(Event* event)
    {
        return event->timestamp > time;
    }
};

//A function to push the events in their correct places
void push_in_eventsList(Event *event)
{
	int time = event->timestamp;
	list<Event*>::iterator found = find_if(eventsList.begin(),eventsList.end(),
						is_bigger(time));
	if (found==eventsList.end())
	{
		eventsList.push_back(event);
	}else{
		eventsList.insert(found,event);
	}
}

//The scheduler obkect to be used in the program
Scheduler *scheduler;

//The event which happens when a CPU burst is finished
class CPU_Burst_Complete_Event: public Event{
	public:
	CPU_Burst_Complete_Event(Process *process,int timestamp,int start_time)
	:Event(process,timestamp,start_time)
	{}

	void perform();
};

//To schedule and run a new process in CPU, events use this function
void run_a_new_process()
{
		if(!CPU_engaged)
		{
				if(eventsList.empty()==false &&
					 eventsList.front()->timestamp == current_time)
				{
					if(vflag){cout << "   delay scheduler" << endl;}
					return;
				}
			try{
				Process *p_next = scheduler->schedule_next();
				int burst;
				if (p_next->burst_remain!=0){
					burst = p_next->burst_remain;
				}else{
					burst = myrandom(p_next->CB);
					p_next->burst_remain = burst;
					if(p_next->TC_remain<=burst)
					{
						burst = p_next->TC_remain;
						p_next->burst_remain = burst;
					}
				}
				if(RRquantum!=0)
				{
					if(RRquantum < burst)
					{
						burst = RRquantum;
					}
				}
				CPU_Burst_Complete_Event *cbce  =
					 new CPU_Burst_Complete_Event(p_next,current_time +burst
									,current_time);
				CPU_engaged = true;
				if(vflag){
				cout << endl << "==> " << current_time << " " << p_next->ID << " ts="
					<< p_next->CW_start_time << " RUNNG  dur="
					<< current_time - p_next->CW_start_time << endl;
				cout << "T(" << p_next->ID << ":" << current_time
				<< "): READY -> RUNNG  cb=" << p_next->burst_remain << " rem="
				<< p_next->TC_remain << endl;
				}
				push_in_eventsList(cbce);
				p_next->CW += current_time - p_next->CW_start_time;
			}catch(const exception& e){
				return;
			}
		}
}

//The event class which occurs when an IO burst is completed
class IO_Burst_Complete_Event: public Event{
	public:
	IO_Burst_Complete_Event(Process *process,int timestamp,int start_time)
	:Event(process,timestamp,start_time)
	{}

	void perform()
	{
		eventsList.pop_front();
		current_time = timestamp;
		process->IT += current_time - start_time;
		for(int i=start_time;i<current_time;i++)
		{
			IO_util.insert(i);
		}
		readyQueue.push_back(process);
		process->CW_start_time = current_time;
		if(vflag){
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< start_time << " READY  dur=" << current_time - start_time
			<< endl;
		cout << "T(" << process->ID << ":" << current_time << "): BLOCK -> READY"
			<< endl;
		}
		run_a_new_process();
	}
};

//Implemention of the CPU burst event
void CPU_Burst_Complete_Event::perform(){
	eventsList.pop_front();
	current_time = timestamp;
	process->TC_remain -= current_time - start_time;
	process->burst_remain -= current_time - start_time;
	CPU_util += current_time - start_time;
	CPU_engaged = false;
	if (process->burst_remain > 0)
	{
		if(vflag){
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< start_time << " PREEMPT  dur=" << current_time - start_time
			<< endl;
		}
	}else
	{
		if(vflag){
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< start_time << " BLOCK  dur=" << current_time - start_time
			<< endl;
		}
	}
	if(process->TC_remain==0)
	{
		if(vflag){cout << "==> T(" << process->ID << "): Done" << endl;}
		process->FT = current_time;
		process->TT = current_time - process->AT;
		last_event_finish_time = current_time;
		
	}else
	{
		if(process->burst_remain > 0)
		{
			readyQueue.push_back(process);
			process->CW_start_time = current_time;
			if(vflag){cout << "T(" << process->ID << ":" << current_time << "): RUNNG -> READY  cb="
				<< process->burst_remain << " rem=" << process->TC_remain << endl;}
		}else{
			int random = myrandom(process->IO);
			IO_Burst_Complete_Event *ibce  =
				 new IO_Burst_Complete_Event(process,current_time +random
								,current_time);
			if(vflag){cout << "T(" << process->ID << ":" << current_time
				<< "): RUNNG -> BLOCK  ib="
				<< random << " rem=" << process->TC_remain << endl;
			}
			push_in_eventsList(ibce);
		}
	}
	run_a_new_process();
}

//A different event that is generated only when the program starts to push
//processes into ready queue when they arrive
class Init_Event: public Event{
	public:
	Init_Event(Process *process)
	:Event(process,process->AT,process->AT)
	{}
	void perform()
	{
		eventsList.pop_front();
		readyQueue.push_back(process);
		current_time = timestamp;
		process->CW_start_time = current_time;
		if(vflag){cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< current_time	<< " READY  dur=0" << endl;
		cout << "T(" << process->ID << ":" << current_time << "): READY -> READY"
			<< endl;
		}
		run_a_new_process();
	}
};

//Program starts here!
int main(int argc, char *argv[])
{
	char *svalue = NULL;
	int c;
	int index;
	while ((c = getopt (argc, argv, "vs:")) != -1)
	{
		switch (c)
		{
			case 'v':
			vflag = 1;
			break;
			case 's':
			svalue = optarg;
			break;
			default:
			abort ();
		}
	}
	ifstream infile(argv[optind]);
	ifstream randfile(argv[optind+1]);
	RRquantum = 0;
	switch(svalue[0])
	{
		case 'F':
		scheduler = new FCFS_Scheduler();
		break;
		case 'L':
		scheduler = new LCFS_Scheduler();
		break;
		case 'S':
		scheduler = new SJF_Scheduler();
		break;
		case 'R':
		scheduler = new RR_Scheduler();
		RRquantum = atoi(svalue+1);
	}
	randfile >> rand_count;
	randvals = new int[rand_count];
	for(int i=0;i<rand_count;i++)
	{
		randfile >> randvals[i];
	}
	list<Process*> processes;
	int AT, TC, CB, IO;
	int ID = 0;
	while(infile >> AT >> TC >> CB >> IO)
	{
		Process *process = new Process(AT,TC,CB,IO,ID++);
		processes.push_back(process);
		Init_Event *ie = new Init_Event(process);
		eventsList.push_back(ie);
	}
	
	while(eventsList.empty()!=true)
	{
/*		for (list<Event*>::const_iterator ci = eventsList.begin(); ci != eventsList.end(); ci++)
		{
			cout << ((*ci)->process)->ID << "("<< ((*ci)->timestamp) <<")";
		}cout << endl;*/
		eventsList.front()->perform();
	}
	switch(svalue[0])
	{
		case 'F':
		cout << "FCFS" << endl;
		break;
		case 'L':
		cout << "LCFS" << endl;
		break;
		case 'S':
		cout << "SJF" << endl;
		break;
		case 'R':
		cout << "RR " << atoi(svalue+1) << endl;
	}
	for (list<Process*>::const_iterator ci = processes.begin(); ci != processes.end(); ci++)
	{
		printf("%04d: %4d %4d %4d %4d | %4d %4d %4d %4d\n",(*ci)->ID,(*ci)->AT,
		(*ci)->TC,(*ci)->CB,(*ci)->IO,(*ci)->FT,(*ci)->TT,(*ci)->IT,(*ci)->CW);
	}
	double cpu_util = (double)CPU_util/(double)last_event_finish_time*100;
	double io_util = (double)IO_util.size()/(double)last_event_finish_time*100;
	double TT_ave = 0;
	double CW_ave = 0;
	for (list<Process*>::const_iterator ci = processes.begin(); ci != processes.end(); ci++)
	{
		TT_ave += (*ci)->TT;
		CW_ave += (*ci)->CW;
	}
	TT_ave /= (double)processes.size();
	CW_ave /= (double)processes.size();
	printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n",last_event_finish_time,
		cpu_util,io_util,TT_ave,CW_ave,
		(double)processes.size()/(double)last_event_finish_time*100);
}
