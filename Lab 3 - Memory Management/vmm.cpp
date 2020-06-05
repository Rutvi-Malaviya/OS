#include<iostream>
#include <list> 
#include <iterator> 
#include<getopt.h>
#include<fstream>
#include<string.h>
#include<stdio.h>
#include<vector>
#include<queue>

using namespace std;

const int PTE_NUMBER = 64;

bool summaryOpt;
bool frameTableOpt;
bool pageTableOpt;
bool Oopt;

unsigned long total_ctx_switch;
unsigned long total_process_exits;
unsigned long long total_cost;
unsigned long instructionCnt;

vector<int> rand_num;
int total_rand;

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
    return (rand_num[ofs++] % burst); 
}

// Page Table Entry
class PTE
{
    public:
        unsigned PRESENT:1;
        unsigned WRITE_PROTECT:1;
        unsigned MODIFIED:1;
        unsigned REFERENCED:1;
        unsigned PAGEDOUT:1;
        unsigned frameNumber:7;
        unsigned fileMapped:1;
        unsigned vPageNo:6;
        unsigned vmaSet:1;
        unsigned vma:12;

        PTE()
        {
            PRESENT = 0;
            WRITE_PROTECT  = 0;
            REFERENCED = 0;
            MODIFIED = 0;
            PAGEDOUT = 0;
            frameNumber = 0;
            fileMapped = 0;
            vmaSet = 0;
            vma = 0;
        }
};

class VMA
{
    public:
        int starting_virtual_page;
        int ending_virtual_page;
        int write_protected;
        int file_mapped;

        VMA(int svp, int evp, int wp, int fm)
        {
            starting_virtual_page = svp;
            ending_virtual_page = evp;
            write_protected = wp;
            file_mapped = fm;
        }
};

//  Process structure 
//  Contains vector of virtual memory address and page table  
class Process
{
    public:
        int pid;
        vector<VMA> vmas;
        PTE pageTable[PTE_NUMBER];
        unsigned long maps;
        unsigned long unmaps;
        unsigned long ins;
        unsigned long outs;
        unsigned long fins;
        unsigned long fouts;
        unsigned long zeros;
        unsigned long segv;
        unsigned long segprot;

        Process(int pid)
        {
            this->pid = pid;
            int i;
            for(i=0; i<PTE_NUMBER; i++)
            {
                pageTable[i] = *(new PTE);
                pageTable[i].vPageNo = i;
            }

            unmaps = 0;
            maps = 0;
            ins = 0;
            outs = 0;
            fins = 0;
            fouts = 0;
            zeros = 0;
            segv = 0;
            segprot = 0;
        }

        void printPageTable()
        {
            cout << "PT[" << pid << "]: ";
            int i;
            for(i = 0; i<PTE_NUMBER; i++)
            {
                if(pageTable[i].PRESENT==1)
                {
                    cout << i << ":";

                    if(pageTable[i].REFERENCED==1)
                        cout << "R";
                    else
                        cout << "-";

                    if(pageTable[i].MODIFIED==1)
                        cout << "M";
                    else
                        cout << "-";
                    
                    if(pageTable[i].PAGEDOUT==1)
                        cout << "S ";
                    else
                        cout << "- ";
                }
                else
                {
                    if(pageTable[i].PAGEDOUT==1)
                    {
                        cout << "# ";
                    }
                    else
                    {
                        cout << "* ";
                    }
                }
            }
            cout << endl;
        }

        void printSummary()
        {
            printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n", pid,
unmaps, maps, ins, outs, fins, fouts, zeros, segv, segprot);
        }
};

vector<Process> processes;

class Frame
{
    public:
        Process *process;
        PTE *vpage;
        int frameNo;
        uint32_t age;
        int tau;
    
    public:
        Frame()
        {
            process = NULL;
            vpage = NULL;
            age = 0;
            tau = 0;
        }

        void setFrame(Process *process, PTE *vpage)
        {
            this->process = process;
            this->vpage = vpage;
        }
};

Frame * FrameTable;
queue<Frame *> FreeList;
int MAX_FRAMES;

void printFrameTable()
{
    cout << "FT: ";
    for(int i=0; i<MAX_FRAMES; i++)
    {
        if(FrameTable[i].process==NULL)
        {
            cout << "* ";
        }
        else
        {
            cout << FrameTable[i].process->pid << ":" << FrameTable[i].vpage->vPageNo << " ";
        }
    }

    cout << endl;
}

class Pager
{
    public:
        virtual Frame* select_victim_frame() = 0;
};

