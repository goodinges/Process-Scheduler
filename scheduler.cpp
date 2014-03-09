#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <set>
using namespace std;

int current_time = 0;
int *randvals;
int rand_count;
int ofs = 0;
bool CPU_engaged = false;

//reporting variables
int last_event_finish_time = 0;
int CPU_util = 0;
set<int> IO_util;

int myrandom(int burst)
	{
		if (ofs==rand_count)
		{
			ofs=0;
		}
		return 1 + (randvals[ofs++] % burst);	
	}

class Process {
	public:
	int AT, TC, CB, IO, ID, FT, TT, IT, CW, CW_start_time, TC_remain;
	Process(int AT, int TC, int CB, int IO, int ID)
		:AT(AT),TC(TC),CB(CB),IO(IO),ID(ID),TC_remain(TC),IT(0),CW(0)
		{
		}
};

list<Process*> readyQueue;

class Scheduler {
	public:
	virtual Process *schedule_next()
	{
	}
};

class FIFO_Scheduler: public Scheduler {
	public:
	Process *schedule_next()
	{
		if(readyQueue.empty())
		{
			throw exception();
		}
		Process *p = readyQueue.front();
//	cout << "--";
//	for (list<Process*>::const_iterator ci = readyQueue.begin(); ci != readyQueue.end(); ci++)
//	{
//		cout << (*ci)->ID;
//	}cout << endl;
		readyQueue.pop_front();
		return p;
	}
};

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

list<Event*> eventsList;

struct is_bigger
{
    is_bigger(int time) : time(time) {}
    int time;
    bool operator()(Event* event)
    {
        return event->timestamp > time;
    }
};

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

Scheduler *scheduler;

class CPU_Burst_Complete_Event: public Event{
	public:
	CPU_Burst_Complete_Event(Process *process,int timestamp,int start_time)
	:Event(process,timestamp,start_time)
	{}

	void perform();
};

void run_a_new_process()
{
		if(!CPU_engaged)
		{
				if(eventsList.empty()==false &&
					 eventsList.front()->timestamp == current_time)
				{
					cout << "   delay scheduler" << endl;
					return;
				}
			try{
				Process *p_next = scheduler->schedule_next();
				int random = myrandom(p_next->CB);
				if(p_next->TC_remain<random)
				{
					random = p_next->TC_remain;
				}
				CPU_Burst_Complete_Event *cbce  =
					 new CPU_Burst_Complete_Event(p_next,current_time +random
									,current_time);
				CPU_engaged = true;
				//(p_next->TC_remain) -= random;//check for total and quartz?
				cout << endl << "==> " << current_time << " " << p_next->ID << " ts="
					<< p_next->CW_start_time << " RUNNG  dur="
					<< current_time - p_next->CW_start_time << endl;
				cout << "T(" << p_next->ID << ":" << current_time
				<< "): READY -> RUNNG  cb=" << random << " rem="
				<< p_next->TC_remain << endl;
	//			cout<< "CPU burst push request for CPU" <<endl;
				push_in_eventsList(cbce);
				p_next->CW += current_time - p_next->CW_start_time;
			}catch(const exception& e){
				return;
			}
		}
}
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
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< start_time << " READY  dur=" << current_time - start_time
			<< endl;
		cout << "T(" << process->ID << ":" << current_time << "): BLOCK -> READY"
			<< endl;
			run_a_new_process();
	}
};

void CPU_Burst_Complete_Event::perform(){
//		cout << "CPU burst" << endl;
	eventsList.pop_front();
	current_time = timestamp;
	process->TC_remain -= current_time - start_time;
	CPU_util += current_time - start_time;
	CPU_engaged = false;
	cout << endl << "==> " << current_time << " " << process->ID << " ts="
		<< start_time << " BLOCK  dur=" << current_time - start_time
		<< endl;
	if(process->TC_remain==0)
	{
		cout << "==> T(" << process->ID << "): Done" << endl;
		process->FT = current_time;
		process->TT = current_time - process->AT;
		last_event_finish_time = current_time;
		
	}else
	{
		int random = myrandom(process->IO);
		IO_Burst_Complete_Event *ibce  =
			 new IO_Burst_Complete_Event(process,current_time +random
							,current_time);
		//(p_next->TC_remain) -= random;//check for total and quartz?
		cout << "T(" << process->ID << ":" << current_time
			<< "): RUNNG -> BLOCK  ib="
			<< random << " rem=" << process->TC_remain << endl;
//			cout<< "CPU burst push request for IO" <<endl;
		push_in_eventsList(ibce);
	}
	run_a_new_process();
}

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
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< current_time	<< " READY  dur=0" << endl;
		cout << "T(" << process->ID << ":" << current_time << "): READY -> READY"
			<< endl;
		run_a_new_process();
	}
};

int main()
{
	ifstream infile("input3");
	scheduler = new FIFO_Scheduler();
	ifstream randfile("rfile");
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
	cout << "FCFS" << endl;
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
