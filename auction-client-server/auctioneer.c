#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <csse2310a3.h>
#include <csse2310a4.h>
#include <limits.h>

#define MAX_ATOI_VALUE 2147483647
#define WRITE_END 0
#define READ_END 1
#define SELL 0
#define BID 1
#define LIST 2
#define MESSAGE_BUFFER 80
#define COMMAND 0
#define ITEM_NAME 1
#define RESERVE 2
#define DURATION 3
#define BID_PRICE 2
#define MILI 100000
#define SEC 1000000

/* Containing commandline 
 * args
 */
typedef struct {
    int maxCon;
    char* portNum;
} Args;

/* Contains information for a 
 * specific client
 */
typedef struct {
    int id;
    FILE** fdArray;
    bool connected;
} ClientInfo;

/* Contains the information for
 * a single auction
 */
typedef struct {
    sem_t* auctionLock;
    char* name;
    int reserve;
    int price;
    double expiry;
    FILE* sellerId;
    FILE* highestBidderId;
    FILE* buyerId;
    bool completed;
} AuctionInfo;

/* Contains the pointer to an array
 * of auctinos
 */
typedef struct {
    sem_t* auctionArrayLock;
    AuctionInfo** auctions;
    int numAuc;
} AuctionArray;

/* Contains the statistics */
typedef struct {
    sem_t* statLock;
    int numClient;
    int compClient;
    int activeAuc;
    int totalSellReq;
    int sucSellReq;
    int totalBidReq;
    int sucBidReq;  
} ServerStats;

/* Contains the pointer to a single client
 * as well as a pointer to the ServerStats and
 * AuctionArray structs
 * Will get passed to each client
 */
typedef struct {
    sem_t* serverLock;
    ClientInfo* client;
    AuctionArray* auctionArray;
    ServerStats* serverStats;
} ServerInfo;

Args* parse_args(int argc, char** argv);
void parse_args_helper(char** argv, Args* args); 
void invalid_args(void);
int get_num(char* arg, int indicator);
int validate_num(char* arg, int argInd);
int open_port(Args* args);
void port_listen_error(void);
void process_connections(int server, Args* args);
ServerInfo* create_server_info_struct(Args* args); 
AuctionArray* create_auction_array_struct(void); 
ServerStats* create_stats_struct(void);
ClientInfo* create_client(int fd, int id); 
AuctionInfo* create_auction(char* itemName, int itemPrice, double time, 
        FILE* sellId);
void* client_handler(void* clientServerInfo);
void handle_client_disconnect(ServerInfo* serverInfo); 
void client_input_handler(char** splitLine, ServerInfo* serverInfo);
int test_input(char** splitLine, int indicator);
void handle_client_sell(char** splitLine, ServerInfo* serverInfo); 
int check_unique_name(char* name, ServerInfo* serverInfo); 
void handle_client_bid(char** splitLine, ServerInfo* serverInfo);
void valid_bid_helper(char* itemName, ServerInfo* serverInfo, int aucInd, 
        int bidPrice, FILE* fd); 
void handle_client_list(ServerInfo* serverInfo);
void* time_keeper(void* serverInfo); 
void* stat_handler(void* serverStats);
void send_message(FILE* fd, char* message);
void sigpipe_handler(int sig); 

int main(int argc, char** argv) {
    Args* args = parse_args(argc, argv);
    signal(SIGPIPE, sigpipe_handler); 
    int port = open_port(args);
    process_connections(port, args);
    return 0;
}

/* parse_args()
* −−−−−−−−−−−−−−−
* Function sets up the Args struct given commandline arguments.
*
* arg1: int argc, the number of commandline arguments.
* arg2: char** argv, an array containing the commandline arguments.
*
* Returns: Args*, a pointer to a newly formed Args struct.
* Errors: Exits 14 if program recieves invalid arguments.
*/
Args* parse_args(int argc, char** argv) {
    Args* args = malloc(sizeof(Args));
    char* eph = malloc(sizeof(char) * 2);
    eph = "0\0";
    int maxInd = 0;
    int portInd = 0;
    if (argc > 5) {
        invalid_args();
    } else if (argc == 5) {
        parse_args_helper(argv, args); 
    } else if (argc == 3) {
        if (!strcmp(argv[1], "--max")) {
            args->maxCon = validate_num(argv[2], 0); 
            args->portNum = eph;
            maxInd++;
        } else if (!strcmp(argv[1], "--port")) {
            validate_num(argv[2], 1); 
            args->portNum = argv[2]; 
            args->maxCon = INT_MAX;
            portInd++;
        } else {
            invalid_args();
        }
    } else if (argc == 1) {
        args->portNum = eph;
        args->maxCon = INT_MAX;
    } else {
        invalid_args();
    }
    if (maxInd > 1 || portInd > 1) {
        invalid_args();
    }
    return args;
}

