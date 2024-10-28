#include <stdlib.h>
#include <unistd.h> // access() to check if file path exists
#include <stdio.h>
#include <sys/wait.h>
#include <sys/stat.h> // these 2 for mkdir()
#include <sys/types.h> //
#include <string.h>
#include <ctype.h> // ???
#include <stdbool.h>
#include <csse2310a3.h> // for read_line(), split_string(), compare_timespecs
#include <errno.h> // for errno
#include <fcntl.h> // for open()
#include <signal.h> // for sigaction
#include <time.h> // for sigtimedwait()

#define READ_END 0
#define WRITE_END 1
#define BUFFER_LENGTH 50
#define TEMP_FILE "./tmp"
#define WORDLADDER_CHILD 0
#define CMP_CHILD_1 1
#define CMP_CHILD_2 2

// TO DO LIST
// remove magic strings or numbers
// make sure that command line arg parsing is 100% robust
// add more valgrind stuff


// Global variable for handling SIGINT detection
bool signalKill = false;

/* A struct to hold a given test from a jobfile */
typedef struct {
    char* testId;
    char* inputFileName;
    // commands to be given to a program
    char** optionalArgs;
    int optionalArgsSize;
} Test;

/* Holds the command line arguments */
typedef struct {
    char* testdir;
    bool recreate;
    char* jobfile;
    char* program;
} Args;

void parse_args(int, char**, Args*);
int test_command_line_arg(char*, Args*);
void test_jobfile_program_arg(Args*);
void program_error(int, int, char*, char*);
void job_line_syntax_error(Args*, char*, char**, FILE*, Test*, int, int);
Test* jobfile_specification(Args*, FILE*);
void create_new_test(char**, Test*);
bool check_test_struct_valid(Test, int, char*);
bool check_test_id(Test*, int);
void check_input_file(char*, char*, int);
void test_file_directory(char*);
void generate_output(Test*, char*, bool, char*, char*);
void regenerate_output_files(Test, char*, char*, char*, char*);
void check_file_for_writing(int, char*);
bool validate_file(char*, char*);
void handle_signal(int);
int run_job_tests(Test*, char*, char*, char*);
int passed_tests(int, int);
pid_t* run_test(Test, char*, char*, char*);
pid_t* create_children(int*, int*, char*, char**, int, char*, char*, char*);
void kill_children(pid_t*);
int handle_child_status(pid_t*, char*, char*);
int handle_child(pid_t, int, bool);
void close_pipes(int*, int*);
void free_char_array(char**, int);
void free_tests(Test*, int);

int main(int argc, char** argv) {
    Args* args = malloc(sizeof(Args));
    parse_args(argc, argv, args);
    FILE* jf = fopen(args->jobfile, "r");
    if (jf == NULL) {
        fprintf(stderr, "testuqwordladder: Unable to open job file \"%s\"\n",
                args->jobfile);
        free(args);
        exit(13);
    }
    Test* testArray = jobfile_specification(args, jf);
    test_file_directory(args->testdir);
    generate_output(testArray, args->testdir, args->recreate, args->jobfile,  
            "good-uqwordladder");
    int runTestResult = run_job_tests(testArray, args->testdir, args->jobfile, 
            args->program);
    // Free args->testdir if it is "./tmp" and is not in argv
    if (argc == 6) {
        if ((!strcmp(args->testdir, "./tmp")) && ((strcmp(argv[2], "./tmp")) 
                && (strcmp(argv[4], "./tmp")))) {
            free(args->testdir);
        }
    } else if (argc == 5) {
        if ((!strcmp(args->testdir, "./tmp")) && (strcmp(argv[2], "./tmp"))) {
            free(args->testdir);
        }
    }
    free(args);
    free_tests(testArray, 0);
    return runTestResult;
}

