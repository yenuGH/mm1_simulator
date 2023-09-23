#include<stdio.h>
#include<time.h>
#include<math.h>
#include<stdlib.h>
#include<unistd.h>
#include<assert.h>
#include<float.h>


// Definition of a Queue Node including arrival and service time
struct QueueNode {
    double arrival_time;  // customer arrival time, measured from time t=0, inter-arrival times exponential
    double service_time;  // customer service time (exponential) 
    struct QueueNode *next;  // next element in line; NULL if this is the last element
};

// Suggested queue definition
// Feel free to use some of the functions you implemented in HW2 to manipulate the queue
// You can change the queue definition and add/remove member variables to suit your implementation
struct Queue {
    struct QueueNode* head;    // Point to the first node of the element queue
    struct QueueNode* tail;    // Point to the tail node of the element queue
    struct QueueNode* free;    // Point to the first node to free the list

    struct QueueNode* first;  // Point to the first arrived customer that is waiting for service
    struct QueueNode* last;   // Point to the last arrrived customer that is waiting for service
    int waiting_count;     // Current number of customers waiting for service

    double last_departure_time;  // Time of the last departure from the queue

    double cumulative_response;  // Accumulated response time for all effective departures
    double cumulative_waiting;  // Accumulated waiting time for all effective departures
    double cumulative_idle_times;  // Accumulated times when the system is idle, i.e., no customers in the system
    double cumulative_area;   // Accumulated number of customers in the system multiplied by their residence time, for E[n] computation
};

// Definition of an Event Node including event type and time
struct EventNode {
    char event_type;  // 'a' for arrival, 's' for start_service, 'd' for departure
    double event_time;  // event time, measured from time t=0
    struct EventNode *next;  // next element in line; NULL if this is the last element
};

// Definition of a priority queue for EventNodes
// EventNode->event_time is used as the priority: smaller event_time has higher priority
struct EventList {
    struct EventNode* head;    // Point to the first node of the event queue.
    struct EventNode* tail;    // Point to the tail node of the event queue
};

// ------------Global variables----------------------------------------------------------------------
// Feel free to add or remove. 
// You could make these local variables for cleaner code, but you need to change function definitions 
static double computed_stats[4];  // Store computed statistics: [E(n), E(r), E(w), p0]
static double simulated_stats[4]; // Store simulated statistics [n, r, w, sim_p0]

int departure_count = 0;         // current number of departures from queue
double last_departure_time = 0;  // time of the last departure from queue

double current_time = 0;          // current time during simulation
double last_event_time = 0;       // time of the last event during simulation

struct EventList *eventList;      // Event list

//-----------------Queue Functions--------------------------------------------------------------------
// Feel free to add more functions or redefine the following functions
struct QueueNode *CreateQueueNode(double arrival_time, double service_time){
    struct QueueNode *queueNode = malloc(sizeof(struct QueueNode));
    queueNode->arrival_time = arrival_time;
    queueNode->service_time = service_time;
    queueNode->next = NULL;
    return queueNode;
}

void InsertQueueNode(struct Queue *elementQ, double arrival_time, double service_time){
    struct QueueNode *queueNode = CreateQueueNode(arrival_time, service_time);
    if(elementQ->head == NULL){
        elementQ->head = queueNode;
        elementQ->tail = queueNode;
    }
    else{
        elementQ->tail->next = queueNode;
        elementQ->tail = queueNode;
    }
}

struct QueueNode *PopQueueNode(struct Queue *elementQ){
    if(elementQ->head == NULL){
        return NULL;
    }
    else{
        struct QueueNode *queueNode = elementQ->head;
        elementQ->head = elementQ->head->next;
        return queueNode;
    }
}

struct EventNode *CreateEventNode(char event_type, double event_time){
    struct EventNode *eventNode = malloc(sizeof(struct EventNode));
    eventNode->event_type = event_type;
    eventNode->event_time = event_time;
    eventNode->next = NULL;
    return eventNode;
}

