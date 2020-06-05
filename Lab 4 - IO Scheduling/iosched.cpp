#include<iostream>
#include<getopt.h>
#include<fstream>
#include<stdio.h>
#include<deque>
#include<unistd.h>
#include<string.h>

using namespace std;

bool vopt;
bool qopt;
bool fopt;
int total_time;
int total_movement;
int total_turnaround;
int total_wait_time;
int max_wait_time;

class Operation
{
    public:
        Operation(int time, int track)
        {
            this->time = time;
            this->track = track;
        }

        int time;
        int track;
};

deque<Operation> operations;

int curr_time;
int curr_track;
int curr_dir;

class Request
{
    public:
        int req_no;
        int arrival_time;
        int start_time;
        int end_time;
        int req_track;

        Request(int req_no, int track)
        {
            this->req_no = req_no;
            this->req_track = track;
            this->arrival_time = curr_time;
        }        
};

class IOScheduler 
{
    public:
        deque<Request *> IOQueue;
        virtual Request * strategy() = 0;
        virtual void removeRequest() = 0;
        virtual void addRequest(Request *r) = 0;
        virtual bool isEmpty() = 0;

        void printQueue()
        {
            int pos = 0;
            for(deque<Request *>::iterator it = IOQueue.begin(); it != IOQueue.end(); ++it)
            {
                pos++;
                Request *req = *it;

                int diff = curr_track - req->req_track;
                if(diff<0)
                    printf("%d:%d:%d %d",req->req_no, req->req_track, 1, diff);
                else
                    printf("%d:%d:%d %d",req->req_no, req->req_track, -1, diff);

                printf("\n");             
            }
        }
};

class FIFO : public IOScheduler
{
    public:
        FIFO()
        {

        }

        Request * strategy()
        {
            Request *r = IOQueue.front();

            if(curr_track > r->req_track)
            {
                curr_dir = -1;
            }
            else
            {
                curr_dir = 1;
            }
            return r;
        }

        void addRequest(Request *r)
        {
            IOQueue.push_back(r);  
        }

        void removeRequest()
        {
            IOQueue.pop_front();
        }

        bool isEmpty()
        {
            return IOQueue.empty();
        }
};

class SSTF : public IOScheduler
{
    int curr_index;
    public:
        SSTF()
        {

        }

        Request * strategy()
        {
            int pos = 0;
            int min_at = 0;
            int min_diff = -1;
            int dist;
            for(deque<Request *>::iterator it = IOQueue.begin(); it != IOQueue.end(); ++it)
            {
                pos++;
                Request *req = *it;

                dist = abs(curr_track - req->req_track);
                // cout << dist << endl;    
                if(min_diff==-1)
                {
                    min_at = pos-1;
                    min_diff = dist;
                }
                if(min_diff>dist)
                {
                    min_at = pos-1;
                    min_diff = dist;
                }
            }
            curr_index = min_at;
            Request *r = IOQueue.at(min_at);

            if(curr_track > r->req_track)
            {
                curr_dir = -1;
            }
            else
            {
                curr_dir = 1;
            }
            return r;   
        }

        void addRequest(Request *r)
        {
            IOQueue.push_back(r);
        }

        void removeRequest()
        {
            deque<Request *>::iterator it = IOQueue.begin();
            advance(it,curr_index);
            IOQueue.erase(it);
        }

        bool isEmpty()
        {
            return IOQueue.empty();
        }
};

class Look : public IOScheduler
{
    private:
        int curr_index;
    public:
        Look()
        {
            curr_index = 0;
        }

        Request * strategy()
        {
            int pos = 0;
            int min_at = 0;
            int min_diff = -1;
            int dist;
            bool moreInThisDir = false;
            for(deque<Request *>::iterator it = IOQueue.begin(); it != IOQueue.end(); ++it)
            {
                pos++;
                Request *req = *it;

                dist = req->req_track - curr_track;
                
                if(curr_dir==1 && dist>=0)
                {
                    moreInThisDir = true;
                    if(min_diff==-1)
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }

                    if(min_diff>abs(dist))
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }
                }
                else if(curr_dir==-1 && dist<=0)
                {
                    moreInThisDir = true;
                    if(min_diff==-1)
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }

                    if(min_diff>abs(dist))
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }
                }  
            }

            if(!moreInThisDir)
            {
                if(curr_dir==-1)
                    curr_dir = 1;
                else
                    curr_dir = -1;
                
                return this->strategy();
            }

            curr_index = min_at;
            Request *r = IOQueue.at(min_at);

            return r; 
        }

        void addRequest(Request *r)
        {
            IOQueue.push_back(r);
        }

        void removeRequest()
        {
            deque<Request *>::iterator it = IOQueue.begin();
            advance(it,curr_index);
            IOQueue.erase(it);
        }

        bool isEmpty()
        {
            return IOQueue.empty();
        }
};