/* parse_args_helper()
 * --------------
 * Function helps the parse args function where argc = 5.
 *
 * arg2: char** argv, the command line arguments.
 * arg2: Args* args, the args stuct to be filled.
 *
 * Return: void:
 */
void parse_args_helper(char** argv, Args* args) {
    int maxInd = 0;
    int portInd = 0;
    if (!strcmp(argv[1], "--max")) {
        args->maxCon = validate_num(argv[2], 0); 
        maxInd++;
    } else if (!strcmp(argv[1], "--port")) {
        validate_num(argv[2], 1); 
        args->portNum = argv[2]; 
        portInd++;
    } else {
        invalid_args();
    }
    if (!strcmp(argv[3], "--port")) {
        validate_num(argv[4], 1); 
        args->portNum = argv[4];
        portInd++;
    } else if (!strcmp(argv[3], "--max")) {
        args->maxCon = validate_num(argv[4], 0); 
        maxInd++;
    } else {
        invalid_args();
    }
    if (maxInd > 1 || portInd > 1) {
        invalid_args();
    }
}

/* invalid_args()
* −−−−−−−−−−−−−−−
* Function exits program if given invalid args.
*
* Returns: void.
* Errors: Exits 14 if program recieves invalid arguments.
*/
void invalid_args(void) {
    fprintf(stderr, "Usage: auctioneer [--max num-connections] "
            "[--port portnum]\n");
    exit(14);
}

/* get_num()
* −−−−−−−−−−−−−−−
* Function converts a string into a number.
*
* arg1: char* arg, the given string from the command line
* arg2: int indicator, indicates if get_num is being called for command line
* or client input testing.
* 0 -> commandline
* 1 -> client
*
* Returns: int num, the number to be returned.
* Errors: Exits 14 if arg is not a number.
*/
int get_num(char* arg, int indicator) {
    char* numTest;
    long int testInt = strtol(arg, &numTest, 10);
    if (testInt > MAX_ATOI_VALUE || strcmp(numTest, "")) {
        if (!indicator) {
            invalid_args();
        } else {
            return -1;
        }
    }
    int num = atoi(arg);
    if (!indicator) {
        if (num < 0) {
            invalid_args();
        }
    } else {
        if (num <= 0) {
            return -1;
        }
    }
    return num;
}

/* validate_num()
 * −−−−−−−−−−−−−−−
 * Function validates an integer.
 *
 * arg1: char* arg, the string to be converted into an int
 * arg2: int argInd, indicates if testing max connections or port number
 * 0 -> max connections
 * 1 -> port number
 * 
 * Returns: int num, the integer converted from a string.
 * Errors: Exits 14 is number is invalid.
 */
int validate_num(char* arg, int argInd) {
    int num = get_num(arg, 0);
    if (argInd) {
        if (num < 1024 || num > 65535) {
            invalid_args();
        }
    }
    return num;
}

/* open_port()
 * −−−−−−−−−−−−−−−
 * Function opens a port for listening.
 *
 * arg1: Args* args is a pointer to the Args struct containing
 * the port number.
 *
 * Returns: int, the file directory to the opened port.
 * Errors: Exits with status 13 if any error occurs.
 */
int open_port(Args* args) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // listen on all IP addresses
    if (getaddrinfo(NULL, args->portNum, &hints, &ai)) {
        freeaddrinfo(ai);
        port_listen_error();
    }
    // Create a socket and bind it to a port
    int server = socket(AF_INET, SOCK_STREAM, 0); //0 = TCP
    // Allow address (port number) to be reused immediately
    int optVal = 1;
    if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, 
            &optVal, sizeof(int)) < 0) {
        port_listen_error();
    }
    if (bind(server, ai->ai_addr, sizeof(struct sockaddr))) {
        port_listen_error();
    }
    // Up to 10 connection requests can queue
    if (listen(server, 10) < 0) {
        port_listen_error();
    }
    // Prints the opened port
    struct sockaddr_in ad; 
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in); 
    if (getsockname(server, (struct sockaddr*)&ad, &len)) {
        port_listen_error();
    }
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));
    fflush(stderr);
    return server;
}