void InsertEventNode(struct EventList *eventList, char event_type, double event_time){
    // the event nodes's time will be considered the priority for this priority queue
    // the smaller the time value, the higher the priority

    struct EventNode *eventNode = CreateEventNode(event_type, event_time);

    // if the event list is empty, we need to set the head and tail to the new node
    if (eventList->head == NULL){
        eventList->head = eventNode;
        eventList->tail = eventNode;
        //eventList->free = eventNode;
    }
    // otherwise, we need to traverse the list until we find a node with a larger time value
    else{
        // if the new event node's time is less than the head's time
        if (eventNode->event_time < eventList->head->event_time){
            eventNode->next = eventList->head;
            eventList->head = eventNode;
        }
        // otherwise, we need to traverse the list until we find a node with a larger time value
        else{
            struct EventNode *current = eventList->head;
            // we need to stop when the current node's next is NULL or when the current node's next time is greater than the new node's time
            // we check current->next as if the current node's time is bigger, we can't go back to the previous node
            while (current->next != NULL && current->next->event_time < eventNode->event_time){
                current = current->next;
            }
            // set the new node's next to the current node's next
            eventNode->next = current->next;
            // set the current node's next to the new node
            current->next = eventNode;
        }
    }
}

struct EventNode *PopEventNode(){
    // if the event list is empty, return NULL
    if (eventList->head == NULL){
        return NULL;
    }
    // otherwise, we need to pop the head node
    else{
        struct EventNode *eventNode = eventList->head;
        eventList->head = eventList->head->next;
        return eventNode;
    }
}

double generateRandomNumber(){
    // when calling rand(), there lies a probability where it may return 0 or 1
    // this will result in some sort of unstable system????
    double randomNumber = 0;
    while (randomNumber == 0 || randomNumber == 1){
        randomNumber = (double) rand() / (double) RAND_MAX;
    }
    return randomNumber;
}

// The following function initializes all "D" (i.e., total_departure) elements in the queue
// 1. It uses the seed value to initialize random number generator
// 2. Generates "D" exponentially distributed inter-arrival times based on lambda
//    And inserts "D" elements in queue with the correct arrival times
//    Arrival time for element i is computed based on the arrival time of element i+1 added to element i's generated inter-arrival time
//    Arrival time for first element is just that element's generated inter-arrival time
// 3. Generates "D" exponentially distributed service times based on mu
//    And updates each queue element's service time in order based on generated service times
// 4. Returns a pointer to the generated queue
struct Queue* InitializeQueue(int seed, double lambda, double mu, int total_departures){
    // Recall:
    // seed is the value we use for srand()
    // lambda = arrival rate
    // mu = service rate
    // total_departures = D

    srand(seed);
    struct Queue *elementQ = malloc(sizeof(struct Queue));

    elementQ->head = NULL;
    elementQ->tail = NULL;
    elementQ->first = NULL;
    elementQ->last = NULL;

    elementQ->waiting_count = 0;
    elementQ->cumulative_response = 0;
    elementQ->cumulative_waiting = 0;
    elementQ->cumulative_idle_times = 0;
    elementQ->cumulative_area = 0;

    double arrival_time = 0;
    double interarrival_time = 0;
    double service_time = 0;

    for (int i = 0; i <= total_departures; i++){
        interarrival_time = -logf(1.0f - generateRandomNumber())/lambda; 
        service_time = -logf(1.0f - generateRandomNumber()) / mu; 

        // regenerate if values are 0 or 1
        while (interarrival_time == 0 || interarrival_time == 1){
            // use the inverse transform method
            interarrival_time = -logf(1.0f - generateRandomNumber())/lambda; 
        }
        while (service_time == 0 || service_time == 1){
            service_time = -logf(1.0f - generateRandomNumber()) / mu;
        }

        // update the arrival time for the current node
        // even the first node will have an arrival time that is no 0
        arrival_time += interarrival_time;

        // insert the node into the queue
        InsertQueueNode(elementQ, arrival_time, service_time);
    }

    // we set first and last to the first element for now
    // this will update as the simulation is run
    elementQ->first = elementQ->head;
    elementQ->last = elementQ->head;
    elementQ->free = elementQ->head;

    return elementQ;
}