/* parse_args()
* −−−−−−−−−−−−−−−
* Function validates and places command line arguments into a struct Args
*
* arg1: argc is an int and is the amount of arguments in argv
* arg2: argv is a char** and contains the arguments from the command line
* arg3: args is an Args struct pointer that needs to be filled. It holds the 
* command line arguments.
*
* Returns: void.
* Errors: If there is an invalild arg then function will call program_error()
* and exit with status 3.
*/
void parse_args(int argc, char** argv, Args* args){
    if (argc > 6 || argc < 3) {
        program_error(3, 0, NULL, NULL);
    }
    args->jobfile = argv[argc - 2];
    args->program = argv[argc - 1];
    test_jobfile_program_arg(args);
    args->recreate = false;
    args->testdir = NULL;
    for (int i = 1; i < argc - 2; i++) {
        if (test_command_line_arg(argv[i], args) == 0) {
            if (i >= argc - 2) {
                free(args);
                program_error(3, 0, NULL, NULL);
            }
            args->testdir = argv[i + 1];
            i++;
        } else if (test_command_line_arg(argv[i], args) == 1) {
            if (i >= argc - 2) {
                free(args);
                program_error(3, 0, NULL, NULL);
            }
            args->recreate = true;
        } else {
            free(args);
            program_error(3, 0, NULL, NULL);
        }
    }
    if (!(args->testdir)) {
        args->testdir = "./tmp";
    }
}

/* test_jobfile_program_arg()
* −−−−−−−−−−−−−−−
* Function validates the job file and program args from argv
*
* arg1: jobfile is a char* and is the name of the job file
* arg2: program is a char* and is the name of the program to be tested
*
* Returns: void.
* Errors: If there is an invalild arg then function will call program_error()
* and exit with status 3.
*/
void test_jobfile_program_arg(Args* args) {
    if ((args->program[0] == '-' && args->program[1] == '-') || 
            (args->jobfile[0] == '-' && args->jobfile[1] == '-')) {
        free(args);
        program_error(3, 0, NULL, NULL);
    }
}

/* test_command_line_arg()
* −−−−−−−−−−−−−−−
* Function validates that the command line args that are not jobfile and 
* program are either testdir or recreate
*
* arg1: arg is a char*. It is the arg being tested
* arg2: duplicate is a bool that indicates if --testdir or --recreate have 
* ready been detected
*
* Returns: an int. 0 if arg is --testdir, 1 if arg is --recreate else 3.
* Errors: If there is an invalild arg then function will call program_error()
* and exit with status 3.
*/
int test_command_line_arg(char* arg, Args* args) {
    if (args->recreate || args->testdir) {
        free(args);
        program_error(3, 0, NULL, NULL);
    }
    char c1 = arg[0];
    char c2 = arg[1];
    // if arg starts with "--"
    if (c1 == '-' && c2 == '-') { 
        if (!strcmp(arg, "--testdir")) {
            return 0;
        } else if (!strcmp(arg, "--recreate")) {
            return 1;
        } else {
            free(args);
            program_error(3, 0, NULL, NULL);
        }
    }
    return 3;
}

/* job_line_syntax_error()
 * --------------
 * Function exits the program if a job line syntax error is detected.
 *
 * arg1: args is an Args*. Points to the Args struct.
 * arg2: line is a char*. Points to the current line being read from the 
 * jobfile.
 * arg3: splitLine is a char**. Is line split accross '\t'.
 * arg4: openedJobFile is a FILE*. Is the currently open job file.
 * arg5: tests is a Test*. It points to the first test struct.
 * arg6: lineNum is an int. It is the current line number of the open job file.
 *
 * Returns: void.
 * Errors: exit(3).
 */
void job_line_syntax_error(Args* args, char* line, char** splitLine, 
        FILE* openedJobFile, Test* tests, int testsSize, int lineNum) {
    fprintf(stderr, "testuqwordladder: Line %d of job file \"%s\": "
            "syntax error\n", lineNum, args->jobfile);
    free(args);
    free(line);
    free(splitLine);
    //fclose(openedJobFile);
    free_tests(tests, testsSize);
    exit(7);
}

