/*
 * a1.c
 *  Written by: Benjamin Burn
 *  Student id: 45507087
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // this is for strlen() etc.
#include <ctype.h> // this is for the isdigit() function
#include <csse2310a1.h> // this is for get_wordladder_word()
#define MAX_ATOI_VALUE 2147483647

// Function prototypes
int game_over(int steps);
void program_error(int, char*);
void check_is_digit(char*);
void check_words_length(char*, char*, int, int);
void check_length(char*, int);
void check_length_bounds(int);
int init_word_length(char*, char*, int, int); 
void check_command_null(char*);
void check_word_valid(char*, char*);
int check_word(char*);
void check_max_steps(int, int);
void check_file_path(char*);
void convert_words_upper(char*, char*);
void convert_upper(char*);
FILE* open_file(char*);
char* read_line(FILE*);
char* get_word(char*, int);
void game(char*, char*, int, int, char**);
void free_function(char*, char*, char**, char**);
void word_suggestion(char**, char**, char*, int);
int word_letter_difference(char*, char*);
int check_word_dictionary(char**, char*);
char** new_dictionary(FILE*, int);
int read_line_conditions(char**, char**, char*, char*, char*, char*, 
        int, int, int); 

// Main
int main(int argc, char* argv[]) {
    // Commands - Set at default values
    char* initWord = NULL;
    char* endWord = NULL;
    int wordLength = 4; 
    int wLI = 0;
    int maxSteps = 19;
    int mSI = 0;
    char* defaultFilePath = "/usr/share/dict/words";    
    char* inputFilePath = NULL;
    char** wordDict;
    int commandCount = 1;
    // checking command line for each command and executing appropriate logic
    while (commandCount < argc) {
        if ((!strcmp(argv[commandCount], "--init")) && (initWord == NULL)) {
            check_command_null(argv[commandCount + 1]);
            initWord = argv[commandCount + 1];
            commandCount++;        
        } else if ((!strcmp(argv[commandCount], "--end")) &&
                (endWord == NULL)) {
            check_command_null(argv[commandCount + 1]);
            endWord = argv[commandCount + 1];
            commandCount++;        
        } else if ((!strcmp(argv[commandCount], "--len") && (wLI == 0))) {
            check_is_digit(argv[commandCount + 1]);
            // need to make its own fn
            char* ptrTest;
            long int testInt = strtol(argv[commandCount + 1], &ptrTest, 10);
            if (testInt > MAX_ATOI_VALUE) {
                wordLength = 10;
            } else {
                wordLength = atoi(argv[commandCount + 1]);
                if (wordLength <= 0) {
                    program_error(8, NULL);
                }
            }
            wLI++;
            commandCount++;        
        } else if ((!strcmp(argv[commandCount], "--max")) && (mSI == 0)) {
            check_is_digit(argv[commandCount + 1]);
            // need to make its own fn
            char* ptrTest;
            long int testInt = strtol(argv[commandCount + 1], &ptrTest, 10);
            if (testInt > MAX_ATOI_VALUE) {
                maxSteps = 51;
            } else {
                maxSteps = atoi(argv[commandCount + 1]);
                if (maxSteps <= 0) {
                    program_error(8, NULL);
                }
            }
            mSI++;
            commandCount++;        
        } else if ((!strcmp(argv[commandCount], "--dictionary")) &&
                (inputFilePath == NULL)) {
            check_command_null(argv[commandCount + 1]);
            inputFilePath = argv[commandCount + 1];
            commandCount++;        
        } else {
            program_error(8, NULL);
        }
        commandCount++;        
    }
    check_word_valid(initWord, endWord);
    wordLength = init_word_length(initWord, endWord, wordLength, wLI);
    
    check_words_length(initWord, endWord, wordLength, wLI); 
    check_max_steps(wordLength, maxSteps);
    convert_words_upper(initWord, endWord);

    if (inputFilePath == NULL) {
        FILE* dictionary = open_file(defaultFilePath);
        wordDict = new_dictionary(dictionary, wordLength);
        fclose(dictionary);
    } else {
        FILE* dictionary = open_file(inputFilePath);
        wordDict = new_dictionary(dictionary, wordLength);
        fclose(dictionary);
    }
    initWord = get_word(initWord, wordLength);
    endWord = get_word(endWord, wordLength);
    game(initWord, endWord, wordLength, maxSteps, wordDict);
}

/* program_error()
* −−−−−−−−−−−−−−−
* Given an int and a fileName exits the program with the appropriate message.
*
* arg1: int i is the exit status where i > 0.   
* argument2: char* fileName is a string containing the path to a file.
*
* Returns: void
*/
void program_error(int i, char* fileName) {
    if (i == 8) {
        fprintf(stderr, "Usage: uqwordladder [--init sourceWord] "
                "[--end endWord] [--max maxSteps] [--len length] "
                "[--dictionary filename]\n");
        exit(8);
    }
    if (i == 15) {
        fprintf(stderr, "uqwordladder: Word lengths must be consistent\n");
        exit(15);
    }
    if (i == 17) {
        fprintf(stderr, "uqwordladder: Word lengths should be between 2 "
                "and 9 (inclusive)\n");
        exit(17);
    }
    if (i == 1) {
        fprintf(stderr, "uqwordladder: Words should not contain non-letters"
                "\n");
        exit(1);
    }
    if (i == 6) {
        fprintf(stderr, "uqwordladder: Words must not be the same\n");
        exit(6);
    }
    if (i == 3) {
        fprintf(stderr, "uqwordladder: Step limit must be word length to "
                "50 (inclusive)\n");
        exit(3);
    }
    if (i == 10) {
        fprintf(stderr, "uqwordladder: File \"%s\" cannot be opened\n",
                fileName);
        exit(10);
    }
}