/* port_listen_error()
 * −−−−−−−−−−−−−−−
 * Function exits the program if unable to listen on port.
 *
 * Returns: void.
 * Errors: Exits with status of 13.
 */
void port_listen_error(void) {
    fprintf(stderr, "auctioneer: socket can\'t be listened on\n");
    exit(13);
}

/* process_connections()
 * −−−−−−−−−−−−−−−
 * Function handles the incoming server connections.
 *
 * arg1: int server, the file descripter for the listening port.
 * arg2: Args* args, the struct containing the command line arguments.
 *
 * Returns: void.
 */
void process_connections(int server, Args* args) {
    int clientFd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    int idCounter = 0;
    ServerInfo* serverInfo = create_server_info_struct(args);
    serverInfo->client = malloc(sizeof(ClientInfo));
    sigset_t signalSet;
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &signalSet, NULL);
    pthread_t tid;
    pthread_create(&tid, NULL, stat_handler, serverInfo->serverStats);
    pthread_detach(tid);
    pthread_create(&tid, NULL, time_keeper, serverInfo);
    pthread_detach(tid);
    while (1) {
        fromAddrSize = sizeof(struct sockaddr_in);
        sem_wait(serverInfo->serverLock);
        clientFd = accept(server, (struct sockaddr*)&fromAddr, &fromAddrSize);
        if (clientFd < 0) {
            sem_post(serverInfo->serverLock);
            continue;
        }
        sem_wait(serverInfo->serverStats->statLock);
        serverInfo->serverStats->numClient++;
        sem_post(serverInfo->serverStats->statLock);
        // For each new client, create an identical copy of serverInfo except
        // for a newly created client pointer
        ServerInfo* clientServerInfo = malloc(sizeof(ServerInfo));
        memcpy(clientServerInfo, serverInfo, sizeof(ServerInfo));
        clientServerInfo->client = create_client(clientFd, idCounter);
        idCounter++;
        pthread_create(&tid, NULL, client_handler, clientServerInfo); 
        pthread_detach(tid);
    }
}

/* create_server_info_struct()
 * −−−−−−−−−−−−−−−
 * Function creates the ServerInfo struct.
 *
 * arg1: Args* args, the command line arguments.
 *
 * Returns: ServerInfo* serverInfo, the struct conatining the serverinfo.
 */
ServerInfo* create_server_info_struct(Args* args) {
    ServerInfo* serverInfo = malloc(sizeof(ServerStats));
    sem_t* l = malloc(sizeof(sem_t));
    serverInfo->serverLock = l;
    sem_init(l, 0, args->maxCon);
    serverInfo->client = NULL;
    serverInfo->auctionArray = create_auction_array_struct();
    serverInfo->serverStats = create_stats_struct(); 
    return serverInfo;
}

/* create_auction_array_struct()
 * −−−−−−−−−−−−−−−
 * Function creates an AuctionArray struct.
 *
 * Returns: AuctionArray* aArr, the pointer to an AuctionArray struct.
 */
AuctionArray* create_auction_array_struct(void) {
    AuctionArray* aArr = malloc(sizeof(AuctionArray));
    sem_t* l = malloc(sizeof(sem_t));
    aArr->auctionArrayLock = l;
    sem_init(l, 0, 1);
    aArr->auctions = malloc(sizeof(AuctionInfo*));
    aArr->numAuc = 0;
    return aArr;
}

/* create_stats_struct()
 * −−−−−−−−−−−−−−−
 * Function creates a ServerStats struct.
 * 
 * Returns: ServerStats* serverStats, the struct containing the server
 * stats
 */
ServerStats* create_stats_struct(void) {
    ServerStats* serverStats = malloc(sizeof(ServerStats));
    sem_t* l = malloc(sizeof(sem_t));
    serverStats->statLock = l;
    sem_init(l, 0, 1);
    serverStats->numClient = 0;
    serverStats->compClient = 0;
    serverStats->activeAuc = 0;
    serverStats->totalSellReq = 0;
    serverStats->sucSellReq = 0;
    serverStats->totalBidReq = 0;
    serverStats->sucBidReq = 0;
    return serverStats;
}