/* program_error()
* −−−−−−−−−−−−−−−
* Function exits the program based on inputs.
*
* arg1: exitStatus is an int. It is the int that is exitted with
* arg2: optionalInt is an int. It is optionally printed in an fprintf 
* statement
* arg3: errorPrint is a char*. It is optionally printed in an fprintf 
* statement
* arg4: errorPrint2 is a char*. It is optionally printed in an fprintf 
* statement
*
* Returns: void.
* Errors: will exit with whatever existStatus is given.
*/
void program_error(int exitStatus, int optionalInt, char* errorPrint,
        char* errorPrint2) {
    if (exitStatus == 3) {
        fprintf(stderr, "Usage: testuqwordladder [--diffshow N] "
                "[--testdir dir] [--recreate] jobfile program\n");
        exit(exitStatus);
    } else if (exitStatus == 19) {
        fprintf(stderr, "testuqwordladder: Duplicate Test ID on line %d of "
                "test file \"%s\"\n", optionalInt, errorPrint);
        exit(exitStatus);
    } else if (exitStatus == 17) {
        fprintf(stderr, "testuqwordladder: Job file \"%s\" was empty\n", 
                errorPrint);
        exit(exitStatus);
    } else if (exitStatus == 1) {
        fprintf(stderr, "testuqwordladder: Unable to open input file "
                "\"%s\" specified on line %d of file \"%s\"\n", 
                errorPrint2, optionalInt, errorPrint);
        exit(exitStatus);
    } else if (exitStatus == 8) {
        fprintf(stderr, "testuqwordladder: Can\'t create directory named "
                "\"%s\"\n", errorPrint);
        exit(exitStatus);
    } else if (exitStatus == 6) {
        fprintf(stderr, "testuqwordladder: Can\'t open \"%s\" for "
                "writing\n", errorPrint);
        exit(exitStatus);
    } else if (exitStatus == 15) {
        fprintf(stderr, "testuqwordladder: No tests have been finished\n");
        exit(exitStatus);
    }
}

/* jobfile_specification()
* −−−−−−−−−−−−−−−
* Function creates all the job tests in the form of a Test*
*
* arg1: jobfile is a char*. It is the name of the job file.
*
* Returns: void.
* Errors: will call program_error(). Exit status 13 if the jobfile cannot be
* opened. Exit status 7 if job line is syntactually incorrect. Exit status of
* 19 if there is a duplicate test ID. Exit status of 17 if jobfile is empty.
*/
Test* jobfile_specification(Args* args, FILE* jf) {
    char* line;
    char** splitLine = NULL;
    Test* testArray = malloc(sizeof(Test));
    int testArraySize = 1;
    int testNum = 0;
    int lineNum = 0;
    while ((line = read_line(jf)) != NULL) {
        lineNum++;
        if (!strcmp(line, "")) {
            free(line);
            continue;
        }
        splitLine = split_string(line, '\t');
        if (splitLine[0][0] == '#') {
            free(line); 
            free(splitLine);
            continue;
        }
        if (splitLine[0] != NULL && splitLine[1] != NULL) {
            if (!strcmp(splitLine[0], "") || !strcmp(splitLine[1], "")) {
                job_line_syntax_error(args, line, splitLine, jf, testArray, 
                        testArraySize, lineNum);
            }
            if (testArraySize < testNum + 1) {
                testArraySize += 1;
                testArray = realloc(testArray, sizeof(Test) * testArraySize);
            }
            create_new_test(splitLine, &testArray[testNum]); 
            if (!check_test_struct_valid(testArray[testNum], lineNum, 
                    args->jobfile)) {job_line_syntax_error(args, line, 
                    splitLine, jf, testArray, testArraySize, lineNum);}
            check_input_file(args->jobfile, testArray[testNum].inputFileName, 
                    lineNum);
            testNum++;
            if (!check_test_id(testArray, testNum)) {
                program_error(19, lineNum, args->jobfile, NULL);
            }
        } else {job_line_syntax_error(args, line, splitLine, jf, testArray, 
                        testArraySize, lineNum);}
        free(splitLine);
        free(line);
    }
    testArray = realloc(testArray, sizeof(Test) * (testArraySize + 1));
    testArray[testNum].testId = NULL;
    if (!testNum) {
        program_error(17, 0, args->jobfile, NULL);
    }
    fclose(jf);
    return testArray;
}