/* check_is_digit()
* −−−−−−−−−−−−−−−
* Calls program_error() if the given argument contains a non-digit.
*
* arg1: arg is a char* pointing to a string.
*
* Returns: void
*/
void check_is_digit(char* arg) {
    for (int i = 0; i < strlen(arg); i++) {
        if (!isdigit(arg[i])) {
            program_error(8, NULL);
        }
    }
}

/* check_command_null()
* −−−−−−−−−−−−−−−
* Calls program_error() if the given argument is NULL.
*
* arg1: arg is a char* pointing to a string.
*
* Returns: void
*/
void check_command_null(char* arg) {
    if (arg == NULL) {
        program_error(8, NULL);
    }
}

/* check_words_length()
* −−−−−−−−−−−−−−−
* Calls program_error() if any of arg1, arg2, arg3 lengths do not 
* equal eachother, or if the length of arg1, arg2 or arg3 is greater than 9
* or less than 2.
* 
* arg1: initWord is a char* pointing to a string containing the initWord.
*       Assumes initWord != NULL and only contains letters.
* arg2: endWord is a char* pointing to a string containing the endWord.
*       Assumes initWord != NULL and only contains letters.
* arg3: wordLength is an int describing the length of the words
* arg4: wLI is an int indicating if the user inputted a length
*
* Returns: void
*/
void check_words_length(char* initWord, char* endWord, int wordLength,
        int wLI) {
    if ((initWord != NULL) && (endWord != NULL)) {
        check_length_bounds(wordLength);
        check_length_bounds(strlen(initWord));
        check_length_bounds(strlen(endWord));
        check_length(initWord, wordLength);
        check_length(endWord, wordLength);
    }
    if ((initWord == NULL) && (endWord != NULL)) {
        check_length_bounds(strlen(endWord));
        check_length(endWord, wordLength);
    }
    if ((endWord == NULL) && (initWord != NULL)) {
        check_length_bounds(strlen(initWord));
        check_length(initWord, wordLength);
    }
}