/* create_client()
 * −−−−−−−−−−−−−−−
 * Function creates a ClientInfo struct.
 * 
 * arg1: int fd, the file descripter to the client.
 * arg2: int id, the unique ID of the client struct.
 * arg3: ClientInfo* toFill, the address of the struct to be filled.
 *
 * Returns: void.
 */
ClientInfo* create_client(int fd, int id) {
    ClientInfo* toFill = malloc(sizeof(ClientInfo));
    toFill->id = id;
    int fd2 = dup(fd);
    FILE** newFdArray = malloc(sizeof(FILE*) * 2);
    newFdArray[WRITE_END] = fdopen(fd, "w");
    newFdArray[READ_END] = fdopen(fd2, "r");
    toFill->fdArray = newFdArray;
    toFill->connected = true;
    return toFill;
}

/* create_auction()
 * −−−−−−−−−−−−−−−
 * Function creates an AuctionInfo struct.
 *
 * arg1: char* itemName, the name of the item for sale.
 * arg2: int itemPrice, the price of the item for sale.
 * arg3: double time, the time limit of the item for sale.
 * arg4: int sellId, the ID of the seller.
 *
 * Retruns: AuctionInfo* auctionInfo, the struct containing information about
 * a specific auction.
 */
AuctionInfo* create_auction(char* itemName, int itemPrice, double time,
        FILE* sellId) {
    AuctionInfo* auctionInfo = malloc(sizeof(AuctionInfo));
    sem_t* l = malloc(sizeof(sem_t));
    auctionInfo->auctionLock = l;
    sem_init(l, 0, 1);
    auctionInfo->name = strdup(itemName);
    auctionInfo->reserve = itemPrice;
    auctionInfo->price = 0;
    auctionInfo->expiry = get_time_ms() + time;
    auctionInfo->sellerId = sellId;
    auctionInfo->highestBidderId = NULL;
    auctionInfo->buyerId = NULL;
    auctionInfo->completed = false;
    return auctionInfo;
}

/* client_handler()
 * −−−−−−−−−−−−−−−
 * Function handles a client.
 *
 * arg1: void* clientServerInfo, the struct holding the information for one 
 * client.
 *
 * Returns: void*.
 */
void* client_handler(void* clientServerInfo) {
    ServerInfo* serverInfo = (ServerInfo*)clientServerInfo;
    char* line;
    while (1) {
        line = read_line(serverInfo->client->fdArray[READ_END]);
        if (line == NULL) {
            break;
        }
        client_input_handler(split_string(line, ' '), serverInfo);
    }
    sem_wait(serverInfo->serverStats->statLock);
    serverInfo->serverStats->numClient--;
    serverInfo->serverStats->compClient++;
    sem_post(serverInfo->serverStats->statLock);
    sem_post(serverInfo->serverLock);
    return NULL;
}

/* handle_client_disconnect()
 * −−−−−−−−−−−−−−−
 * Function handles the disconnect of a client. Will set the sellerId and 
 * highestBidderId that matches the disconnecting client to NULL.
 *
 * arg1: ServerInfo* serverInfo, the struct conaining all the data 
 * relating to the client and auctions.
 *
 * Returns: void.
 */
void handle_client_disconnect(ServerInfo* serverInfo) {
    AuctionArray* aucArr = serverInfo->auctionArray;
    FILE* fd = serverInfo->client->fdArray[WRITE_END];
    sem_wait(aucArr->auctionArrayLock);
    for (int i = 0; i < aucArr->numAuc; i++) {
        AuctionInfo* curAuc = aucArr->auctions[i];
        if (curAuc->sellerId == fd) {
            curAuc->sellerId = NULL;
        }
        if (curAuc->highestBidderId == fd) {
            curAuc->highestBidderId = NULL;
        }
    }
    sem_post(aucArr->auctionArrayLock);
}

/* client_input_handler()
 * −−−−−−−−−−−−−−−
 * Function handles the input from the client.
 *
 * arg1: char* line, the input from the client process.
 * arg2: ServerInfo* serverInfo, the struct containing the server information.
 *
 * Returns: void.
 */
