#include<iostream>
#include <list> 
#include <iterator> 
#include<getopt.h>
#include<fstream>
#include<string.h>
#include<stdio.h>
#include<vector>


using namespace std;
enum State {CREATED,READY,RUNNING,BLOCKED, DONE, PREMPT}; //States of process
enum Trans_to {TRANS_TO_READY, TRANS_TO_RUN, TRANS_TO_BLOCK, TRANS_TO_PREEMPT}; //Transitions
bool verbose; // print verbose info
int quantum = 10000; // default quantum 
int max_prio=4; // default number of priorities
bool param_e; // parameter e coming from command line
bool param_t; // parameter t coming from command line
 
class Process
{
    public:
        int pid; // process id
        State state; // current state
        int AT; //Arrival time
        int TC; // Total CPU time
        int CB; // CPU Burst
        int IO; // IO Burst
        int static_priority; // static priority
        int dynamic_priority; // dynamic priority
        int cpu_burst_rem; // cpu burst remaining
        int state_ts; // time when moved to current state
        int CW; // CPU waiting
        int FT; // Finish time
        int IT; // IO time
        int RT; // Response time
        int TT; // Turnaround time
        int CT; // CPU time or total time when cpu is used
        
        Process(int id, int AT, int TC, int CB, int IO)
        {
            pid = id;
            this->AT = AT;
            this->TC = TC;
            this->CB = CB;
            this->IO = IO;
            state = CREATED;
            RT = -1;
            CT = 0;
            IT = 0;
            CW =0;
            cpu_burst_rem = 0;
        }

        void printInfo()
        {
            printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n",pid, AT, TC, CB, IO, static_priority, FT, TT, IT, CW);
        }
};

class Scheduler
{
    protected:
        list<Process *> run_queue;

    public:
    // creating virtual process for runtime polymorphism
        virtual void addProcess(Process * process)=0;
	    virtual Process * getNextProcess()=0;

        void printRunQueue()
        {
            cout << "SCHED(" << run_queue.size() << "): ";

            for(list<Process *>::iterator i = run_queue.begin(); i!= run_queue.end(); i++)
            {
                Process *p = *i;
                printf("%d:%d ",p->pid,p->state_ts);
            }
            cout << endl;
        }
};

// First come first serve shceduler
class FCFS: public Scheduler
{
    public:
        void addProcess(Process * process)
        {
            run_queue.push_back(process);
        }

        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty())
            {
                p = run_queue.front();
                run_queue.erase(run_queue.begin());
            }
            else
            {
                return NULL;
            }
            return p;
        }
};

// Last come first serve scheduler
class LCFS: public Scheduler
{
    public:
        void addProcess(Process * process)
        {
            run_queue.push_back(process);
        }

        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty())
            {
                p = run_queue.back();
                run_queue.pop_back();
            }
            else
            {
                return NULL;
            }
            return p;
        }
};

// Round Robin scheduler
class RoundRobin: public Scheduler
{
    public:
        void addProcess(Process * process)
        {
            run_queue.push_back(process);
        }

        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty())
            {
                p = run_queue.front();
                run_queue.pop_front();
            }
            else
            {
                return NULL;
            }
            return p;
        }
};

// Shortest remaining time first
class SRTF: public Scheduler
{
    public:
        void addProcess(Process * process)
        {
            int flag = 0;
            int pos = 0;
            for(list<Process *>::iterator i = run_queue.begin() ; i!= run_queue.end(); i++)
            {
                pos++;
                Process *p = *i;
                if(p->TC - p->CT > process->TC - process->CT)
                {
                    flag=1;
                    break;
                }
            }
            if(flag==0)
            {
                run_queue.push_back(process);
            }
            else
            {
                list<Process *>::iterator it = run_queue.begin();
                advance(it,pos-1);
                run_queue.insert(it,process);
            }
        }

        Process * getNextProcess()
        {
            Process *p;
            if(!run_queue.empty())
            {
                p = run_queue.front();
                run_queue.pop_front();
            }
            else
            {
                return NULL;
            }
            return p;
        }
};

// priority scheduler
class PRIO: public Scheduler
{
    vector<list<Process *> > *active_queue, *expired_queue; // pointers to the queue
    vector<list<Process *> >  queue1, queue2; // actual queue
    int num_run, num_exp;
  
    public:
    // initialize the queues and the pointers
        void initialize()
        {
            for(int i=0; i<max_prio; i++)
            {
                queue1.push_back(list<Process *>());
                queue2.push_back(list<Process *>());
            }

            active_queue = &queue1;
            expired_queue = &queue2;
            num_run=0;
            num_exp=0;
        }