// First in First out
class FIFO: public Pager
{
    int hand;
    public:
        FIFO()
        {
            hand = 0;
        }

        Frame* select_victim_frame()
        {
            Frame *f = &FrameTable[hand];
            hand = (hand + 1) % MAX_FRAMES;
            return f;
        }
};

// Random ... selects the victim frame randomly
class Random: public Pager
{
    public:
        Random()
        {
           
        }

        Frame* select_victim_frame()
        {
            int victim = myrandom(MAX_FRAMES);
            Frame *f = &FrameTable[victim];
            victim++;
            return f;
        }
};

// Clock replacemnt
class Clock: public Pager
{
    int hand;
    public:
        Clock()
        {
            hand = 0;
        }

        Frame* select_victim_frame()
        {
            while(true)
            {
                if(FrameTable[hand].vpage->REFERENCED==0)
                {
                    Frame *f = &FrameTable[hand];
                    hand = (hand+1)%MAX_FRAMES;
                    return f;
                }
                else
                {
                    FrameTable[hand].vpage->REFERENCED=0;
                    hand = (hand+1)%MAX_FRAMES;
                }  
            }
        }
};

//  Not Recently Used
class NRU: public Pager
{
    int hand;
    int instCntRst; // instruction number when last reset
    public:
        NRU()
        {
            hand = 0;
            instCntRst = 0;
        }

        Frame* select_victim_frame()
        {
            Frame *classIndex[4];
            bool reset = false;

            int i;
            
            for(i=0;i<4;i++)
            {
                classIndex[i] = NULL;
            }

            if(instructionCnt - instCntRst >= 50)
            {
                reset = true;
                instCntRst = instructionCnt;
            }

            int frame_class = -1;

            for(i=0; i < MAX_FRAMES; i++)
            {
                frame_class = (2*(int)FrameTable[hand].vpage->REFERENCED) + (int)FrameTable[hand].vpage->MODIFIED;

                if(classIndex[frame_class]==NULL)
                {
                    classIndex[frame_class] = &FrameTable[hand];
                }

                if(reset)
                {
                    FrameTable[hand].vpage->REFERENCED = 0;
                }
                else if(frame_class==0)
                {
                    break;
                }

                hand = (hand + 1) % MAX_FRAMES;
            }

            Frame *victim = NULL;
            for(i=0;i<4;i++)
            {
                if(classIndex[i]!=NULL)
                {
                    hand = (classIndex[i]->frameNo + 1) % MAX_FRAMES;
                    victim = classIndex[i];
                    break;
                }
            }
            return victim;
        }
};

// Aging 
class Aging: public Pager
{
    int hand;
    public:
        Aging()
        {
            hand = 0;
        }

        Frame* select_victim_frame()
        {
            int i;
            Frame *victim = &FrameTable[hand];
            for(i=0; i<MAX_FRAMES; i++)
            {
                FrameTable[hand].age >>= 1;
                if(FrameTable[hand].vpage->REFERENCED==1)
                {
                    FrameTable[hand].age =  FrameTable[hand].age | (uint32_t)(1<<31) ;
                    FrameTable[hand].vpage->REFERENCED = 0;
                }

                if(FrameTable[hand].age < victim->age)
                {
                    victim = &FrameTable[hand];
                }

                hand = (hand + 1) % MAX_FRAMES;
            }

            hand = (victim->frameNo + 1) % MAX_FRAMES;
            return victim;
        }
};

//  Working set
class WorkingSet: public Pager
{
    int hand;
    public:
        WorkingSet()
        {
            hand = 0;
        }

        Frame* select_victim_frame()
        {
            Frame *current = &FrameTable[hand];
            Frame *victim = NULL;

            int minTime = 0;
            int i;
            for(i=0;i<MAX_FRAMES; i++)
            {
                Frame *tmp = &FrameTable[hand];

                int time = instructionCnt - tmp->tau;

                if(tmp->vpage->REFERENCED==1)
                {
                    tmp->tau = instructionCnt;
                    tmp->vpage->REFERENCED = 0;
                }
                else if(time>=50)
                {
                    victim = tmp;
                    break;
                }
                else if(time > minTime)
                {
                    victim = tmp;
                    minTime = time;
                }

                hand = (hand + 1) % MAX_FRAMES;
            }

            if(victim == NULL)
                victim = current;

            hand = (victim->frameNo + 1) % MAX_FRAMES;
            return victim;
        }
};

Pager *pager;

// Instruction
class Instruction
{
    public:
        char command;
        int page;
};

vector<Instruction> instructions;

Process *current_proc;