/* check_length()
* −−−−−−−−−−−−−−−
* Calls program_error() if the length of arg1 does not equal arg2 
* 
* arg1: wird is a char* pointing to a string.
*       Assumes word != NULL and only contains letters.
* arg2: wordLength is an int describing the length of the word.
*
* Returns: void
*/
void check_length(char* word, int wordLength) {
    if (strlen(word) != wordLength) {
        program_error(15, NULL);
    }
}

/* check_length_bounds()
* −−−−−−−−−−−−−−−
* Calls program_error() if arg1 > 9 || arg1 < 2. 
* 
* arg1: length is an int describing the length of the word.
*
* Returns: void
*/
void check_length_bounds(int length) {
    if ((length < 2) || (length > 9)) {
        program_error(17, NULL);
    }
}

/* init_word_length()
* −−−−−−−−−−−−−−−
* Determines the correct word length for the game. 
* 
* arg1: initWord is a char* pointing to a string containing the initWord.
* arg2: endWord is a char* pointing a string containing the endWord.
* arg3: wLI is the word length indicator. Indicates whether the user has input
* a desired length.
*
* Returns: The length of either the initWord, endWord or 4 depending on if any
* of the args are NULL.
*/
int init_word_length(char* initWord, char* endWord, int wordLength,
        int wLI) {
    int len = wordLength;
    if (wLI == 0 && initWord != NULL) {
        len = strlen(initWord);
        return len; 
    }
    if (wLI == 0 && endWord != NULL) {
        len = strlen(endWord);
        return len;
    }
    return len;
}

/* check_word_valid()
* −−−−−−−−−−−−−−−
* Calls program_error() if arg1 or arg1 contains a non-letter character or 
* both words are equal. 
* 
* arg1: initWord is a char* pointing to a string containing initWord.
* arg2: endWord is a char* pointing to a string containing endWord.
*
* Returns: void
*/
void check_word_valid(char* initWord, char* endWord) {
    if (initWord != NULL) {
        if (check_word(initWord) == 1) {
            program_error(1, NULL);
        }
    }
    if (endWord != NULL) {
        if (check_word(endWord) == 1) {
            program_error(1, NULL);
        }
    }
    if (endWord != NULL && initWord != NULL) {
        if (!strcmp(initWord, endWord)) {
                program_error(6, NULL);
        }
    }
}

/* check_word()
* −−−−−−−−−−−−−−−
* Determines if the arg contains a non-letter character. 
* 
* arg1: word is a char* pointing to a string.
*
* Returns: 0 if word contains only characters else returns 1.
* else returns 1
*/
int check_word(char* word) {
    for (int i = 0; word[i]; i++) {
        if (!isalpha(word[i])) {
            return 1;
        }
    }
    return 0;
}

/* check__max_steps()
* −−−−−−−−−−−−−−−
* Calls program_error() if the maxSteps are outside the range of 
* wordLength < maxSteps < 50.
* 
* arg1: wordLength is an int describing the length of the words.
* arg2: maxSteps is an int describing the maximum number of steps in the game.
*
* Returns: void
*/
void check_max_steps(int wordLength, int maxSteps) {
    if ((maxSteps < wordLength) || (maxSteps > 50)) {
        program_error(3, NULL);
    }
}

/* check_max_steps()
* −−−−−−−−−−−−−−− 
* Converts both arg1 and arg2 to uppercase characters.
* 
* arg1: initWord is a char* pointing to a string containing the initWord.
*       Assuming initWord contains only letters.
* arg2: endWord is a char* pointing to a string containing the endWord.
*       Assuming endWord contains only letters.
*
* Returns: void
*/
void convert_words_upper(char* initWord, char* endWord) {
    if (initWord != NULL) {
        convert_upper(initWord);
    }
    if (endWord != NULL) {
        convert_upper(endWord);
    }
}

/* convert_upper()
* −−−−−−−−−−−−−−− 
* Converts arg1 and arg2.
* 
* arg1: word is a char* pointing to a string containing a word.
*       Assuming initWord contains only letters.
*
* Returns: void
*/
void convert_upper(char* word) {
    for (int i = 0; word[i]; i++) {
        word[i] = toupper(word[i]);
    }
}