void client_input_handler(char** splitLine, ServerInfo* serverInfo) {
    FILE* fd = serverInfo->client->fdArray[WRITE_END];
    if (!strcmp(splitLine[COMMAND], "sell")) {
        handle_client_sell(splitLine, serverInfo);
    } else if (!strcmp(splitLine[COMMAND], "bid")) {
        handle_client_bid(splitLine, serverInfo);
    } else if (!strcmp(splitLine[COMMAND], "list")) {
        if (test_input(splitLine, LIST)) {
            handle_client_list(serverInfo);
        } else {
            send_message(fd, ":invalid\n");
        }
    } else {
        send_message(fd, ":invalid\n");
    }
}           
    
/* test_input()
 * _______________
 * Functions tests a command.
 *
 * arg1: char** splitLine, the array to be tested.
 * arg2: int indicator, indicates if testing for sell (0), bid (1), 
 * list command (2).
 *
 * Returns: int, 1 if valid else 0.
 */
int test_input(char** splitLine, int indicator) {
    int length = 0;
    if (indicator == SELL) {
        while (splitLine[length]) {
            length++;
        }
        if (length == 4) {
            if (get_num(splitLine[RESERVE], 1) < 0 || 
                    get_num(splitLine[DURATION], 1) < 0 ||
                    atoi(splitLine[DURATION]) < 1) {
                return 0;
            }
            return 1;
        }
        return 0;
    } else if (indicator == BID) {
        while (splitLine[length]) {
            length++;
        }
        if (length == 3) {
            if (get_num(splitLine[BID_PRICE], 1) < 0) {
                return 0;
            }
            return 1;
        }
        return 0;
    } else if (indicator == LIST) {
        while (splitLine[length]) {
            length++;
        }
        if (length == 1) {
            return 1;
        } 
        return 0;
    }
    return 0;
}

/* handle_client_sell()
 * _______________
 * Functions handles selling of item.
 *
 * arg1: char** splitLine, the input from the client process.
 * arg2: ServerInfo* serverInfo, the struct containing the server information.
 *
 * Returns: void.
 */
void handle_client_sell(char** splitLine, ServerInfo* serverInfo) {
    sem_wait(serverInfo->serverStats->statLock);
    serverInfo->serverStats->totalSellReq++;
    sem_post(serverInfo->serverStats->statLock);
    FILE* fd = serverInfo->client->fdArray[WRITE_END];

    if (test_input(splitLine, SELL)) {
        if (serverInfo->auctionArray->numAuc == 0 || 
                check_unique_name(splitLine[ITEM_NAME], serverInfo)) {
            AuctionArray* aucArr = serverInfo->auctionArray;
            sem_wait(aucArr->auctionArrayLock);
            aucArr->auctions = realloc(aucArr->auctions, 
                    sizeof(aucArr->numAuc + 1));
            aucArr->auctions[aucArr->numAuc] = create_auction(
                    splitLine[ITEM_NAME], atoi(splitLine[RESERVE]), 
                    (double)atoi(splitLine[DURATION]), fd);            
            aucArr->numAuc++;
            // Update stats
            sem_wait(serverInfo->serverStats->statLock);
            serverInfo->serverStats->activeAuc++;
            serverInfo->serverStats->sucSellReq++;
            sem_post(serverInfo->serverStats->statLock);
            // Send message
            char newMessage[MESSAGE_BUFFER];
            sprintf(newMessage, ":listed %s\n", splitLine[ITEM_NAME]);
            send_message(fd, newMessage);
            sem_post(aucArr->auctionArrayLock);
        }
    } else {
        send_message(fd, ":invalid\n");
    }
}

/* check_unique_name()
 * _______________
 * Functions checks if a name is already on auction.
 *
 * arg1: char* name, the name to be checked.
 * arg2: ServerInfo* serverInfo, the struct containing a list of auctions.
 *
 * Returns: int, 1 if given name is unique, else 0.
 */
int check_unique_name(char* name, ServerInfo* serverInfo) {
    AuctionArray* aucArr = serverInfo->auctionArray;
    sem_wait(aucArr->auctionArrayLock);
    for (int i = 0; i < aucArr->numAuc; i++) {
        AuctionInfo* curAuc = aucArr->auctions[i];
        if (!(curAuc->completed)) {
            if (!strcmp(name, curAuc->name)) {
                send_message(serverInfo->client->fdArray[WRITE_END], 
                        ":rejected\n");
                sem_post(aucArr->auctionArrayLock);
                return 0;
            }
        }
    }
    sem_post(aucArr->auctionArrayLock);
    return 1;
}