/* create_new_test()
* −−−−−−−−−−−−−−−
* Function creates a new Test by filling a given Test*.
*
* arg1: splitLine is a char**. It is an array that contains the test ID, input
* file, and any optional args of a given test.
* arg2: toFill is a Test*. It is the Test struct that is to be filled.
*
* Returns: void.
*/
void create_new_test(char** splitLine, Test* toFill) {
    char** optionalArgs = malloc(sizeof(char*));
    int optionalArgsSize = 1;
    toFill->testId = strdup(splitLine[0]);
    toFill->inputFileName = strdup(splitLine[1]);
    int argNum = 0;
    int i = 2;
    while (splitLine[i]) {
        argNum++;
        optionalArgsSize += 1;
        optionalArgs = realloc(optionalArgs, optionalArgsSize * sizeof(char*));
        optionalArgs[i - 2] = strdup(splitLine[i]);
        i++;
    }
    optionalArgs[i - 2] = NULL;
    toFill->optionalArgsSize = optionalArgsSize;
    toFill->optionalArgs = optionalArgs;
}

/* check_test_struct_valid()
* −−−−−−−−−−−−−−−
* Function checks if a given Test is valid.
*
* arg1: test is a Test struct. It is the Test being tested.
* arg2: lineNum is an int. It is the current line number of the job file.
* arg3: jobfile is a char*. It is the name of the job file.
*
* Returns: bool. Returns false if invalid else returns true.
*/
bool check_test_struct_valid(Test test, int lineNum, char* jobfile) {
    if (!strcmp(test.testId, "")) {
        return false;
    }
    for (int i = 0; i < strlen(test.testId); i++) {
        if (test.testId[i] == '/') {
            return false;
        }
    }
    return true;
}

/* check_test_id()
* −−−−−−−−−−−−−−−
* Function checks if a given test id is valid.
*
* arg1: testArray is a Test*. It is the array of tests being tested.
* arg2: size is an int. It is the amount of Tests in the array.
*
* Returns: bool. If the test Id of the latest test is equal to the test Id
* of any other test withting the testArray return false. Else return true.
*/
bool check_test_id(Test* testArray, int size) {
    int lastTestLoc = size - 1;
    for (int i = 0; i < lastTestLoc; i++) {
        if (!strcmp(testArray[lastTestLoc].testId, testArray[i].testId)) {
            return false;
        }
    }
    return true;
} 

/* check_input_file()
* −−−−−−−−−−−−−−−
* Function checks if an input file of a Test can be opened.
*
* arg1: jobfile is a char*. It is the name of the job file.
* arg2: inputFile is a char*. It is the name of the input file of a given 
* Test.
* arg3: lineNum is an int. It is the current line number of the job file.
*
* Returns: void.
*/
void check_input_file(char* jobfile, char* inputFile, int lineNum) {
    FILE* jf = fopen(inputFile, "r");
    if (jf == NULL) {
        program_error(1, lineNum, jobfile, inputFile);
    }
    fclose(jf);
}

/* test_file_directory()
* −−−−−−−−−−−−−−−
* Function makes a directory.
*
* arg1: testdir is a char*. It is the name of the directory to be made.
*
* Returns: void.
* Errors: will call program_error(). Exit with exit status of 8 if the 
* unable to make the directory.
*/
void test_file_directory(char* testdir) {
    // 0700 is just permission for owner
    if (mkdir(testdir, 0777)) {
        if (errno != EEXIST) {
            program_error(8, 0, testdir, NULL);
        }
    }
}