        void addProcess(Process * process)
        {
            if(active_queue==NULL)
                initialize();

            
            if(process->state==PREMPT)
            {
                process->dynamic_priority--;
            }

            process->state = READY;

            if(process->dynamic_priority<0)
            {
                process->dynamic_priority = process->static_priority-1;
                expired_queue->at(process->dynamic_priority).push_back(process);
                num_exp++;
            }
            else
            {
                // cout << "process added" << endl;

                active_queue->at(process->dynamic_priority).push_back(process);
                num_run++;
            } 
        }

    //  swaps the pointers 
        void swap()
        {
            // cout << "swap called" << endl;
            vector<list<Process *> > *temp = active_queue;
            active_queue = expired_queue;
            expired_queue = temp;

            num_run = num_exp;
            num_exp = 0;    
        }

        Process * getNextProcess()
        {
            if(num_run==0) 
                swap();

            Process *p = NULL;
            int queue_num = -1;

            for(int i=0; i<active_queue->size(); i++)
            {
                if(!active_queue->at(i).empty())
                {
                    queue_num=i;
                }
            }

            if(queue_num>=0)
            {
                p = active_queue->at(queue_num).front();
                active_queue->at(queue_num).pop_front(); 
                num_run--;
            }

            return p;
        }
};

//  preemptive priority scheduler
class PrePRIO: public Scheduler
{
    vector<list<Process *> > *active_queue, *expired_queue;
    vector<list<Process *> >  queue1, queue2;
    int num_run, num_exp;
    public:
        void initialize()
        {
            
            
            for(int i=0; i<max_prio; i++)
            {
                queue1.push_back(list<Process *>());
                queue2.push_back(list<Process *>());
            }

            active_queue = &queue1;
            expired_queue = &queue2;
            num_run=0;
            num_exp=0;
        }

        void addProcess(Process * process)
        {
            if(active_queue==NULL)
                initialize();

            
            if(process->state==PREMPT)
            {
                process->dynamic_priority--;
            }

            process->state = READY;

            if(process->dynamic_priority<0)
            {
                process->dynamic_priority = process->static_priority-1;
                expired_queue->at(process->dynamic_priority).push_back(process);
                num_exp++;
            }
            else
            {
                // cout << "process added" << endl;

                active_queue->at(process->dynamic_priority).push_back(process);
                num_run++;
            } 
        }

        void swap()
        {
            // cout << "swap called" << endl;
            vector<list<Process *> > *temp = active_queue;
            active_queue = expired_queue;
            expired_queue = temp;

            num_run = num_exp;
            num_exp = 0;    
        }

        Process * getNextProcess()
        {
            if(num_run==0) 
                swap();

            Process *p = NULL;
            int queue_num = -1;

            for(int i=0; i<active_queue->size(); i++)
            {
                if(!active_queue->at(i).empty())
                {
                    queue_num=i;
                }
            }

            if(queue_num>=0)
            {
                p = active_queue->at(queue_num).front();
                active_queue->at(queue_num).pop_front(); 
                num_run--;
            }

            return p;
        }
};

class Event
{
    public:
        int timestamp;
        Process *process;
        Trans_to transition;
        State old_state;

        Event(int timestamp, Process *process, Trans_to trans_to )
        {
            this->timestamp = timestamp;
            this->process = process;
            this->transition = trans_to;
        }
};

class DES
{
    public:
        list <Event> event_queue;

    public:
        void putEvent(Event event)
        {
            
            if(param_e)
            {
                string trans;
                if(event.transition==TRANS_TO_BLOCK)
                    trans = "BLOCK";
                else if(event.transition==TRANS_TO_RUN)
                    trans = "RUNNG";
                else if(event.transition==TRANS_TO_READY)
                    trans = "READY";
                else if(event.transition==TRANS_TO_PREEMPT)
                    trans = "PREEMPT";
                    
                cout << "AddEvent(" << event.timestamp << ":" << event.process->pid << ":" << trans << "): ";

                printQueue();
                printf("==> ");
            }

            
            int pos=0;
            int flag=0;
            for(list<Event>::iterator i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                pos++;
                Event e = *i;
                if(e.timestamp>event.timestamp)
                {
                    flag=1;
                    break;
                }
            }
            // cout << event_queue.size() << endl;
            // cout << pos << endl;
            if(flag==0)
            {
                event_queue.push_back(event);
            }
            else
            {
                list<Event>::iterator it = event_queue.begin();
                advance(it,pos-1);
                event_queue.insert(it,event);
            }

            if(param_e)
            {
                printQueue();
                cout << endl;
            }
        }

