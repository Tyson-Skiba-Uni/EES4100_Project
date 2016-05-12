// Project Part 1: Modbus Client

// Required to Read 3 Registers each Second from the Modbus Server Running on the Test Bench
// If i recall Kim gave me 4 instances so i'll go with that instead
// Input Register Reply
// Output TCP connect, Read Register

#include <stdio.h>
#include <stdlib.h>
#include <modbus-tcp.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>								// Included Header Files, These Cotain Functions that will be used within the Program

#define MODBUS_SERVER_ADR	"127.0.0.1" 					// modbus server location
#define MODBUS_SERVER_PORT	502            	
#define DATA_LENGTH 		256

struct list_object_s {								// Structure with 3 members
    char *string;                   		
    int strlen;                     		
    struct list_object_s *next;     						// Dynamic Pointer to Next List Item
};

static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t list_data_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t list_data_flush = PTHREAD_COND_INITIALIZER;
static struct list_object_s *list_head;						// Pointer to the Linked List Head

static void add_to_list(char *input) {						// Input is a Charecter Pointer
    struct list_object_s *last_item;						// Point to the last item
    struct list_object_s *new_item = malloc(sizeof(struct list_object_s));	// Alloc Memory for the Size of the Structure
    if (!new_item) {								// If there are no new items
        fprintf(stderr, "Memory Alocation has Failed, Aborting\n");		// Print Memory Aloocation Error to Standard Error
        exit(1);								// Exit the Application
    }
										// -> is member of a structure
    new_item->string = strdup(input);						// place value of input into new_item.string
    new_item->strlen = strlen(input);						// place value of input length into new_item.strlen
    new_item->next = NULL;

    pthread_mutex_lock(&list_lock);						// Take the lock to protect from corruption as data is about to be modified

    if (list_head == NULL) {							// If no list head exists (first run)...
        list_head = new_item;							// ...Create an Object
    } else {									// For Every Other Possible Condition ie Data Exists...
        last_item = list_head;							// Move the pointer to the current item (list head)
        while (last_item->next) last_item = last_item->next;
        last_item->next = new_item;						// Make the new item the bottom of the list
    }

    pthread_cond_signal(&list_data_ready);					// Tell the function that Data can now be free'd
    pthread_mutex_unlock(&list_lock);						// Release Thread Lock
}

static struct list_object_s *list_get_first(void) {				
    struct list_object_s *first_item;						// Define struture with one element						

    first_item = list_head;							// Make the top item in the list the list head pointer
    list_head = list_head->next;						// Make the first item the next value

    return first_item;								// Return the first_item value
}

static void *print_and_free(void *arg) {
    struct list_object_s *cur_object;						// Define Structure with one member

    printf("thread is starting\n");						// Print Thread Starting to Standard Out

    while (1) {									// Ever loop, wait for data to become avaliable
        pthread_mutex_lock(&list_lock);

        while (!list_head)							// While there is no data avaliable
            pthread_cond_wait(&list_data_ready, &list_lock);			// Wait

        cur_object = list_get_first();						// Current Object equals the returned value of the lis_get_first function
        pthread_mutex_unlock(&list_lock);					// Release Thread Lock

        printf("t2: String is: %s\n", cur_object->string);			// Print String Value
        printf("t2: String length is %i\n", cur_object->strlen);		// Print Size of the String
        free(cur_object->string);						// Free Memory
        free(cur_object);

        pthread_cond_signal(&list_data_flush);					// Tell Thread that Work Has Been Done
    }
}

static void list_flush(void) {					
    pthread_mutex_lock(&list_lock);						// Thread Lock

    while (list_head) {								// While there is still an item
        pthread_cond_signal(&list_data_flush);					// Run the removal function on it
        pthread_cond_wait(&list_data_flush, &list_lock);			// Wait for Work Done Signal
    }

    pthread_mutex_unlock(&list_lock);						// Release Lock
}


static void client(int counter_given, int counter) {				// Modbus Client , requires a bool counter_given and counter integer
    int sock;									// Initialise an integer named sock
    struct sockaddr_in addr;							// Create a structure
    char input[256]; 								// Input Array with 256 slots

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {				// If less than zero
        printf("Could Not Open Socket: Are you Root?\n");			// Socket Could not Be Opened
        exit(EXIT_FAILURE);							// Exit with Value of EXIT_FALUIRE
    }

    memset(&addr, 0, sizeof(addr));						// Fill all 256 in the addr array with 0
    addr.sin_family = AF_INET;							// Use IPv4 Addresses
    addr.sin_port = htons(SERVER_PORT);						// Port Value is converted from Host Byte order to Network Byte order
    addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);				// Convert Server IP into Network Byte order

    if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {		// If less than 0
        printf("Unable to Connect ot the Server\n");				// Print Error Message
        exit(EXIT_FAILURE);							// Exit with returned EXIT_FALUIRE
    }

    while (scanf("%256s", input) != EOF) {					// While the input value is not the nd of file charecter
        if (send(sock, input, strlen(input) + 1, 0) < 0) {			// If value is less then 0
            printf("Unable to Send Data\n");					// Print Error Message
            exit(EXIT_FAILURE);
        }

        if (counter_given) {							// If there is a counter
            counter--;								// Decrement the Value
            if (!counter) break;						// If the counter has reached zero , break the input loop
        }
    }
}

int main(int argc, char **argv) {
    modbus_t *ctx;								// Modbus Pointer
    int option_index, c, counter, counter_given = 0, run_server = 0;		// Initialise Values

    struct option long_options[] = {						// Input Arguments
        { "count",      required_argument,  0, 'c' },				// --count requires an argument
        { "directive",  no_argument,        0, 'd' },				// --directive does not
        { "server",     no_argument,        0, 's' },				// --server does not
        { 0 }
    };

    while (1) {									// Ever Loop, Run Inifintly
        c = getopt_long(argc, argv, "c:ds", long_options, &option_index);	// C is equal to the input options

        if (c == -1) break;							// If c is equal to -1 break loop

        switch (c) {								// C is the case argument
            case 'c':								// for case c is c
                printf("Got count argument with value %s\n", optarg);		// Print Count Value
                counter = atoi(optarg);						// Convert tring to integer and place in counter
                counter_given = 1;						// Set the counter flag
                break;
            case 'd':								// For case c is d
                printf("Got directive argument\n");				// Print Phrase
                break;
            case 's':								// For case c is s
                run_server = 1;							// Flag Server Value
                break;
        }
    }

    else client(counter_given, counter);

    return 0;
}