/* generate_output()
* −−−−−−−−−−−−−−−
* Function generates the expected output using good-uqwordladder.
*
* arg1: test is a Test*. It is the pointer to the first Test.
* arg2: testdir is a char*. It is the name of the test directory.
* arg3: recreate is a bool. If true then all output files need to be 
* regenerated.
* arg4: jobfile is a char*. It is the name of the job file.
* arg5: program is a char*. It is the name of the program to be run 
* that generates the output.
*
* Returns: void.
* Errors: will call program_error(). Exit with exit status of 8 if the 
* unable to make the directory.
*/
void generate_output(Test* test, char* testdir, bool recreate, 
        char* jobfile, char* program) {
    int i = 0;
    while (test[i].testId) {
        // Create output file names
        char fstdout[BUFFER_LENGTH];
        char fstderr[BUFFER_LENGTH];
        char fexitstatus[BUFFER_LENGTH];
        sprintf(fstdout, "%s/%s.stdout", testdir, test[i].testId); 
        sprintf(fstderr, "%s/%s.stderr", testdir, test[i].testId);
        sprintf(fexitstatus, "%s/%s.exitstatus", testdir, test[i].testId);
        if (recreate) {
            regenerate_output_files(test[i], fstdout, fstderr, fexitstatus, 
                    program);
        } else if (!validate_file(fstdout, jobfile) || 
                !validate_file(fstderr, jobfile) || 
                !validate_file(fexitstatus, jobfile)) {
            regenerate_output_files(test[i], fstdout, fstderr, fexitstatus, 
                    program);
        }
        i++;
    }
}