        Event * getEvent()
        {
            if(event_queue.empty())
            {
                return NULL;
            }

            return &event_queue.front();
        
        }

        void rm_event()
        {
            event_queue.pop_front();
        }

        int getNextEventTime()
        {
            if(event_queue.empty())
            {
                return -1;
            }
            
            return event_queue.front().timestamp;
        }

        void deleteFutureEvents(Process *p)
        {
            int pos = 0;
            int flag = 0;
            for(list<Event>::iterator i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                pos++;
                Event e = *i;
                if(e.process==p)
                {
                    flag = 1;
                    break;
                }
            }

            if(flag==1)
            {
                list<Event>::iterator it = event_queue.begin();
                advance(it,pos-1);
                event_queue.erase(it);
            }
        }

        void printQueue()
        {   
            string old;
            string trans;
            for(list<Event>::iterator i = event_queue.begin() ; i!= event_queue.end(); i++)
            {
                Event e = *i;
                
                if(e.transition==TRANS_TO_BLOCK)
                    trans = "BLOCK";
                else if(e.transition==TRANS_TO_RUN)
                     trans = "RUNNG";
                else if(e.transition==TRANS_TO_READY)
                    trans = "READY";
                else if(e.transition==TRANS_TO_PREEMPT)
                    trans = "PREEMPT";

                cout << "AddEvent(" << e.timestamp << ":" << e.process->pid << ":" << trans << "): ";
            }

        }
};

vector<Process> process_queue;
vector<int> rand_num;
int total_rand;
Scheduler *scheduler;
string scheduler_type;

// Reads the random file and create an array of random numbers
void readRandom(string file_name)
{
    ifstream file;
    file.open(file_name);
    string line;
    bool first_tok = true;
    if(file.is_open())
    {
        while(file>>line)
        {
            if(first_tok)
            {
                total_rand = stoi(line);
                first_tok = false;
            }
            else
            {
                rand_num.push_back(stoi(line));
                // cout << line << endl;
            }

        }
        file.close();
    }
    else
    {
        cout << "File " << file_name << " could not be opened." << endl;
    }    
}

int ofs=0;
// returns the next random number
int myrandom(int burst)
{
    if(ofs==total_rand) ofs=0;
    return 1 + (rand_num[ofs++] % burst); 
}

// Reads the file and creates process objects 
void readFile(string file_name, int prio)
{
    ifstream file;
    file.open(file_name);
    string line;
    if(file.is_open())
    {
        int i=0;
        while(file>>line)
        {   
            int AT, TC, CB, IO;
            AT = stoi(line);
            file>>line;
            TC = stoi(line);
            file>>line;
            CB = stoi(line);
            file>>line;     
            IO = stoi(line);

            // cout << AT << " " << TC << " " << CB << " " << IO << endl;

            Process p(i,AT,TC,CB,IO);
            p.static_priority = myrandom(prio);
            p.dynamic_priority = p.static_priority-1;
            p.state_ts = AT;
            process_queue.push_back(p);
            // cout << "process created" << endl;
            i++;
        }

        file.close();
    }
    else
    {
        cout << "File " << file_name << " could not be opened." << endl;
    }    
}

int last_event_finish;
int total_cpu_utilization=0;
int total_io_utilization=0;
int total_tt;
int total_cw;
bool isPrePrio=false;
int io_start = 0;
int io_cnt=0;