// Use the M/M/1 formulas from class to compute E(n), E(r), E(w), p0
void GenerateComputedStatistics(double lambda, double mu){
    // If the parameters allow for a stable system
    // a stable system is when p = lambda/mu < 1
    double p = lambda/mu;

    // recall:
    // computed_stats[0] = E(n)
    // computed_stats[1] = E(r)
    // computed_stats[2] = E(w)
    // computed_stats[3] = p0

    // stable system
    if (p < 1) {
        computed_stats[0] = p / (1 - p);        // E(n)
        computed_stats[1] = 1 / (mu * (1 - p)); // E(r)
        computed_stats[2] = 1 / (mu * (1 - p)); // E(w)
        computed_stats[3] = 1 - p;              // p0
    }
    // unstable system
    else {
        computed_stats[0] = INFINITY;           // E(n)
        computed_stats[1] = INFINITY;           // E(r)
        computed_stats[2] = INFINITY;           // E(w)
        computed_stats[3] = 0;                  // p0
    }
}

// This function should be called to print periodic and/or end-of-simulation statistics
// Do not modify output format
void PrintStatistics(struct Queue* elementQ, int total_departures, int print_period, double lambda){
    // compute the simulated statistics
    simulated_stats[0] = (double) elementQ->cumulative_area / (double) current_time;
    simulated_stats[1] = (double) elementQ->cumulative_response / (double) departure_count;
    simulated_stats[2] = (double) elementQ->cumulative_waiting / (double) departure_count;
    simulated_stats[3] = (double) elementQ->cumulative_idle_times / (double) current_time;

    printf("\n");
    if(departure_count == total_departures) printf("End of Simulation - after %d departures\n", departure_count);
    else printf("After %d departures\n", departure_count);

    printf("Mean n = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[0], computed_stats[0]);
    printf("Mean r = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[1], computed_stats[1]);
    printf("Mean w = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[2], computed_stats[2]);
    printf("p0 = %.4f (Simulated) and %.4f (Computed)\n", simulated_stats[3], computed_stats[3]);
}

/*     double cumulative_response;  // Accumulated response time for all effective departures
    double cumulative_waiting;  // Accumulated waiting time for all effective departures
    double cumulative_idle_times;  // Accumulated times when the system is idle, i.e., no customers in the system
    double cumulative_area;   // Accumulated number of customers in the system multiplied by their residence time, for E[n] computation */

/* ---------------------------- Event prototypes ---------------------------- */
struct QueueNode* ProcessArrival(struct Queue* elementQ, struct QueueNode* arrival);
void StartService(struct Queue* elementQ);
void ProcessDeparture(struct Queue* elementQ, struct QueueNode* arrival);

// This function is called from simulator if the next event is an arrival
// Should update simulated statistics based on new arrival
// Should update current queue nodes and various queue member variables
// *arrival points to queue node that arrived
// Returns pointer to node that will arrive next
struct QueueNode* ProcessArrival(struct Queue* elementQ, struct QueueNode* arrival){
    //push arrival of next element in queue
    if(arrival->next->next != NULL){
        InsertEventNode(eventList, 'a', arrival->next->arrival_time);
    }
    //increase count of people in queue
    elementQ->waiting_count++;

    if(elementQ->waiting_count == 1){
        StartService(elementQ);
    }

    return arrival->next;
}

// This function is called from simulator if next event is "start_service"
//  Should update queue statistics
void StartService(struct Queue* elementQ){
    // grab customer at head of the queue
    struct QueueNode* customer = elementQ->head;

    if (last_departure_time > customer->arrival_time){
        InsertEventNode(eventList, 'd', last_departure_time + customer->service_time);
        last_departure_time = last_departure_time + customer->service_time;

        // If a node departs after the next node arrives, the queue is busy
        elementQ->cumulative_waiting += (last_departure_time - customer->arrival_time);
    }
    if (last_departure_time < customer->arrival_time){
        InsertEventNode(eventList, 'd', customer->arrival_time + customer->service_time);
        last_departure_time = customer->arrival_time + customer->service_time;

        // If a node departs before the next node arrives, the queue is idle.
        // The idle time is the time between the departure and the arrival of the next node.
        // The idle time is added to the cumulative idle time.
        elementQ->cumulative_idle_times += (last_departure_time - customer->arrival_time);
    }

    // update the queue statistic variables
    elementQ->cumulative_response += (last_departure_time - customer->arrival_time);
}

// This function is called from simulator if the next event is a departure
// Should update simulated queue statistics 
// Should update current queue nodes and various queue member variables
void ProcessDeparture(struct Queue* elementQ, struct QueueNode* arrival){
    // pop arrival of next element in queue
    if(arrival->next != NULL){
        PopQueueNode(elementQ);
    }

    //decrease count of people in queue
    elementQ->waiting_count--;
    departure_count++;
    
    if(elementQ->waiting_count > 0){
        StartService(elementQ);
    }
}

// This is the main simulator function
// Should run until departure_count == total_departures
// Determines what the next event is based on current_time
// Calls appropriate function based on next event: ProcessArrival(), StartService(), ProcessDeparture()
// Advances current_time to next event
// Updates queue statistics if needed
// Print statistics if departure_count is a multiple of print_period
// Print statistics at end of simulation (departure_count == total_departures) 
void Simulation(struct Queue* elementQ, double lambda, double mu, int print_period, int total_departures){
    // Initialize the event list
    eventList = malloc(sizeof(struct EventList));

    // We initialize and insert our first event which is an arrival
    InsertEventNode(eventList, 'a', elementQ->first->arrival_time);

    // then we initialize the free node in event list to the head of eventlist
    //eventList->free = eventList->head;

    // This node will be used to contain events popped out of the event list
    //int counter = 0;
    while (departure_count <= total_departures && eventList->head != NULL) {
        // Your simulator code here

        // Pop the earliest event off the event list and process the type of event
        struct EventNode *event = PopEventNode();

        // extract info from the event node so we can free it
        char event_type = event->event_type;
        current_time = event->event_time;
        free(event);

        // If the event is an arrival
        if (event_type == 'a'){
            // Process the arrival of a node
            // Now, the first node's next is returned from ProcessArrival
            elementQ->first->next = ProcessArrival(elementQ, elementQ->first);

            // Now we need to increment to the next "first" node to receive service in the queue
            elementQ->first = elementQ->first->next;
        }
        // If the event is a departure
        if (event_type == 'd'){
            // Process the departure of a node
            ProcessDeparture(elementQ, elementQ->last);

            // Now, the last event should be updated to the event that just departed
            elementQ->last = elementQ->last->next;
        }

        // As we calculate the cumulative response, 
        // the cumulative area is the cumulative response
        elementQ->cumulative_area = elementQ->cumulative_response;
        
        // Once we process an event, it will become the last event
        // As such, we update the last event time to the current time
        last_event_time = current_time;

        // Print Periodic Statistics
        if ((departure_count % print_period) == 0 && departure_count != total_departures) {
            PrintStatistics(elementQ, total_departures, print_period, lambda);
        }
    }

    current_time = last_departure_time;

    // Print Statistics at end of simulation
    PrintStatistics(elementQ, total_departures, print_period, lambda);
}

// Free memory allocated for queue nodes at the end of simulation
void FreeQueue(struct Queue* elementQ){
    while (elementQ->free->next != NULL) {
        struct QueueNode *temp = elementQ->free;
        elementQ->free = elementQ->free->next;
        free(temp);
    }
    free(elementQ->free);
    free(elementQ);
}

// Free memory allocated for event nodes at the end of simulation
void FreeEventList(){
    while (eventList->head != NULL){
        struct EventNode *temp = eventList->head;
        eventList->head = eventList->head->next;
        free(temp);
    }
    free(eventList->head);
    free(eventList);
}

// Program's main function
int main(int argc, char* argv[]){
    
    // input arguments lambda, mu, P, D, S
    if(argc >= 6){
		double lambda = atof(argv[1]);
		double mu = atof(argv[2]);
		int print_period = atoi(argv[3]);
		int total_departures = atoi(argv[4]);
		int random_seed = atoi(argv[5]);
        
        // Add error checks for input variables here, exit if incorrect input
        if (lambda <= 0 || mu <= 0 || print_period <= 0 || total_departures <= 0 || random_seed <= 0){
            printf("Invalid argument(s) provided!\n");
            return 0;
        }
        
        // If no input errors, generate M/M/1 computed statistics based on
        // formulas from class
        GenerateComputedStatistics(lambda, mu);

        // Start Simulation
        printf("Simulating M/M/1 queue with lambda = %f, mu = %f, P = %d, D = %d, ""S = %d:\n\n", lambda, mu, print_period, total_departures, random_seed);

        struct Queue* elementQ = InitializeQueue(random_seed, lambda, mu, total_departures);

        Simulation(elementQ, lambda, mu, print_period, total_departures);
        
        FreeQueue(elementQ);
        FreeEventList();
    }
    else{
        printf("Insufficient number of arguments provided!\n");
    }

    return 0;
}