/* regenerate_output_files()
* −−−−−−−−−−−−−−−
* Function generates the expected output for each test.
*
* arg1: test is a Test. It is a single Test.
* arg2: outputFile is a char*. It is the name of the file to contain the 
* expected stdout of the test.
* arg3: errorFile is a char*. It is the name of the file to contain the 
* expected stderr of the test.
* arg4: exitSFile is a char*. It is the name of the file to contain the 
* expected exit status of the program.
* arg5: program is a char*. It is the name of the program that generates the
* expected output.
*
* Returns: void.
*/
void regenerate_output_files(Test test, char* outputFile, char* errorFile, 
        char* exitSFile, char* program) {
    printf("Building expected output for test %s\n", test.testId);
    fflush(stdout);
    int out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    check_file_for_writing(out, outputFile);
    int err = open(errorFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    check_file_for_writing(err, errorFile);
    int exits = open(exitSFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    check_file_for_writing(exits, exitSFile);
    int input = open(test.inputFileName, O_RDONLY);
    pid_t childPid;
    if (!(childPid = fork())) {
        dup2(out, STDOUT_FILENO);
        dup2(err, STDERR_FILENO);
        dup2(input, STDIN_FILENO);
        close(out);
        close(err);
        close(exits);
        close(input);
        char* execArgs[1 + test.optionalArgsSize];
        execArgs[0] = program;
        for (int j = 1; j < test.optionalArgsSize + 1; j++) {
            execArgs[j] = test.optionalArgs[j - 1];
        }
        execvp(program, execArgs);
    } else {
        close(out);
        close(err);
        close(input);
        int status;
        waitpid(childPid, &status, 0);
        char exitStatus[BUFFER_LENGTH];
        sprintf(exitStatus, "%d\n", WEXITSTATUS(status));
        write(exits, exitStatus, strlen(exitStatus));
        close(exits);
    }
}

/* check_file_for_writing()
* −−−−−−−−−−−−−−−
* Function checks if a file can be written to given the file descriptor.
*
* arg1: fd is an int. It is the writing file descriptor to the file.
* arg2: fileName is a char*. It is the name of the file to be tested.
*
* Returns: void.
* Errors: will call program_error(). Exit with exit status of 6 if the file 
* cannot be written to.
*/
void check_file_for_writing(int fd, char* fileName) {
    if (fd == -1) {
        program_error(6, 0, fileName, NULL);
    }
}

/* validate_file()
* −−−−−−−−−−−−−−−
* Function checks the time an output file was created is the same as the 
* jobfile.
*
* arg1: fileName is a char*. It is the name of the file to be checked.
* arg2: jobfile is a char*. It is the name of the job file.
*
* Returns: bool. If the file described by fileName is older than jobfile 
* return false. Else return true.
*/
bool validate_file(char* fileName, char* jobfile) {
    struct stat testStat;
    struct stat jobStat;
    int testInt = stat(fileName, &testStat);
    stat(jobfile, &jobStat);
    if (testInt) {
        return false;
    } else {
        // Check if there is a difference between modification times of the 
        // files
        int timeCheck = compare_timespecs(testStat.st_mtim, 
                jobStat.st_mtim);
        if (timeCheck < 0) {
            return false;
        }
    }
    return true;
}

/* handle_signal()
* −−−−−−−−−−−−−−−
* Function handles the global variable signalKill
*
* Returns: void.
* Global variables modified: signalKill is set to true;
*/
void handle_signal(int s) {
    signalKill = true;
}

/* run_job_tests()
* −−−−−−−−−−−−−−−
* Function runs the each test on the given program. Compares output to 
* expected output.
*
* arg1: test is a Test*. Points to the first Test.
* arg2: testdir is a char*. It is the name of the test directory.
* arg3: jobfile is a char*. It is the name of the job file.
* arg4: program is a char*. It is the name of the program to be tested.
*
* Returns: int. Returns the number of tests passed.
*/
int run_job_tests(Test* test, char* testdir, char* jobfile, char* program) {
    int i = 0;
    int passed = 0;
    int passedTests = 0;
    pid_t* pids = NULL;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, 0);
    while (test[i].testId) {
        if (signalKill) {
            break;
        }
        // Create output file names
        char fstdout[BUFFER_LENGTH];
        char fstderr[BUFFER_LENGTH];
        char fexitstatus[BUFFER_LENGTH];
        sprintf(fstdout, "%s/%s.stdout", testdir, test[i].testId); 
        sprintf(fstderr, "%s/%s.stderr", testdir, test[i].testId);
        sprintf(fexitstatus, "%s/%s.exitstatus", testdir, test[i].testId);
        printf("Running test %s\n", test[i].testId);
        fflush(stdout);
        pids = run_test(test[i], fstdout, fstderr, program);
        passed = handle_child_status(pids, test[i].testId, fexitstatus); 
        if (passed == 3) {
            passedTests++;
        }
        free(pids);
        i++;
    }
    if (signalKill) {
        return passed_tests(passedTests, i - 1); 
    }
    return passed_tests(passedTests, i); 
}

/* passed_tests()
* −−−−−−−−−−−−−−−
* Function prints the amount of tests passed and returns the exit status to
* return from main.
*
* arg1: passedTests is an int. It is the number of passed tests.
* arg2: testNum is an int. It is the total number of tests.
*
* Returns: int. Returns the number of tests passed.
* Errors: will call program_error(). Exit with exit status of 15 if no tests
* are passed.
*/
int passed_tests(int passedTests, int testNum) {
    if (testNum > 0) { 
        printf("testuqwordladder: %d out of %d tests passed\n", passedTests, 
                testNum);
        return passedTests == testNum ? 0 : 14;
    } else {
        printf("testuqwordladder: No tests have been finished\n");
        return 15;
    }
}

/* run_test()
* −−−−−−−−−−−−−−−
* Function runs a single test. Creates and returns the pids of 3 children.
*
* arg1: test is a Test struct. It is the test to be tested...
* arg2: fstout is a char*. It is the name of the file containing the expected 
* stdout.
* arg3: fstderr is a char*. It is the name of the file containing the 
* expected stderr.
* arg4: program is a char*. It is the name of the program being tested.
*
* Returns: pid_t*. Returns the pids of the children.
*/
pid_t* run_test(Test test, char* fstdout, char* fstderr, char* program) {
    int fdout[2] = {0};
    int fderr[2] = {0};
    pipe(fdout);
    pipe(fderr);
    pid_t* pids = create_children(fdout, fderr, test.inputFileName,
            test.optionalArgs, test.optionalArgsSize, program, fstdout, 
            fstderr);
    close_pipes(fdout, fderr);
    // Set up sigtimedwait
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGCHLD);
    struct timespec timeout = {0};
    timeout.tv_sec = 1;
    timeout.tv_nsec = 500000000;
    sigtimedwait(&sigset, NULL, &timeout); 
    kill_children(pids);
    return pids;
}