void simulation(DES *des)
{
    Process *proc;
    int current_time;
    int time_in_prev_state;
    bool call_scheduler;
    int cpu_burst;
    Process *proc_running = NULL;
    Event *e;

    while((e = des->getEvent())!=NULL)
    {
        proc = e->process;
        current_time = e->timestamp;
        time_in_prev_state = current_time - proc->state_ts;

        proc->state_ts = current_time;

        switch(e->transition)
        {
            case TRANS_TO_READY:{
                                    string old;
                                    if(e->old_state==CREATED)
                                        old = "CREATED";
                                    else if(e->old_state==BLOCKED)
                                        old = "BLOCK";
                                    else if(e->old_state==RUNNING)
                                        old = "RUNNG";
                                
                                    if(verbose)
                                        cout << current_time << " " << proc->pid << " " << time_in_prev_state << ": " << old << " -> " << "READY" << endl;

                                    if(e->old_state==BLOCKED)
                                    {
                                        proc->dynamic_priority = proc->static_priority - 1;
                                        proc->IT += time_in_prev_state;
                                        io_cnt--;
                                        if(io_cnt ==0)
                                        {
                                            total_io_utilization += current_time - io_start;
                                        }
                                        // if(verbose)
                                        //     cout << current_time << " " << proc->pid << " " << time_in_prev_state << ": " << old << " -> " << "READY" << endl;
                                    }
                                    else if(e->old_state==RUNNING)
                                    {
                                        proc->CT += time_in_prev_state;
                                        total_cpu_utilization += time_in_prev_state;
                                    }

                                    if(isPrePrio && proc_running!=NULL)
                                    {
                                        if(proc->state==BLOCKED || proc->state==CREATED)
                                        {
                                            int cts;
                                            for(list<Event>::iterator i = des->event_queue.begin(); i != des->event_queue.end();i++)
                                            {
                                                Event e = *i;
                                                if(e.process==proc_running)
                                                {
                                                    cts = e.timestamp;
                                                }
                                            }

                                            if(proc->dynamic_priority > proc_running->dynamic_priority && current_time < cts)
                                            {
                                                des->deleteFutureEvents(proc_running);
                                                Event e(current_time, proc_running, TRANS_TO_PREEMPT);
                                                e.old_state = RUNNING;
                                                des->putEvent(e);
                                            }
                                        }
                                    }

                                    scheduler->addProcess(proc);
                                    call_scheduler = true;
                                    break;
                                }
            case TRANS_TO_RUN:  { 
                                    proc->state = RUNNING;
                                    proc_running = proc;
                                    if(proc->cpu_burst_rem>0)
                                    {
                                        cpu_burst = proc->cpu_burst_rem;
                                    }
                                    else
                                    {
                                        cpu_burst = myrandom(proc->CB);
                                       
                                        if(cpu_burst > proc->TC - proc->CT)
                                        {
                                            cpu_burst = proc->TC - proc->CT;
                                        }
                                        
                                    }

                                    proc->CW += time_in_prev_state;
                                    total_cw += time_in_prev_state;
                                    proc->cpu_burst_rem = cpu_burst;

                                    if(verbose)
                                    {
                                        printf("%d %d %d: %s -> %s cb=%2d rem=%d prio=%d\n",current_time, proc->pid, time_in_prev_state, "READY", "RUNNG" ,cpu_burst, proc->TC-proc->CT, proc->dynamic_priority);
                                    }

                                    if(cpu_burst <= quantum)
                                    {
                                        Event e(current_time+cpu_burst, proc, TRANS_TO_BLOCK);
                                        e.old_state = RUNNING;
                                        des->putEvent(e);
                                    }
                                    else
                                    {
                                        Event e(current_time+quantum, proc, TRANS_TO_PREEMPT);
                                        e.old_state = RUNNING;
                                        des->putEvent(e);
                                    }

                                    break;
                                }  
            case TRANS_TO_BLOCK:    {
                                        proc_running = NULL;
                                        proc->CT += proc->cpu_burst_rem;
                                        total_cpu_utilization += time_in_prev_state;
                                        proc->cpu_burst_rem -= time_in_prev_state;
                                        

                                        if(proc->TC-proc->CT == 0)
                                        {
                                            proc->state = DONE;
                                            proc->FT = current_time;
                                            proc->TT = current_time - proc->AT;

                                            last_event_finish = current_time;
                                            total_tt += proc->TT;   

                                            if (verbose)
                                            {
                                                 printf("%d %d %d: Done \n", current_time, proc->pid, time_in_prev_state);
                                            }

                                        }
                                        else
                                        {
                                            proc->state = BLOCKED;

                                            if(io_cnt==0)
                                                io_start = current_time;

                                            io_cnt++;

                                            int ib = myrandom(proc->IO);
                                            Event e(current_time+ib, proc, TRANS_TO_READY);
                                            e.old_state = BLOCKED;
                                            
                                            des->putEvent(e);
                                            if(verbose)
                                            {
                                                printf("%d %d %d: %s -> %s ib=%2d rem=%d \n",current_time, proc->pid, time_in_prev_state,  "RUNNG", "BLOCK", ib, proc->TC-proc->CT);
                                            }
                                        }
//                                        scheduler->printRunQueue();
                                        call_scheduler = true;
                                        break;
                                        
                                    }
            case TRANS_TO_PREEMPT:  {
                                        proc->cpu_burst_rem -= time_in_prev_state;
                                        proc->CT += time_in_prev_state;

                                        if(verbose)
                                        {
                                            printf("%d %d %d: %s -> %s cb=%2d rem=%d prio=%d\n",current_time, proc->pid, time_in_prev_state, "RUNNG", "READY", proc->cpu_burst_rem, proc->TC-proc->CT, proc->dynamic_priority);
                                        }

                                        proc->state = PREMPT;
                                        proc_running = NULL;
                                        total_cpu_utilization += time_in_prev_state;
                                        scheduler->addProcess(proc);
                                        call_scheduler = true;
                                        break;
                                    }
        }
        des->rm_event();
        // cout << endl;
        // cout << " Event queue: " << endl;
        // des->printQueue();

        if(call_scheduler)
        {
            // cout << "\n here \n";
            if(des->getNextEventTime() == current_time)
            {
                continue;
            }

            call_scheduler = false;

            if(proc_running==NULL)
            {   
                proc_running = scheduler->getNextProcess();
                
                if(proc_running==NULL)
                    continue;

                Event e(current_time, proc_running, TRANS_TO_RUN);
                e.old_state = READY;
                des->putEvent(e);
            }
        }
        // cout << "Event Queue: "  << endl;;
        // des->printQueue();
    }
}