/* open_file()
* −−−−−−−−−−−−−−− 
* Function opens a file given a file path and returns a file pointer.
* 
* arg1: filePath is a char* pointing to a string containing a file path.
*
* Returns: A file pointer FILE*.
* Errors: Will call program_error() if the file could not be opened.
*/
FILE* open_file(char* filePath) {
    FILE* fp = fopen(filePath, "r");
    if (fp == NULL) {
        program_error(10, filePath);
    }
    return fp;
}

/* get_word()
* −−−−−−−−−−−−−−− 
* Function returns a new word of the length of arg2.
* 
* arg1: word is a char* that points to a string
* arg2: length is an int describing the length of the word
*
* Returns: A word as a char* as either a new word or arg1 if arg1 == NULL.
*/
char* get_word(char* word, int length) {
    if (word == NULL) {
        char* newWord = malloc(sizeof(char) * length + 1);
        strcpy(newWord, get_uqwordladder_word(length));
        return newWord; 
    } else {
        return strdup(word);
    }
}

/* game()
* −−−−−−−−−−−−−−− 
* Function runs the game. Takes user input and runs logic through 
* read_line_conditions() function.
* 
* arg1: initWord is a char* that points to a string containing the initWord.
* arg1: endWord is a char* that points to a string containing the endWord.
* arg3: wordLength is an int describing the length of both arg1 and arg2.
* arg4: maxSteps is an int describing the maximum steps the user can take.
* arg5: dict is a char** that represents an array of strings
*
* Returns: void
* Errors: Through the read_line_conditions() function, if the user runs out 
* of steps the game will call exit(5). If the user gives up, ctrl+d, the 
* function will call exit(16). Success will call exit(0).
*/
void game(char* initWord, char* endWord, int wordLength, int maxSteps,
        char** dict) {
    printf("Welcome to UQWordLadder!\nYour goal is to turn \'%s\' into "
            "\'%s\' in at most %d steps\n"
            "Enter word %d (or ? for help):\n", initWord, endWord,
            maxSteps, 1);
    int wordNum = 1;
    int start = 0;
    char* currentWord;
    char** usedWords = malloc(sizeof(char*) * 2);
    int usedWordsSize = 2;
    usedWords[0] = initWord;
    usedWords[1] = NULL;
    currentWord = initWord;
    while (1) {
        if (start != 0) {
            printf("Enter word %d (or ? for help):\n", wordNum);
        }
        start++;
        char* userInput = read_line(stdin);
        int rLC = read_line_conditions(dict, usedWords, currentWord, 
                userInput, endWord, initWord, wordNum, wordLength, maxSteps); 
        if (rLC == 1) {
            free(userInput);
            continue;
        }
        if (rLC == 0) {
            currentWord = userInput;
            wordNum++;
        }
        if (rLC == 2) {
            free(userInput);
            //wordNum--;
            printf("Enter word %d (or ? for help):", wordNum);
            start = 0;
        }
        if (wordNum + 1 > usedWordsSize) {
            usedWordsSize *= 2;
            usedWords = realloc(usedWords, sizeof(char*) * usedWordsSize); 
        }
        usedWords[wordNum - 1] = currentWord;
        usedWords[wordNum] = NULL;
    }
}