class CLook : public IOScheduler
{
    private:
        int curr_index;
    public:
        CLook()
        {

        }

        Request * strategy()
        {
            int pos = 0;
            int min_at = 0;
            int min_diff = -1;
            int dist;
            bool moreInThisDir = false;
            int min_track = curr_track;
            int min_track_pos = -1;

            for(deque<Request *>::iterator it = IOQueue.begin(); it != IOQueue.end(); ++it)
            {
                pos++;
                Request *req = *it;

                dist = req->req_track - curr_track;

                if(dist>=0)
                {
                    moreInThisDir = true;
                    if(min_diff==-1)
                    {
                        min_at = pos - 1;
                        min_diff = abs(dist);
                    }

                    if(min_diff > abs(dist))
                    {
                        min_at = pos - 1;
                        min_diff = abs(dist);
                    }
                }

                if(min_track > req->req_track)
                {
                    min_track = req->req_track;
                    min_track_pos = pos - 1;
                }
            }

            if(!moreInThisDir)
            {
                curr_dir = -1;
                curr_index = min_track_pos;
            }
            else
            {
                curr_dir = 1;
                curr_index = min_at;   
            }
            
            Request *r = IOQueue.at(curr_index);
            return r;
        }

        void addRequest(Request *r)
        {
            IOQueue.push_back(r);
        }

       void removeRequest()
        {
            deque<Request *>::iterator it = IOQueue.begin();
            advance(it,curr_index);
            IOQueue.erase(it);
        } 

        bool isEmpty()
        {
            return IOQueue.empty();
        }
};

class FLook : public IOScheduler
{
    private:
        deque<Request *> newQueue;
        deque<Request *> *active;
        deque<Request *> *add;
        int curr_index;
    public:
        FLook()
        {
            active = &IOQueue;
            add = &newQueue;
        }

        Request * strategy()
        {
            if(active->empty())
            {
                deque<Request *> *temp;
                temp = active;
                active = add;
                add = temp;
            }

            int pos = 0;
            int min_at = 0;
            int min_diff = -1;
            int dist;
            bool moreInThisDir = false;
            for(deque<Request *>::iterator it = active->begin(); it != active->end(); ++it)
            {
                pos++;
                Request *req = *it;

                dist = req->req_track - curr_track;
                
                if(curr_dir==1 && dist>=0)
                {
                    moreInThisDir = true;
                    if(min_diff==-1)
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }

                    if(min_diff>abs(dist))
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }
                }
                else if(curr_dir==-1 && dist<=0)
                {
                    moreInThisDir = true;
                    if(min_diff==-1)
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }

                    if(min_diff>abs(dist))
                    {
                        min_at = pos-1;
                        min_diff = abs(dist);
                    }
                }  
            }

            if(!moreInThisDir)
            {
                if(curr_dir==-1)
                    curr_dir = 1;
                else
                    curr_dir = -1;
                
                return this->strategy();
            }

            curr_index = min_at;
            Request *r = active->at(min_at);

            return r;
        }

        void addRequest(Request *r)
        {
            add->push_back(r);  

            if(active->empty())
            {
                deque<Request *> *temp;
                temp = active;
                active = add;
                add = temp;
            }
        }

        void removeRequest()
        {
            deque<Request *>::iterator it = active->begin();
            advance(it,curr_index);
            active->erase(it);
        }

        bool isEmpty()
        {
            if(add->empty() && active->empty())
            {
                return true;
            }
            else
            {
                if(active->empty())
                {
                    deque<Request *> *temp;
                    temp = active;
                    active = add;
                    add = temp;
                }

                return false;
            }
            
        }
};

IOScheduler *scheduler;
Request *curr_req;

deque<Request> completedRequests;

void addCompleted(Request r)
{
    int flag=0;
    int pos=0;
    for(deque<Request>::iterator it = completedRequests.begin(); it != completedRequests.end(); ++it)
    {
        pos++;

        if(it->req_no > r.req_no)
        {
            flag = 1;
            break;
        }
    }

    if(flag==0)
    {
        completedRequests.push_back(r);
    }
    else
    {
        deque<Request>::iterator it = completedRequests.begin();
        advance(it,pos-1);
        completedRequests.insert(it,r);
    }
}


void printProcessSummary()
{
    for(deque<Request>::iterator it = completedRequests.begin(); it != completedRequests.end(); ++it)
    {
        printf("%5d: %5d %5d %5d\n",it->req_no, it->arrival_time, it->start_time, it->end_time);   
    }
}

