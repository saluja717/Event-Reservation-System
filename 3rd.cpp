#include <iostream>
#include <pthread.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

using namespace std;

#define random_interval ((rand()%1000)+1)*1000

const int MAX_EVENTS = 100;
const int AUDITORIUM_CAPACITY = 500;
const int NUM_WORKER_THREADS = 20;
const int MAX_ACTIVE_QUERIES = 5;
const int MAX_RUNNING_TIME_MINUTES = 1;
const int MAX_TICKETS_BOOKED_TOGETHER = 10;
const int MAX_ATTEMPTS = 5;

pthread_mutex_t mutex_shared_table = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_active_queries = PTHREAD_COND_INITIALIZER;

struct Query {
    int event_number;
    int query_type; // 1: Inquire, 2: Book, 3: Cancel
    int thread_number;
    int k; // For Booking
};

int e, availality[MAX_EVENTS];
bool should_run;

// Shared table to manage active queries
vector<Query> shared_table;

void* worker_thread_function(void* arg);
void execute_query(Query query);
void print_reservation_status();

int main() {
    // Set events
    cout << "Enter number of events: ";
    cin >> e;
    if (e > MAX_EVENTS) {
        cerr << "Events limit exceeded" << endl;
        return 1;
    }
    for (int i = 0; i < e; i++) {
        availality[i] = AUDITORIUM_CAPACITY;
    }

    // Setup and execute worker threads
    srand(time(NULL));
    should_run = true;

    pthread_t worker_threads[NUM_WORKER_THREADS];

    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        pthread_create(&worker_threads[i], NULL, worker_thread_function, (void*)(intptr_t)i);
        usleep(100);
    }

    sleep(MAX_RUNNING_TIME_MINUTES * 60);

    // Terminate worker threads
    should_run = false;
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        pthread_join(worker_threads[i], NULL);
    }

    print_reservation_status();

    return 0;
}

void* worker_thread_function(void* arg) {
    int thread_number = (intptr_t)arg;
    cout << "Thread " << thread_number << " started" << endl;

    while (should_run) {
        // Generate random query
        Query query;
        query.event_number = rand() % e;
        query.query_type = rand() % 3 + 1; // 1: Inquire, 2: Book, 3: Cancel
        query.thread_number = thread_number;

        if (query.query_type == 2) {
            query.k = rand() % MAX_TICKETS_BOOKED_TOGETHER + 1;
        }

        // Execute query
        execute_query(query);
        usleep(random_interval);
    }
    cout << "Thread " << thread_number << " ended" << endl;
    return NULL;
}

void execute_query(Query query) {
    bool query_successful = false;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        // Acquire lock for shared table
        pthread_mutex_lock(&mutex_shared_table);
        while (shared_table.size() >= MAX_ACTIVE_QUERIES) {
            pthread_cond_wait(&cond_active_queries, &mutex_shared_table);
        }

        // Acquire lock for reading/writing
        bool is_write_locked = false;
        bool is_read_locked = false;
        for (const Query& q : shared_table) {
            if (q.event_number == query.event_number) {
                if (q.query_type == 2 || q.query_type == 3) {
                    is_write_locked = true;
                    break;
                } else if (q.query_type == 1) {
                    is_read_locked = true;
                }
            }
        }
        if (is_write_locked || (is_read_locked && (query.query_type == 2 || query.query_type == 3))) {
            pthread_mutex_unlock(&mutex_shared_table);
            usleep(random_interval * attempt); 
            continue; 
        }

        // Add query to shared table
        shared_table.push_back(query);
        pthread_mutex_unlock(&mutex_shared_table);

        // Random interval
        usleep(random_interval);

        // Perform query task
        switch (query.query_type) {
            case 1: { // Inquire
                cout << "Thread " << query.thread_number << " inquires about event " << query.event_number << " Available Seats: " << availality[query.event_number] << endl;
                query_successful = true;
                break;
            }
            case 2: { // Book
                if (availality[query.event_number] > query.k) {
                    cout << "Thread " << query.thread_number << " books " << query.k << " tickets for event " << query.event_number << endl;
                    availality[query.event_number] -= query.k;
                    query_successful = true;
                } else {
                    cout << "Thread " << query.thread_number << " failed to book tickets for event " << query.event_number << ". Not enough seats available." << endl;
                    query_successful = true;
                }
                break;
            }
            case 3: { // Cancel
                if (availality[query.event_number] < AUDITORIUM_CAPACITY) {
                    cout << "Thread " << query.thread_number << " cancels a ticket for event " << query.event_number << endl;
                    availality[query.event_number]++;
                    query_successful = true;
                } else {
                    cout << "Thread " << query.thread_number << " failed to cancel tickets for event " << query.event_number << ". No tickets booked." << endl;
                    query_successful = true; 
                }
                break;
            }
        }

        // Get lock for shared table again to remove query from there
        pthread_mutex_lock(&mutex_shared_table);
        for (auto it = shared_table.begin(); it != shared_table.end(); ++it) {
            if (it->thread_number == query.thread_number) {
                shared_table.erase(it);
                break;
            }
        }
        pthread_cond_signal(&cond_active_queries);
        pthread_mutex_unlock(&mutex_shared_table);
        break;
    }
    // Message for unable to execute query
    if (!query_successful) {
        cerr << "Thread " << query.thread_number << " failed to process the query" << endl;
    }
}

void print_reservation_status() {
    cout << "Reservation status:" << endl;
    for (int i = 0; i < e; ++i) {
        cout << "Event " << i << ": "<< "Tickets Booked: " << (AUDITORIUM_CAPACITY - availality[i]) << ", Available Seats: " << availality[i] << endl;
    }
}