int main(int argc, char *argv[])
{
    verbose = false;
    int option;
    bool Hasquantum = false;

    while((option = getopt (argc, argv, "vtes:")) != -1)
    {
        switch(option)
        {
            case 'v': verbose = true;
                      break;
            case 't': param_t = true;
                        break;
            case 'e':  param_e = true;
                        break;
            case 's': //printf(" optarg = %s",optarg);
                      char temp;
                      if(optarg!=NULL)
                      {
                            switch(optarg[0])
                            {
                                case 'F':   scheduler_type="FCFS";
                                            scheduler = new FCFS();
                                            break;
                                case 'L':   scheduler_type="LCFS";
                                            scheduler = new LCFS();
                                            break;
                                case 'S':   scheduler_type="SRTF";
                                            scheduler = new SRTF();
                                            break;
                                case 'R':   scheduler_type = "RR";
                                            scheduler = new RoundRobin();
                                            sscanf(optarg,"%c%d",&temp,&quantum);
                                            Hasquantum=true;
                                            // printf("\n %d",quantum);
                                            break;
                                case 'P':   scheduler_type = "PRIO";
                                            sscanf(optarg,"%c%d:%d",&temp,&quantum,&max_prio);
                                            scheduler = new PRIO();
                                            Hasquantum=true;
                                            // printf("\n quantum = %d, max_prio = %d",quantum, max_prio);
                                            break;
                                case 'E':   scheduler_type = "PREPRIO";
                                            sscanf(optarg,"%c%d:%d",&temp,&quantum,&max_prio);
                                            scheduler = new PrePRIO();
                                            isPrePrio = true;
                                            Hasquantum=true;
                                            // printf("\n quantum = %d, max_prio = %d",quantum, max_prio);
                                            break;
                                default:  printf("\n Invalid scheduler name");
                                            return 1;
                            } 
                      }
                      
                      break;
            case '?': if(optopt == 's')
                        fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                      break;
            default: break;
        }
    }

    // cout << "scheduler type = " << scheduler_type << endl;

    string fileName = argv[optind];
    string randFileName = argv[optind+1];

    // cout<< randFileName << endl;

    readRandom(randFileName);
    readFile(fileName, max_prio);

    DES des;
    for(int i=0; i<process_queue.size();i++)
    {
        Event event(process_queue[i].AT,&process_queue[i],TRANS_TO_READY);
        event.old_state = CREATED;
        des.putEvent(event);
    }

    simulation(&des);
    
    cout << scheduler_type;

    if(Hasquantum)
    {
        cout << " " << quantum;
    }    
    cout << endl;

    
    for(int i=0; i<process_queue.size(); i++)
    {
        process_queue.at(i).printInfo();
    }
   
    double cpu_util = ((double)total_cpu_utilization/last_event_finish)*100;
    double io_util = ((double)total_io_utilization/last_event_finish)*100;
    double avg_tt = ((double)total_tt/process_queue.size());
    double avg_cw = ((double)total_cw/process_queue.size());
    double throughput = (process_queue.size() * 100.0)/last_event_finish;
    printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", last_event_finish, cpu_util, io_util, avg_tt, avg_cw, throughput);

}