void simulate()
{
    int req_no = 0;
    bool flag = false;

    while(1)
    {
        // If a new I/O arrived to the system at this current time → add request to IO-queue 

        if(!operations.empty() && operations.front().time == curr_time)
        {   
            scheduler->addRequest(new Request(req_no, operations.front().track));
            
            if(vopt)
                printf("%5d: %d add %d\n",curr_time,req_no,operations.front().track);

            operations.pop_front();
            req_no++;

        }

        // If an IO is active and completed at this time → Compute relevant info and store in IO request for final summary

        if(curr_req!=NULL && curr_req->req_track==curr_track)
        {
            curr_req->end_time = curr_time;
            total_turnaround += curr_time - curr_req->arrival_time;

            if(vopt)
                printf("%5d: %d finish %d\n",curr_time,curr_req->req_no,curr_time - curr_req->arrival_time);

            Request completed(curr_req->req_no,curr_req->req_track);
            completed.arrival_time = curr_req->arrival_time;
            completed.end_time = curr_req->end_time;
            completed.start_time = curr_req->start_time;
            addCompleted(completed);


            // printf("%5d: %5d %5d %5d\n",curr_req->req_no, curr_req->arrival_time, curr_req->start_time, curr_req->end_time);
            if(curr_req->req_no==3)
                flag = true;
            scheduler->removeRequest();
            curr_req = NULL;

        }

         // If no IO request active now (after (2)) but IO requests are pending → Fetch the next request and start the new IO.

        if(curr_req==NULL && !scheduler->isEmpty())
        {
            curr_req = scheduler->strategy();
            // cout << "curr_dir = " << curr_dir << endl;

            
            if(curr_req!=NULL)
            {
                if(vopt)
                    printf("%5d: %d issue %d %d \n",curr_time,curr_req->req_no,curr_req->req_track, curr_track);

                curr_req->start_time = curr_time;
                total_wait_time += curr_time - curr_req->arrival_time;

                if(curr_time - curr_req->arrival_time > max_wait_time)
                    max_wait_time = curr_time - curr_req->arrival_time;

                if(curr_req->req_track == curr_track)
                    curr_time--;
            }
            else
            {
                curr_dir = -1;
                curr_track --;
                total_movement++;
                total_wait_time--;

                if(curr_track==0)
                    curr_dir = 1;
            }
            
            
        }

        // If an IO is active but did not yet complete→Move the head by one sector/track/unit in the direction it is going (to simulate seek)

        if(curr_req!=NULL && curr_req->req_track!=curr_track)
        {
            if(curr_dir==1)
            {
                // cout << curr_time << " " << curr_track << endl;
                curr_track++;
            }
            else
            {
                curr_track--;
            }

            total_movement++;
        }

        // If no IO request is active now and no IO requests pending→exit simulation

        if(curr_req==NULL && scheduler->isEmpty() && operations.empty())
        {
            break;
        }        

        // Increment time by 1
        curr_time++;        
    }

    printProcessSummary();

    // Printing the summary
    printf("SUM: %d %d %.2lf %.2lf %d\n",curr_time, total_movement, double(total_turnaround)/req_no, double(total_wait_time)/req_no, max_wait_time);
}

void readFile(string fileName)
{
    ifstream file;
    file.open(fileName);
    string line;
    // cout << "inside readFile" << endl;
    while(getline(file,line))
    {   
        if(line.at(0)!='#')
        {
            char ch_str[line.size()+1];
            strcpy(ch_str, &line[0]);
            int time;
            int track;
            // cout << ch_str << endl;    
            sscanf(ch_str, "%d %d", &time, &track);

            Operation o(time,track);
            operations.push_back(o);       
        }
    }

    // for(auto it : operations)
    // {
    //     printf("%d %d \n",it.time, it.track);
    // }
}

int main(int argc, char *argv[])
{
     // Read command line options
    int option;
    while((option = getopt (argc,argv,"vqfs:")) != -1)
    {
        switch(option)
        {
            case 'v':   vopt = true;
                        break;

            case 'q':   qopt = true;
                        break;

            case 's':   if(optarg!=NULL)
                        {
                            switch(optarg[0])
                            {
                                case 'i':   scheduler = new FIFO();
                                            break;
                                case 'j':   scheduler = new SSTF();
                                            break;
                                case 's':   scheduler = new Look();
                                            break;
                                case 'c':   scheduler = new CLook();
                                            break;
                                case 'f':   scheduler = new FLook();
                                            break;
                            }
                        }  
                        break;

            case 'f':   fopt = true;
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

    // cout << "reading a file" << endl;
    readFile(fileName);    
    
    curr_time = 0;
    curr_track = 0;
    curr_dir = 1;
    curr_req = NULL;
    simulate();
}