/* read_line_conditions()
* −−−−−−−−−−−−−−− 
* Function runs the conditional logic on the given parameters according to 
* game rules.
* 
* arg1: dic is a char** that points to an array of strings containing the 
* default/input dictionary
* arg2: usedWords is a char** that points to an array of strings containing
* the words already inputted (includes initWord).
* arg3: currentWord is a char* that points to a string containing the 
* currentWord in the game.
* arg4: userInput is a char* that points to a string containing the current
* input from the user.
* arg5: endWord is a char* that points to a string containing the endWord.
* arg6: initWord is a char* that points to a string containing the initWord.
* arg7: wordNum is an int describing current step the user is on.
* arg8: wordLength is an int describing the length of the initWord and 
* endWord.
* arg9: maxSteps is an int that describes the maximums steps the user has.
* Returns: void
* Errors: If the user runs out of steps the game will call exit(5). If the
* user gives up, ctrl+d, the function will call exit(16). Success will call
* exit(0).
*/
int read_line_conditions(char** dict, char** usedWords, char* currentWord, 
        char* userInput, char* endWord, char* initWord, int wordNum, 
        int wordLength, int maxSteps) {
    if (userInput == NULL) {
        printf("Game over - you gave up.\n");
        free_function(initWord, endWord, dict, usedWords);
        exit(16);
    }
    if (*userInput == '\0') {
        return 2;
    }
    if (!strcmp(userInput, "?")) {
        word_suggestion(dict, usedWords, currentWord, wordNum);
        return 1;
    }
    if (strlen(userInput) != wordLength) {
        printf("Word should be %d characters long - try again."
                "\n", wordLength);
        return 1;
    }
    if (check_word(userInput) == 1) {
        printf("Word contains non-letter characters - try again.\n");
        return 1;
    }
    if (word_letter_difference(currentWord, userInput) != 1) {
        printf("Word must have only one letter different - try again.\n");
        return 1;
    }
    if (check_word_dictionary(usedWords, userInput)) {
        printf("Previous word can't be repeated - try again.\n");
        return 1;
    }
    if (!check_word_dictionary(dict, userInput)) {
        printf("Word can't be found in dictionary - try again.\n");
        return 1;
    }
    if (!strcmp(userInput, endWord)) {
        printf("You solved the ladder in %d steps.\n", wordNum);
        free_function(initWord, endWord, dict, usedWords);
        exit(0);
    }
    if (wordNum == maxSteps) {
        printf("Game over - you ran out of steps.\n");
        free_function(initWord, endWord, dict, usedWords);
        exit(5);
    }
    return 0;
}

/* free_function()
* −−−−−−−−−−−−−−− 
* Function frees the memory of all pointers given to it.
* 
* arg1: initWord is a char* that points to a string containing the initWord.
* arg2: endWord is a char* that points to a string containing the endWord.
* arg4: dict is a char** that points to an array of strings containing the 
* default/input dictionary
* arg5: usedWords is a char** that points to an array of strings containing
* the words already inputted (includes initWord).
*
* Returns: void
*/
void free_function(char* initWord, char* endWord, char** wordDict, 
        char** usedWords) {
    free(initWord);
    free(endWord);
    int i = 0;
    while (wordDict[i] != NULL) {
        free(wordDict[i]);
        i++;
    }
    free(wordDict);
    int j = 1;
    while (usedWords[j] != NULL) {
        free(usedWords[j]);
        j++;
    }
    free(usedWords);
}

/* word_suggestion()
* −−−−−−−−−−−−−−− 
* Function prints words that are 1 letter different than arg3.
* 
* arg1: dict is a char** that points to an array of strings. This is the 
*       default dictionary or the user input one.
* arg1: usedWords is a char** that points to an array of string containing 
*       the words already inputted by the used (including the initWord).
* arg3: currentWord is a char* that points to a string containing the 
*       current word in the game.
* arg4: usedWordsSize is an int describing the amount of words in the 
*       usedWords dictionary.
*
* Returns: void
*/
void word_suggestion(char** dict, char** usedWords, char* currentWord,
        int usedWordsSize) {

    bool anyGuesses = false;
    // Iterate over dict
    for (int i = 0; dict[i] != NULL; i++) {
        int count = 0;
        // Convert each word into uppercase
        convert_upper(dict[i]);
        // Checks if there is a 1 letter difference between dict[i]
        // and currentWord
        if (word_letter_difference(dict[i], currentWord) == 1) {
            // Iterate over usedWords array
            for (int j = 0; usedWords[j] != NULL; j++) {
                // Checks if dict[i] != usedWords[j]
                if (strcmp(dict[i], usedWords[j])) {
                    // increases count if the used word is not in the 
                    // dictionary
                    count++;
                    // If the dict[i] is not in usedWords and its 1 letter
                    // different to currentWord then print
                    if (count == usedWordsSize) {
                        if (!anyGuesses) {
                            printf("Suggestions:-----------\n");
                            anyGuesses = true;
                        }
                        printf(" %s\n", dict[i]);
                    }
                }
            }
        }
    }
    if (anyGuesses) {
        printf("-----End of Suggestions\n");
    } else {
        printf("No suggestions available.\n");
    }
}

