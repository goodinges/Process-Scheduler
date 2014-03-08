#include <iostream>
#include <fstream>
#include <list>
#include <algorithm>
#include <iterator>
using namespace std;

int current_time = 0;

int *randvals;
int rand_count;
int ofs = 0;
int myrandom(int burst)
	{
		if (ofs<rand_count-1)
		{
			return 1 + (randvals[ofs++] % burst);
		}
		ofs = 0;
		return 1 + (randvals[rand_count]  % burst);
	}

class Process {
	public:
	int AT, TC, CB, IO, ID;
	Process(int AT, int TC, int CB, int IO, int ID)
		:AT(AT),TC(TC),CB(CB),IO(IO),ID(ID)
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
		
		Process *p = readyQueue.front();
		readyQueue.pop_front();
		return p;
	}
};

class Event{
	protected:
	Process *process;
	Event(Process *process,int timestamp)
		:process(process),timestamp(timestamp)
		{}
	public:
	int timestamp;
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

void push_in_eventsList(Event *event,int time)
{
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
	CPU_Burst_Complete_Event(Process *process,int timestamp)
	:Event(process,timestamp)
	{}

	void perform()
	{
	}
};

class Init_Event: public Event{
	public:
	Init_Event(Process *process)
	:Event(process,process->AT)
	{}
	void perform()
	{
		eventsList.pop_front();
		readyQueue.push_back(process);
		if(current_time<timestamp)
		{
			current_time = timestamp;
		}
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< current_time	<< " READY  dur=0" << endl;
		cout << "T(" << process->ID << ":" << current_time << "): READY -> READY"
			<< endl;
		if(eventsList.empty()==false && eventsList.front()->timestamp == timestamp)
		{
			return;
		}
		Process *p_next = scheduler->schedule_next();
		int random = myrandom(p_next->CB);
		CPU_Burst_Complete_Event *cbce  =
			 new CPU_Burst_Complete_Event(p_next,current_time +random);
		//(p_next->TC) -= random;//check for total and quartz?
		cout << endl << "==> " << current_time << " " << process->ID << " ts="
			<< current_time	<< " RUNNG  dur=0" << endl;
		cout << "T(" << process->ID << ":" << current_time << "): READY -> RUNNG  cb="
			<< random << " rem=" << p_next->TC << endl;
		push_in_eventsList(cbce,current_time);
	}
};

int main()
{
	ifstream infile("input0");
	scheduler = new FIFO_Scheduler();
	ifstream randfile("rfile");
	randfile >> rand_count;
	randvals = new int[rand_count];
	for(int i=0;i<rand_count;i++)
	{
		randfile >> randvals[i];
	}
	int AT, TC, CB, IO;
	int ID = 0;
	while(infile >> AT >> TC >> CB >> IO)
	{
		Process *process = new Process(AT,TC,CB,IO,ID++);
		Init_Event *ie = new Init_Event(process);
		eventsList.push_back(ie);
	}
	
	while(eventsList.empty()!=true)
	{
		eventsList.front()->perform();
	}
	
}