/* kill_children()
* −−−−−−−−−−−−−−−
* Function sends a kill signal to all children.
*
* arg1: pids is a pid_t*. It is the array holding the pids.
*
* Returns: void.
*/
void kill_children(pid_t* pids) {
    if (pids) {
        kill(pids[WORDLADDER_CHILD], SIGKILL);
        kill(pids[CMP_CHILD_1], SIGKILL);
        kill(pids[CMP_CHILD_2], SIGKILL);
    }
}

/* create_children()
* −−−−−−−−−−−−−−−
* Function creates 3 children.
*
* arg1: fdout is an int*. Holds the file descripters for the pipe transmitting
* the output from the program.
* arg2: fderr is an int*. Holds the file descripters for the pipe transmitting
* the error from the program.
* arg3: inpulFileName is a char*. It is the name of the file thats given as 
* input to the program.
* arg4: optionalArgs is a char**. It is an array containing the args to be 
* given to the program as command line arguments.
* arg5: optionalArgsSize is an int. It is the size of the optionaArgs.
* arg6: program is a char*. It is the name of the program to be created.
* arg7: fstdout is a char*. It is the name of the file containing the expected
* stdout.
* arg8: fstderr is a char*. It is the name of the file containing the expected
* stderr.
*
* Returns: pid_t*. Returns the pids of the children.
*/
pid_t* create_children(int fdout[2], int fderr[2], char* inputFileName, 
        char** optionalArgs, int optionalArgsSize, char* program, 
        char* fstdout, char* fstderr) {
    pid_t* pids = malloc(3 * sizeof(pid_t));
    if (!(pids[WORDLADDER_CHILD] = fork())) {
        // Test program child
        int input = open(inputFileName, O_RDONLY);
        dup2(input, STDIN_FILENO);
        close(input);
        dup2(fdout[WRITE_END], STDOUT_FILENO);
        dup2(fderr[WRITE_END], STDERR_FILENO);
        close_pipes(fdout, fderr);
        char* execArgs[1 + optionalArgsSize];
        execArgs[0] = program;
        for (int j = 1; j < optionalArgsSize + 1; j++) {
            execArgs[j] = optionalArgs[j - 1];
        }
        execvp(program, execArgs);
        exit(99);
    } else if (!(pids[CMP_CHILD_1] = fork())) {
        // Stdout CMP program
        int output = open("/dev/null", O_WRONLY); 
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);
        close(output);
        dup2(fdout[READ_END], STDIN_FILENO);
        close_pipes(fdout, fderr);
        execlp("cmp", "cmp", fstdout, NULL);
        exit(99);
    } else if (!(pids[CMP_CHILD_2] = fork())) {
        // Stderr CMP program
        int output = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        dup2(output, STDOUT_FILENO);
        dup2(output, STDERR_FILENO);
        close(output);
        dup2(fderr[READ_END], STDIN_FILENO);
        close_pipes(fdout, fderr);
        execlp("cmp", "cmp", fstderr, NULL);
        exit(99);
    }
    free_char_array(optionalArgs, optionalArgsSize);
    return pids;
}

