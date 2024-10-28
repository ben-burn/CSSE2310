#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <csse2310a3.h>
#include <csse2310a4.h>

#define WRITE_END 0
#define READ_END 1

/* Tracks how many auctions this client is 
 * involved in
 */
typedef struct {
    int numAuctionItems;
    FILE** fdArray; // The array of file descriptors for reading and writing
                    // to the server
    sem_t* lock;
} AuctionCounter;

char* parse_args(int argc, char** argv);
FILE** attempt_connection(char* portNumber);
void connection_failed(char* portNumber);
AuctionCounter* create_auction_counter(FILE** fileArray);
void run_stdin_thread(AuctionCounter* ac);
void* run_server_connection_thread(void* ac);
int check_server_output(char* line);

/* sigpipe_handler()
 * --------------
 * Function handles a SIGPIPE.
 *
 * arg1: int sig, the signal detected.
 *
 * Returns: void.
 */
void sigpipe_handler(int sig) {
    if (sig == SIGPIPE) {
        fprintf(stderr, "auctionclient: server connection terminated\n");
        exit(13);
    }
}

int main(int argc, char** argv) {
    char* portNumber = parse_args(argc, argv);
    FILE** fdArray = attempt_connection(portNumber);
    signal(SIGPIPE, sigpipe_handler);
    AuctionCounter* ac = create_auction_counter(fdArray);
    pthread_t tid;
    pthread_create(&tid, NULL, run_server_connection_thread, ac);
    run_stdin_thread(ac);
    return 0;
}

/* parse_args()
 * −−−−−−−−−−−−−−−
 * Function designates the port number.
 *
 * arg1: int, argc. The number of commandline arguments.
 * arg2: char**, argv. The commandline arguments.
 *
 * Returns: char*, the port number indicating which port the localhost is 
 * listening on.
 * Errors: Will exit client if number of commandline arguments does not 
 * equal 1 or if the client cannot establish a connection to the server. 
 */
char* parse_args(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: auctionclient portnumber\n");
        exit(12);
    }
    char* port = argv[1];
    return port;
}

/* attempt_connection()
 * −−−−−−−−−−−−−−−
 * Function attempts to connect to the given port number.
 *
 * Returns: void.
 * Errors: Will exit client if unable to establish connection to server via
 * port number.
 */
FILE** attempt_connection(char* portNumber) {
    // Set up addrinf
    struct addrinfo* ai = 0;  
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;        // IPv4, for generic could use AF_UNSPEC 
    hints.ai_socktype = SOCK_STREAM;  // For TCP connection
    // Check addrinfo exists
    if ((getaddrinfo("localhost", portNumber, &hints, &ai))) {
        freeaddrinfo(ai);  
        connection_failed(portNumber);
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    // Attempt connection
    if (connect(fd, ai->ai_addr, sizeof(struct sockaddr))) {
        connection_failed(portNumber);
    }
    // Set up read and write ends
    int fd2 = dup(fd);
    FILE** fdArray = malloc(sizeof(FILE*) * 2);
    fdArray[WRITE_END] = fdopen(fd, "w");
    fdArray[READ_END] = fdopen(fd2, "r");
    return fdArray;
}

/* connection_failed()
 * −−−−−−−−−−−−−−−
 * Function handles a failed connection.
 *
 * arg1: char*, the port number that the client failed to connect to.
 *
 * Returns: void.
 * Errors: Exits 18.
 */
void connection_failed(char* portNumber) {
    fprintf(stderr, "auctionclient: connection failed to port %s\n", 
            portNumber);
    exit(18);
}

/* create_auction_counter()
 * −−−−−−−−−−−−−−−
 * Function creates an auction counter struct.
 *
 * arg1: FILE**, the array holding both the read and write end of the file 
 * descriptor for connecting to the server.
 *
 * Retruns: AuctionCounter*, a pointer to an AuctionCounter struct.
 */
AuctionCounter* create_auction_counter(FILE** fileArray) {
    AuctionCounter* ac = malloc(sizeof(AuctionCounter));
    ac->numAuctionItems = 0;
    ac->fdArray = fileArray;
    sem_t* l = malloc(sizeof(sem_t));
    ac->lock = l;
    sem_init(l, 0, 1);
    return ac;
}

/* run_stdin_thread()
 * −−−−−−−−−−−−−−−
 * Function runs a thread handling stdin. Will also write to the server.
 *
 * arg1: void* ac. Points to an AuctionCounter struct.
 *
 * Returns: void.
 * Errors: Exits 0. Will Exit 3 if detects EOF from stdin.
 */
void run_stdin_thread(AuctionCounter* ac) {
    char* line;
    while ((line = read_line(stdin)) != NULL) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        if (!strcmp(line, "quit")) {
            sem_wait(ac->lock);
            if (ac->numAuctionItems > 0) {
                printf("Auction(s) in progress - unable to exit yet\n");
                fflush(stdout);
                sem_post(ac->lock);
            } else {
                sem_post(ac->lock);
                exit(0);
            }
        } else {
            fprintf(ac->fdArray[WRITE_END], "%s\n", line);
            fflush(ac->fdArray[WRITE_END]);
        }
    }
    sem_wait(ac->lock);
    if (ac->numAuctionItems > 0) {
        fprintf(stderr, "Quitting with auction(s) still in progress\n");
        fflush(stderr);
        sem_post(ac->lock);
        exit(3);
    } 
    sem_post(ac->lock);
    exit(0);
}

/* run_server_connection_thread()
 * −−−−−−−−−−−−−−−
 * Function runs a thread handling input from the server.
 *
 * arg1: void* ac. Points to an AuctionCounter struct.
 *
 * Returns: void.
 * Errors: Exits 13 if EOF is detected from the server.
 */
void* run_server_connection_thread(void* vac) {
    char* line;
    AuctionCounter* ac = (AuctionCounter*)vac;
    while (1) {
        line = read_line(ac->fdArray[READ_END]);
        if (line == NULL) {
            break;
        }
        printf("%s\n", line);
        fflush(stdout);
        int aucNum = check_server_output(line);
        sem_wait(ac->lock);
        ac->numAuctionItems += aucNum;
        sem_post(ac->lock);
    }
    fprintf(stderr, "auctionclient: server connection terminated\n");
    fflush(stderr);
    exit(13);
    return NULL;
}

/* check_server_output()
 * −−−−−−−−−−−−−−−
 * Function checks if client recieved a valid bid or sell response.
 *
 * arg1: char*, the line that the server sent to the client.
 *
 * Returns: int, 1 if involved in another auction, -1 if an auction ended, 
 * 0 if neither.
 */
int check_server_output(char* line) {
    // :invalid --
    // :listed +1
    // :rejected --
    // :unsold -1
    // :list ...--
    // :bid +1
    // :outbid -1
    // :sold -1
    // :won ... - 1
    char c = line[1];
    char c2 = line[5];
    if (c == 'b' || (c == 'l' && c2 == 'e')) {
        return 1;
    } else if (c == 'u' || c == 'o' || c == 's' || c == 'w') {
        return -1;
    } else {
        return 0;
    }
}