/* handle_client_bid()
 * −−−−−−−−−−−−−−−
 * Function handles bid from a client.
 *
 * arg1: char** splitLine, the input from the client.
 * arg2: ServerInfo* serverInfo, the struct containing the server information.
 *
 * Returns: void.
 */
void handle_client_bid(char** splitLine, ServerInfo* serverInfo) {
    sem_wait(serverInfo->serverStats->statLock);
    serverInfo->serverStats->totalBidReq++;
    sem_post(serverInfo->serverStats->statLock);
    FILE* fd = serverInfo->client->fdArray[WRITE_END];
    if (test_input(splitLine, BID)) {
        AuctionArray* aucArr = serverInfo->auctionArray;
        sem_wait(aucArr->auctionArrayLock);
        int aucInd = -1;
        int bidPrice = atoi(splitLine[BID_PRICE]);
        for (int i = 0; i < aucArr->numAuc; i++) {
            AuctionInfo* curAuc = aucArr->auctions[i];
            if (!(curAuc->completed)) {
                if (!strcmp(splitLine[ITEM_NAME], curAuc->name)) {
                    if (!curAuc->price) {
                        if ((bidPrice >= aucArr->auctions[i]->reserve) && 
                                (fd != curAuc->sellerId) &&
                                (fd != curAuc->highestBidderId)) {
                            aucInd = i;
                            break;
                        }
                    } else if ((bidPrice > aucArr->auctions[i]->price) && 
                            (fd != curAuc->sellerId) &&
                            (fd != curAuc->highestBidderId)) {
                        aucInd = i;
                        break;
                    }
                }
            }
        }
        if (aucInd != -1) {
            valid_bid_helper(splitLine[ITEM_NAME], serverInfo, aucInd, 
                    bidPrice, fd);        
        } else {
            send_message(fd, ":rejected\n");
        }
        sem_post(aucArr->auctionArrayLock);
    } else {
        send_message(fd, ":invalid\n");
    }
}

/* valid_bid_helper()
 * −−−−−−−−−−−−−−−
 * Function handles a valid bid.
 *
 * arg1: char* itemName, the item being bid on.
 * arg2: ServerInfo* serverInfo, the struct containing the server information.
 * arg3: int aucInd, the index of the auction being bid on.
 * arg4: int bidPrice, the new bid price.
 * arg5: FILE* fd, the fild descriptor for the client.
 *
 * Returns: void.
 */
void valid_bid_helper(char* itemName, ServerInfo* serverInfo, int aucInd, 
        int bidPrice, FILE* fd) {
    AuctionArray* aucArr = serverInfo->auctionArray;
    aucArr->auctions[aucInd]->price = bidPrice;
    FILE* highBidId = aucArr->auctions[aucInd]->highestBidderId;
    // If highest bidder does not exist set client to new highest bidder else
    // send message to previous highest bidder
    if (!highBidId) {
        aucArr->auctions[aucInd]->highestBidderId = fd;
    } else {
        aucArr->auctions[aucInd]->highestBidderId = fd;
        char outBidMessage[MESSAGE_BUFFER];
        sprintf(outBidMessage, ":outbid %s %d\n", itemName, bidPrice);
        send_message(highBidId, outBidMessage);
    }
    sem_wait(serverInfo->serverStats->statLock);
    serverInfo->serverStats->sucBidReq++;
    sem_post(serverInfo->serverStats->statLock);
    char newMessage[MESSAGE_BUFFER];
    sprintf(newMessage, ":bid %s\n", itemName);
    send_message(fd, newMessage);
}

/* handle_client_list()
 * −−−−−−−−−−−−−−−
 * Function handles list command from a client.
 *
 * arg1: char** splitLine, the input from the client.
 * arg2: ServerInfo* serverInfo, the struct containing the server information.
 *
 * Returns: void.
 */