/* word_letter_difference()
* −−−−−−−−−−−−−−− 
* Function determines the amount of characters different between arg1 and 
* arg2.
* 
* arg1: word1 is a char* that points to a string containing a word.
*       Assuming word1 != NULL.
* arg1: word2 is a char* that points to a string containing a word.
*       Assuming word2 != NULL.
*
* Returns: dif which is an int representing how many characters are different
* between arg1 and arg2.
*/
int word_letter_difference(char* word1, char* word2) {
    int dif = 0;
    convert_upper(word1);
    convert_upper(word2);
    for (int i = 0; word1[i]; i++) {
        if (word1[i] != word2[i]) {
            dif++;
        }
    }
    return dif;
}

/* check_word_dictionary()
* −−−−−−−−−−−−−−− 
* Function determines if a word is in a dictionary (array of strings). 
* 
* arg1: dict is a char** that points to an array of strings.
* arg1: word is a char* that points to a string containing a word.
*       Assuming word2 != NULL.
*
* Returns: 1 the word is in the dictionary. Else returns 0. 
*/
int check_word_dictionary(char** dict, char* word) {
    for (int i = 0; dict[i] != NULL; i++) {
        convert_upper(dict[i]);
        if (!strcmp(dict[i], word)) {
            return 1;
        }
    }
    return 0;
}

/* read_line()
* −−−−−−−−−−−−−−− 
* Function reads a line from a stream. Retruns as a char*. 
* 
* arg1: stream is a FILE* that points to a file.
*
* Returns: The next line in the file as a char*. 
*/
char* read_line(FILE* stream) {
    int bufferSize = 80;
    char* buffer = malloc(sizeof(char) * bufferSize);
    int numRead = 0;
    int next;
    if (feof(stream)) {
        return NULL;
    }
    while (1) {
        next = fgetc(stream);
        if (next == EOF && numRead == 0) {
            free(buffer);
            return NULL;
        }
        if (numRead == bufferSize - 1) {
            bufferSize *= 2;
            buffer = realloc(buffer, sizeof(char) * bufferSize);
        }
        if (next == '\n' || next == EOF) {
            buffer[numRead] = '\0';
            break;
        }
        buffer[numRead++] = next;
    }
    return buffer;
}

/* new_dictionary()
* −−−−−−−−−−−−−−− 
* Function creates a new dictionary (as an array of strings). 
* 
* arg1: stream is a FILE* that points to a file.
* arg2: wordLength is the length of the desired words to be placed into the 
*       new dictionary.
*
* Returns: newDict which is a char**. 
*/
char** new_dictionary(FILE* stream, int wordLength) {
    char** newDict = malloc(sizeof(char*));
    int size = 1;
    int validWords = 0;
    while (1) {
        char* line = read_line(stream);
        if (line == NULL) {
            break;
        }
        // y is used to check if the word = wordLength
        int y = 0;
        int lengthInd = 0;
        // for each character in line
        for (int i = 0; line[i]; i++) {
            // if all characters in line are letters
            if (!isalpha(line[i])) {
                lengthInd++;
                break;
            } else {
                y++;
            }
        }
        if (y == wordLength && lengthInd == 0) {
            if (size < validWords + 1) {
                size *= 2;
                newDict = realloc(newDict, sizeof(char*) * size);
            }
            newDict[validWords] = line;
            validWords++;
        } else {
            free(line);
        }
    }
    newDict[validWords] = NULL;
    return newDict;
}