// Returns the frame to be used to handle page fault
Frame * getFrame()
{
    // Check if some frame is free
    if(!FreeList.empty())
    {
        Frame *f = FreeList.front();
        FreeList.pop();
        return f;
    }
    
    Frame *f = pager->select_victim_frame();
    return f;   
} 

// Tells if the page is associated with some vma of current running process
VMA * isAccessible(int page)
{
    if(current_proc->pageTable[page].vmaSet==1)
    {
        return &current_proc->vmas.at(current_proc->pageTable[page].vma);
    }
    int i=0;

    for(VMA v: current_proc->vmas)
    {
        if(v.starting_virtual_page<= page && v.ending_virtual_page>=page)
        {
            current_proc->pageTable[page].vmaSet=1;
            current_proc->pageTable[page].vma = i;
            return &current_proc->vmas.at(i);
        }

        i++;
    }
    return NULL;
}

//  Simulate the MMU behavior
void simulate()
{
    for(Instruction e: instructions)
    {
        if(Oopt){
            printf("%lu: ==> %c %d\n", instructionCnt,  e.command, e.page);
        }

        instructionCnt++;


        if(e.command=='c')
        {
            current_proc = &processes.at(e.page);
            total_ctx_switch++;
            total_cost += 121;
            continue;
        }
        else if(e.command == 'e')
        {
            printf("EXIT current process %d\n", current_proc->pid);
            int i;
            for(i=0; i<PTE_NUMBER; i++)
            {  
                PTE *p = &current_proc->pageTable[i]; 
                if(p->PRESENT==1)
                {
                    p->PRESENT=0;
                    current_proc->unmaps++;
                    total_cost += 400;

                    if(Oopt)
                    {
                        printf(" UNMAP %d:%d\n", current_proc->pid, p->vPageNo);
                    }

                    FrameTable[p->frameNumber].setFrame(NULL,NULL);
                    FreeList.push(&FrameTable[p->frameNumber]);

                    if(p->MODIFIED==1)
                    {
                        if(p->fileMapped==1)
                        {
                            total_cost += 2500;
                            current_proc->fouts++;
                            printf(" FOUT\n");
                        }
                    }
                }
                p->PAGEDOUT=0;
            }

            
            current_proc = NULL;  
            total_process_exits++;  
            total_cost += 175;
            continue;
        }
        else
        {
            PTE *pte = &current_proc->pageTable[e.page];
            // printFrameTable();
            // current_proc->printPageTable();
            // scanf("%d",&temp);
            if(pte->PRESENT==0)
            {
                VMA *vma;
                vma = isAccessible(e.page);
                
                if(vma!=NULL)
                {
                    // instantiate page ... allocate a frame
                    Frame *frame = getFrame();

                    // unmap that frame from user
                    if(frame->vpage!=NULL)
                    {
                        if(Oopt)
                        {
                            printf(" UNMAP %d:%d\n", frame->process->pid, frame->vpage->vPageNo);
                        }
                        frame->process->unmaps++;
                        total_cost += 400;

                        frame->vpage->PRESENT=0;

                        if(frame->vpage->MODIFIED==1)
                        {
                            // check if it was file mapped in which case write back to file (FOUT)
                            if(frame->vpage->fileMapped==1)
                            {
                                total_cost += 2500;
                                frame->process->fouts++;
                                if(Oopt)
                                    printf(" FOUT\n");
                            }
                            else
                            {
                                frame->process->outs++;
                                total_cost += 3000;
                                frame->vpage->PAGEDOUT = 1;
                                if(Oopt)
                                    printf(" OUT\n");
                            }    
                        }
                    }
        
                //  map the new page to the frame
                    frame->age = 0;
                    frame->tau = instructionCnt;
                    frame->setFrame(current_proc, pte);

                // reset the PTE 

                    pte->PRESENT=1;
                    pte->frameNumber = frame->frameNo;
                    pte->REFERENCED=0;
                    pte->MODIFIED=0;
                    pte->fileMapped = vma->file_mapped;
                    pte->WRITE_PROTECT = vma->write_protected;

                    if(pte->PAGEDOUT==1)
                    {
                        total_cost += 3000;
                        current_proc->ins++;
                        if(Oopt)
                            printf(" IN\n");
                    }
                    else if(pte->fileMapped==1)
                    {
                        current_proc->fins++;
                        total_cost += 2500;
                        if(Oopt)
                            printf(" FIN\n");
                    }
                    else
                    {
                        current_proc->zeros++;
                        total_cost += 150;
                        if(Oopt)
                            printf(" ZERO\n");
                    }

                    if(Oopt)
                    {
                        printf(" MAP %d\n", frame->frameNo);
                    }
                    total_cost += 400;
                    current_proc->maps++;
                
                }
                else
                {
                    total_cost += 240;
                    total_cost += 1;
                    current_proc->segv++;
                    if(Oopt)
                        printf(" SEGV\n");

                    continue;
                }
            }

            if(e.command=='r')
            {
                total_cost += 1;
                pte->REFERENCED=1;
            }   
            else if(e.command=='w')
            {
                total_cost += 1;
                if(pte->WRITE_PROTECT==1)
                {
                    total_cost += 300;
                    current_proc->segprot++;
                    if(Oopt)
                        printf(" SEGPROT\n");
                    pte->REFERENCED=1;
                }
                else
                {   
                    pte->REFERENCED=1;
                    pte->MODIFIED=1;
                }    
            }

        }  
    }

    if(pageTableOpt)
    {
        for(Process p: processes)
        {
            p.printPageTable();
        }
    }

    if(frameTableOpt)
    {
        printFrameTable();
    }

    if(summaryOpt)
    {
        for(Process p: processes)
        {
            p.printSummary();
        }
        printf("TOTALCOST %lu %lu %lu %llu\n", instructionCnt, total_ctx_switch, total_process_exits, total_cost);
    }
}