void handle_client_list(ServerInfo* serverInfo) {
    AuctionArray* aucArr = serverInfo->auctionArray;
    FILE* fd = serverInfo->client->fdArray[WRITE_END];
    send_message(fd, ":list");
    sem_wait(aucArr->auctionArrayLock);
    if (aucArr->numAuc > 0) {
        send_message(fd, " ");
        for (int i = 0; i < aucArr->numAuc; i++) {
            AuctionInfo* curAuc = aucArr->auctions[i];     
            if (!(curAuc->completed)) {
                char newItem[MESSAGE_BUFFER];
                sprintf(newItem, "%s %d %d %d|", curAuc->name, 
                        curAuc->reserve, curAuc->price, 
                        (int)(curAuc->expiry - get_time_ms()));
                send_message(fd, newItem);
            }
        }
    }
    sem_post(aucArr->auctionArrayLock);
    send_message(fd, "\n");
}

/* time_keeper()
 * −−−−−−−−−−−−−−−
 * Function handles the time keeping thread.
 *
 * args1: void* serverInfo, the struct containing the server information.
 *
 * Returns: void*.
 */
void* time_keeper(void* serverInfo) {
    ServerInfo* sI = (ServerInfo*)serverInfo;
    AuctionArray* aucArr = sI->auctionArray;
    while (1) {
        usleep(MILI);
        double currentTime = get_time_ms();
        sem_wait(aucArr->auctionArrayLock);
        for (int i = 0; i < aucArr->numAuc; i++) {
            AuctionInfo* curAuc = aucArr->auctions[i];
            if (currentTime >= curAuc->expiry && !(curAuc->completed)) {
                if (curAuc->highestBidderId) {
                    char soldMessage[MESSAGE_BUFFER];
                    sprintf(soldMessage, ":sold %s %d\n", curAuc->name, 
                            curAuc->price);
                    send_message(curAuc->sellerId, soldMessage);
                    char wonMessage[MESSAGE_BUFFER];
                    sprintf(wonMessage, ":won %s %d\n", curAuc->name, 
                            curAuc->price);
                    send_message(curAuc->highestBidderId, wonMessage);
                    curAuc->buyerId = curAuc->highestBidderId;
                } else {
                    char unSoldMessage[MESSAGE_BUFFER];
                    sprintf(unSoldMessage, ":unsold %s\n", curAuc->name);
                    send_message(curAuc->sellerId, unSoldMessage);
                }
                curAuc->completed = true;
                curAuc->name = NULL;
                sem_wait(sI->serverStats->statLock);
                sI->serverStats->activeAuc--;
                sem_post(sI->serverStats->statLock);
            }
        }
        sem_post(aucArr->auctionArrayLock);  
    }
    return NULL;
}

/* stat_handler()
 * −−−−−−−−−−−−−−−
 * Function handles the stat thread.
 *
 * arg1: void* serverStats, the struct holding the server stats.
 *
 * Returns: void*.
 */
void* stat_handler(void* serverStats) {
    ServerStats* stats = (ServerStats*)serverStats;
    sigset_t signalSet;
    sigemptyset(&signalSet);
    sigaddset(&signalSet, SIGHUP);
    int receivedSignal;
    while (1) {
        sigwait(&signalSet, &receivedSignal);
        sem_wait(stats->statLock);
        fprintf(stderr, "Connected clients: %d\n", stats->numClient);
        fprintf(stderr, "Completed clients: %d\n", stats->compClient);
        fprintf(stderr, "Active auctions: %d\n", stats->activeAuc);
        fprintf(stderr, "Total sell requests: %d\n", stats->totalSellReq);
        fprintf(stderr, "Successful sell requests: %d\n", stats->sucSellReq);
        fprintf(stderr, "Total bid requests: %d\n", stats->totalBidReq);
        fprintf(stderr, "Successful bid requests: %d\n", stats->sucBidReq);
        sem_post(stats->statLock);
        fflush(stderr);
    }
    return NULL;
}

/* send_message()
 * −−−−−−−−−−−−−−−
 * Function sends message to a client.
 *
 * arg1: FILE* fd, the file descripter to write to.
 * arg2: char* message, the message to be sent.
 *
 * Returns: void.
 */
void send_message(FILE* fd, char* message) {
    if (fd) {
        fprintf(fd, "%s", message);
        fflush(fd);
    }
}

/* sigpipe_handler()
 * ------------------
 * Function handles SIGPIPE detection.
 *
 * arg1: int sig, the detected signal.
 *
 * Returns: void.
 */
void sigpipe_handler(int sig) {
    if (sig == SIGPIPE) {
        //
    }
}


                