/* handle_child_status()
* −−−−−−−−−−−−−−−
* Function creates 3 children.
*
* arg1: pids is a pid_t*. Points to the pids of the children.
*
* Returns: int. Returns -1 if unable to complete job. Else returns the 
* amount of matches (stdout stderr exitstatus).
*/
int handle_child_status(pid_t* pids, char* testId, char* fexitstatus) {
    int matches = 0;
    int statusP;
    int statusC1;
    int statusC2;
    waitpid(pids[CMP_CHILD_1], &statusC1, 0);
    waitpid(pids[CMP_CHILD_2], &statusC2, 0);
    waitpid(pids[WORDLADDER_CHILD], &statusP, 0);
    FILE* bb = fopen(fexitstatus, "r");
    char* fExit = read_line(bb);
    int progExits = atoi(fExit);
    int cmpO = handle_child(pids[CMP_CHILD_1], statusC1, false);
    int cmpE = handle_child(pids[CMP_CHILD_2], statusC2, false);
    int prog = handle_child(pids[WORDLADDER_CHILD], statusP, true);
    fclose(bb);
    free(fExit);
    if (cmpO == -1 || cmpE == -1 || prog == -1) {
        printf("Unable to execute test job %s\n", testId);
        return -1;
    }
    if (!signalKill) {
        if (cmpO == 0) {
            printf("Job %s: Stdout matches\n", testId);
            fflush(stdout);
            matches++;
        } else {
            printf("Job %s: Stdout differs\n", testId);
            fflush(stdout);
        }
        if (cmpE == 0) {
            printf("Job %s: Stderr matches\n", testId);
            fflush(stdout);
            matches++;
        } else {
            printf("Job %s: Stderr differs\n", testId);
            fflush(stdout);
        }
        if (prog == progExits) {
            printf("Job %s: Exit status matches\n", testId);
            fflush(stdout);
            matches++;
        } else {
            printf("Job %s: Exit status differs\n", testId);
            fflush(stdout);
        }
    }
    return matches;
}

/* handle_child_status()
* −−−−−−−−−−−−−−−
* Function creates 3 children.
*
* arg1: pids is a pid_t*. Points to the pids of the children.
* arg2: prog is a bool. Tells the function if its checking the exit status of
* the program child or not.
*
* Returns: int. If not program child returns 1 if exit status is 0. Else if 
* exit status is 99 returns -1. Else returns 0.
*/
int handle_child(pid_t pid, int status, bool prog) {
    if (!prog) {
        if (WEXITSTATUS(status) == 0) {
            return 0;
        } else if (WEXITSTATUS(status) == 99) {
            return -1;
        } else {
            return 1;
        }
    } else {
        if (WEXITSTATUS(status) == 99) {
            return -1;
        } else {
            return WEXITSTATUS(status);
        }
    }
}

/* close_pipes()
* −−−−−−−−−−−−−−−
* Function closes 2 pipes.
*
* arg1: fdout is an int*. Holds the file descripters for the 1st pipe.
* arg2: fderr is an int*. Holds the file destripters for the 2nd pipe.
*
* Returns: int. If not program child returns 1 if exit status is 0. Else if 
* exit status is 99 returns -1. Else returns 0.
*/
void close_pipes(int fdout[2], int fderr[2]) {
    close(fdout[WRITE_END]);
    close(fdout[READ_END]);
    close(fderr[WRITE_END]);
    close(fderr[READ_END]);
}

/* free_char_array()
* −−−−−−−−−−−−−−−
* Function frees a char**.
*
* arg1: array is a char**. It is the array to be freeed.
* arg2: size is an int. It is the size of the array.
*
* Returns: void.
*/
void free_char_array(char** array, int size) {
    for (int i = 0; i < size; i++) {
        free(array[i]);
    }
    free(array);
}

/* free_tests()
* −−−−−−−−−−−−−−−
* Function frees all tests.
*
* arg1: test is a Test*. It points to the first test.
*
* Returns: void.
*/
void free_tests(Test* test, int testsSize) {
    int i = 0;
    if (testsSize) {
        while (i < testsSize) {
            free(test[i].testId);
            free(test[i].inputFileName);
            i++;
        }
    } else {
        while (test[i].testId) {
            free(test[i].testId);
            free(test[i].inputFileName);
            i++;
        }
    }
    free(test);
}