int main(int argc, char *argv[])
{
    // initialize global variables
    total_cost = 0;
    total_ctx_switch = 0;
    total_process_exits = 0;
    instructionCnt = 0;

    // Read command line options
    int option;
    while((option = getopt (argc,argv,"f:a:o:")) != -1)
    {
        switch(option)
        {
            case 'o':   if(optarg!=NULL)
                        {
                            string s = optarg;
                            for(int i=0; i<s.size();i++)
                            {
                                switch(s[i])
                                {
                                    case 'F':   frameTableOpt = true;
                                                break;
                                    case 'P':   pageTableOpt = true;
                                                break;
                                    case 'O':   Oopt = true;
                                                break;
                                    case 'S':   summaryOpt = true;
                                                break;
                                }
                            }
                        }
                        break;

            case 'f':   MAX_FRAMES = stoi(optarg);
                        break;

            case 'a':   if(optarg!=NULL)
                        {
                            switch(optarg[0])
                            {
                                case 'f':   pager = new FIFO();
                                            break;
                                case 'r':   pager = new Random();
                                            break;
                                case 'c':   pager = new Clock();
                                            break;
                                case 'e':   pager = new NRU();
                                            break;
                                case 'a':   pager = new Aging();
                                            break;
                                case 'w':   pager = new WorkingSet();
                                            break;
                            }
                        }  

                        break;

            case '?':   if(optopt == 'o')
                            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                        break;
            default:    cout << "nothing" << endl;
                        break;
        }
    }

// Read file names from command line 
    string fileName = argv[optind];
    string randFileName = argv[optind+1];

// Reads the random number file
    readRandom(randFileName);

//  Allcate frame table
    FrameTable = new Frame[MAX_FRAMES];

    for(int i=0; i<MAX_FRAMES; i++)
    {
        FrameTable[i] = *(new Frame);
        FrameTable[i].frameNo = i;
        FrameTable[i].process = NULL;
        FrameTable[i].vpage = NULL;
        FreeList.push(&FrameTable[i]);
    }

    ifstream file;
    file.open(fileName);
    string line;
    int i=0;
    int numProcess;

    while(getline(file,line))
    {   
        if(line.at(0)!='#')
        {
            char ch_str[line.size()+1];
            strcpy(ch_str, &line[0]);
            if(i==0)
            {
                sscanf(ch_str,"%d",&numProcess);
                i++;
                continue;
            }
            else if(i<=numProcess)
            {
                int numVMA;
                sscanf(ch_str,"%d",&numVMA);
                Process process(i-1);
                
                for(int j=0; j<numVMA; j++)
                {
                    do
                    {
                        getline(file,line);
                        
                    } while (line.at(0)=='#');
                    char ch_str1[line.size()+1];
                    strcpy(ch_str1, &line[0]);
                    int svp, evp, wp, fm;
                    sscanf(ch_str1,"%d%d%d%d",&svp, &evp, &wp, &fm);  
                    VMA vma(svp,evp,wp,fm);  
                    process.vmas.push_back(vma);
                }
                processes.push_back(process);
                i++;
            }
            else
            {
                char c;
                int p;
                sscanf(ch_str,"%c%d",&c,&p);
                Instruction e;
                e.command = c;
                e.page = p;
                instructions.push_back(e); 
            }  
        }
    }

    simulate();

    return 0;
}
