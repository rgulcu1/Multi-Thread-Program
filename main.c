#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#define MaxCharacter 200 // define the maximum number of chracter number supported for one line

char fileName[MaxCharacter]; //create a global fileName variable for keep user entered file name.
int readThreadNum, upperThreadNum, replaceThreadNum, writeThreadNum = 0; //create global variables for thread numbers
int totalLine; //totaLine keep the number of line for input file

//create the necessary mutex and semaphores
pthread_mutex_t readIndexMutex = PTHREAD_MUTEX_INITIALIZER; //for protect the index to read from file
pthread_mutex_t storeIndexMutex = PTHREAD_MUTEX_INITIALIZER; //for protect index to add readed line to array
pthread_mutex_t upperIndexMutex = PTHREAD_MUTEX_INITIALIZER; //for protect array index to apply upper operation
pthread_mutex_t replaceIndexMutex = PTHREAD_MUTEX_INITIALIZER; //for protect array index to apply replace operation
pthread_mutex_t writeReadMutex = PTHREAD_MUTEX_INITIALIZER; //to prevent simultaneous writing and reading and more than one write operation same time
pthread_mutex_t readerCountMutex = PTHREAD_MUTEX_INITIALIZER; //to prevent increment and decrement to readerCount

sem_t *replaceSemaphore; //for control the number of elements that have not been raplaceed in the array
sem_t *upperSemaphore; //for control the number of elements that have not been uppered in the array

sem_t *replaceWriteSemaphore; //for control the number of the elements which replaced already
sem_t *upperWriteSemaphore; //for control the number of the elements which uppered already

int readIndex = 0; //keep the current read Index on read txt file
int storeIndex = 0;//keep the current store Index for add element to array
int upperIndex = 0;//keep the current upper Index on array
int replaceIndex = 0;//keep the current read Index on array
int writeIndex =0;//keep the current write Index for write txt file
int readerCount=0;//keep the current reader count for control any reader thread currently reading

struct lineData { //for keeps line and line index on array
    char *line;
    int lineIndex;
};
typedef struct lineData lineData;

struct readThreadData { //for send data to read Threads
    int TheradIndex;
    lineData *lines; // array of lines
    int * lineBytes; // byte information about lines
};
typedef struct readThreadData readThreadData;

struct upperThreadData { //for send data to upper Threads
    int TheradIndex;
    lineData *lines;
    pthread_mutex_t *changeMutexes; //mutex array for protect every index of line array to simultaneous  upper and replace process
};
typedef struct upperThreadData upperThreadData;

struct replaceThreadData { //for send data to replace Threads
    int TheradIndex;
    lineData *lines;
    pthread_mutex_t *changeMutexes;
};
typedef struct replaceThreadData replaceThreadData;

struct writeThreadData { //for send data to write Threads
    int TheradIndex;
    lineData *lines;
    int * lineBytes;
};
typedef struct writeThreadData writeThreadData;



//check the file is exist.If file is exist return 1 otherwise return 0.
int fileIsExists() {
    FILE *file;
    file = fopen(fileName, "r");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}
//check the arguments from user entered.If arguments not fit the syntax display an error message and terminate the program.
void checkArguments(int argc, char *argv[]) {
    if (argc != 8) {   /* check for valid number of command-line arguments if not fit display an error message and terminate the program */
        fprintf(stderr,"Invalid Arguments. The syntax must be: \n%s -d fileName.txt -n readThreadNum, upperThreadNum, replaceThreadTNum, writeThreadNum \n",argv[0]);
        exit(-1);
    }
    int dIndex = 0;
    int nIndex = 0;
    for (int i = 0; i < argc; ++i) { //find the index of -d and -n
        if (strcmp(argv[i], "-d") == 0) dIndex = i;
        if (strcmp(argv[i], "-n") == 0) nIndex = i;
    }

    if (dIndex == 0 || nIndex == 0) { // if input is not contain -n or -d display en error message and terminate the program.
        fprintf(stderr,"Invalid Arguments. The syntax must be: \n%s -d fileName.txt -n readThreadNum, upperThreadNum, replaceThreadTNum, writeThreadNum \n",argv[0]);
        exit(-1);
    }
    if (nIndex > argc - 4 || dIndex > argc - 1) { // if index of -d and -n location is not appropriate display an error message and terminate the program.
        fprintf(stderr,"Invalid Arguments. The syntax must be: \n%s -d fileName.txt -n readThreadNum, upperThreadNum, replaceThreadTNum, writeThreadNum \n",argv[0]);
        exit(-1);
    }

    //assign file name and thread number variables for option location
    strcpy(fileName, argv[dIndex + 1]);
    readThreadNum = atoi(argv[nIndex + 1]);
    upperThreadNum = atoi(argv[nIndex + 2]);
    replaceThreadNum = atoi(argv[nIndex + 3]);
    writeThreadNum = atoi(argv[nIndex + 4]);

    //if given file is not exist display an erro message and terminate the program.
    if (fileIsExists() == 0) {
        fprintf(stderr, "The given file is not exist.\n");
        exit(-1);
    }

    if (readThreadNum == 0 || upperThreadNum == 0 || replaceThreadNum == 0 || writeThreadNum == 0) { //if given thread number is not integer or 0 display en error message and terminate the program.
        fprintf(stderr,"Invalid Arguments. The syntax must be: \n%s -d fileName.txt -n readThreadNum, upperThreadNum, replaceThreadTNum, writeThreadNum \n",argv[0]);
        exit(-1);
    }
}
//return the total number of line of txt file
int getTotalLineCount(){
    FILE *file;
    file = fopen(fileName, "r");
    int ch=0;
    int lineCounter=0;
    while(!feof(file))
    {
        ch = fgetc(file);
        if(ch == '\n')
        {
            lineCounter++;
        }
    }
    return lineCounter;
}
//calculate all bytes for every line on txt
void calculateLineBytes(int lineBytes[]){
    FILE *file = fopen(fileName, "r");
    int ch;
    int lineIndex=1;
    int charCounter=0;
    lineBytes[0]=0;
    while((ch=fgetc(file))!=EOF) {
        charCounter++;
        if(ch=='\n'){
            lineBytes[lineIndex]=charCounter; // keeps on the byte array
            lineIndex++;
        }

    }
}
//read a specific line depend on byte from txt and return them
char *readSpecificLine(int lineByte) {
    char line[MaxCharacter];

    FILE *file = fopen(fileName, "r");
    fseek(file, lineByte, SEEK_SET); // go to byte value from start the txt
    fgets(line, sizeof line, file);
    fclose(file);

    char *currentLine = (char *) malloc(sizeof(char) * MaxCharacter);
    strcpy(currentLine, line);
    return currentLine;
}
//convert the given string to uppercase version
void convertStringToUpperCase(char * String){
    int counter = 0;
    while (String[counter] != '\0') {
        String[counter] = toupper( String[counter]);
        counter++;
    }
}
//replace all blanks with underscore on given String
void replaceBlanksWithUnderScore(char * String){
    int counter = 0;
    while (String[counter] != '\0') {
        if(isspace(String[counter])) String[counter]='_';
        counter++;
    }
}
//write the given line to txt file depend on given byte vlaue
void writeFileSpecificIndex(char * newLine ,int lineByte){

    FILE *file = fopen(fileName, "r+");
    fseek(file, lineByte, SEEK_SET); // go to byte value from start of file

    fprintf(file,"%s",newLine);
    fclose(file);
}

void *readFile(void *threadData) {
    int threadIndex, lineIndex, storeArrayIndex;
    lineData *lines;
    int *lineBytes;
    //take inputs
    readThreadData *data = (readThreadData *) threadData;
    threadIndex = data->TheradIndex;
    lines =data->lines;
    lineBytes = data->lineBytes;
    char *readedLine = (char *) malloc(sizeof(char) * MaxCharacter);

    while (1) {
        // protect the global readIndex
        pthread_mutex_lock(&readIndexMutex);
        lineIndex = readIndex; //assign index and increment
        readIndex++;
        pthread_mutex_unlock(&readIndexMutex);
        //protect the reader count
        pthread_mutex_lock(&readerCountMutex);
        readerCount++;
        if(readerCount==1) pthread_mutex_lock(&writeReadMutex); //if reader count equal 1 there are no reader already.So check the any writer is runnnig.
        pthread_mutex_unlock(&readerCountMutex);

        strcpy(readedLine, readSpecificLine(lineBytes[lineIndex])); //read process

        pthread_mutex_lock(&readerCountMutex);
        readerCount--; // decrement the counter
        if(readerCount==0) pthread_mutex_unlock(&writeReadMutex); // if the las reader thread is exit unlock the lock.
        pthread_mutex_unlock(&readerCountMutex);

        //protect the index of store on array
        pthread_mutex_lock(&storeIndexMutex);
        storeArrayIndex = storeIndex; //assign and increment
        storeIndex++;
        pthread_mutex_unlock(&storeIndexMutex);



        lines[storeArrayIndex].line = (char *) malloc(sizeof(char) * strlen(readedLine)); //allocate memory
        lines[storeArrayIndex].lineIndex = lineIndex; //set the line index indormation
        strcpy(lines[storeArrayIndex].line, readedLine);//set the line of on array
        strtok(lines[storeArrayIndex].line,"\n"); //delete new line character

        printf("Read_%d\t\tRead_%d read the line %d which is \"%s", threadIndex, threadIndex, lineIndex + 1,readedLine); // print display message
        usleep(100);// a very little delay for output order

        //add one to semaphores
        sem_post(replaceSemaphore);
        sem_post(upperSemaphore);

        if(lineIndex >= (totalLine-1)-(readThreadNum-1)){ //if the above threads currently are complates the all indexes break the loop
            break;
        }
    }

    pthread_exit(0); // kill thread
}

void *runUpper(void *threadData) {
    int threadIndex, arrayIndex;
    lineData *lines;
    pthread_mutex_t *indexChangeMutex;
    //take inputs
    upperThreadData * data = (upperThreadData*) threadData;
    threadIndex=data->TheradIndex;
    lines =data->lines;
    indexChangeMutex = data->changeMutexes;
    char * line = malloc(sizeof(char)*MaxCharacter);

    while (1) {
        sem_wait(upperSemaphore); //wait for element on array which is not uppered before
        pthread_mutex_lock(&upperIndexMutex); // protect the global index variable
        arrayIndex = upperIndex;
        upperIndex++;
        pthread_mutex_unlock(&upperIndexMutex);

        pthread_mutex_lock(&indexChangeMutex[arrayIndex]); // allows upper and replace operation at the same time for same index
        strcpy(line,lines[arrayIndex].line);
        convertStringToUpperCase(lines[arrayIndex].line); //convert string uppercase on array
        printf("Upper_%d\t\tUpper_%d read index %d and converted \"%s\" to \"%s\"\n", threadIndex, threadIndex, arrayIndex + 1,line,lines[arrayIndex].line); // print the display message
        pthread_mutex_unlock(&indexChangeMutex[arrayIndex]);
        sem_post(upperWriteSemaphore); //add 1 to upper Semaphore for done information
        if(arrayIndex >= (totalLine-1)-(upperThreadNum-1)){ //if the above threads currently are complates the all indexes break the loop
            break;
        }
    }

    pthread_exit(0);
}

void *runReplace(void *threadData) {
    int threadIndex, arrayIndex;
    lineData *lines;
    pthread_mutex_t *indexChangeMutex;
    //take inputs
    replaceThreadData * data = (replaceThreadData*) threadData;
    threadIndex = data->TheradIndex;
    lines = data->lines;
    indexChangeMutex = data->changeMutexes;
    char * line = malloc(sizeof(char)*MaxCharacter);


    while (1) {
        sem_wait(replaceSemaphore);//wait for element on array which is not replaced before
        pthread_mutex_lock(&replaceIndexMutex);
        arrayIndex = replaceIndex;
        replaceIndex++;
        pthread_mutex_unlock(&replaceIndexMutex);


        pthread_mutex_lock(&(indexChangeMutex[arrayIndex]));// allows upper and replace operation at the same time for same index
        strcpy(line,lines[arrayIndex].line);
        replaceBlanksWithUnderScore(lines[arrayIndex].line); //replace blanks with underscore
        printf("Replace_%d\tReplace_%d read index %d and converted \"%s\" to \"%s\"\n", threadIndex, threadIndex, arrayIndex + 1,line,lines[arrayIndex].line); // display an information message
        pthread_mutex_unlock(&indexChangeMutex[arrayIndex]);

        sem_post(replaceWriteSemaphore);//add 1 to replace Semaphore for done information
        if(arrayIndex >= (totalLine-1)-(replaceThreadNum-1)){ //if the above threads currently are complates the all indexes break the loop
            break;
        }
    }

    pthread_exit(0);
}

void *runWrite(void *threadData) {
    int threadIndex, arrayIndex,lineIndex;
    lineData *lines;
    int *lineBytes;

    writeThreadData *data = (writeThreadData *) threadData;
    threadIndex = data->TheradIndex;
    lines = data->lines;
    lineBytes = data->lineBytes;

    char *writedLine = (char *) malloc(sizeof(char) * MaxCharacter);

    while (1) {
        //wait for element on array which is both  uppered and replaced
        sem_wait(replaceWriteSemaphore);
        sem_wait(upperWriteSemaphore);
        pthread_mutex_lock(&writeReadMutex);//Lock mutex for o prevent threads from reaching the same element in the array
        arrayIndex = writeIndex;
        writeIndex++;
        strcpy(writedLine,lines[arrayIndex].line);//Copy the writedLine to spesific index of lines array
        lineIndex= lines[arrayIndex].lineIndex;
        writeFileSpecificIndex(writedLine,lineBytes[lineIndex]); //Write desired elements of the array to txt file(update txt file)
        printf("Writer_%d\tWriter_%d write line %d back which is \"%s\" \n", threadIndex, threadIndex, lineIndex + 1,writedLine);
        pthread_mutex_unlock(&writeReadMutex);//Unlock mutex

        if(arrayIndex >= (totalLine-1)-(writeThreadNum-1)){ //if the above threads currently are complates the all indexes break the loop
            break;
        }
   }

    pthread_exit(0);
}

void createReadThreads(pthread_t *readThreads, readThreadData *threadDataArray,lineData * lines,int lineBytes[]) { // create the read threads
    int rc;
    for (int i = 0; i < readThreadNum; ++i) {
        //fill data array with inputs
        threadDataArray[i].TheradIndex = i + 1;
        threadDataArray[i].lines = lines;
        threadDataArray[i].lineBytes = lineBytes;
        rc = pthread_create(&readThreads[i], NULL, &readFile, (void *) &threadDataArray[i]); //create the thread
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc); // if any error occured display an erro message and terminate program.
            exit(-1);
        }
    }
}

void createUpperThreads(pthread_t *upperThreads, upperThreadData *threadDataArray,lineData * lines,pthread_mutex_t *changeMutexes) { // create th Upper threads
    int rc;
    for (int i = 0; i < upperThreadNum; ++i) {
        //fill data array with inputs
        threadDataArray[i].TheradIndex = i+1;
        threadDataArray[i].lines = lines;
        threadDataArray[i].changeMutexes=changeMutexes;
        rc = pthread_create(&upperThreads[i], NULL, &runUpper, (void *) &threadDataArray[i]);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
}

void createReplaceThreads(pthread_t *replaceThreads, replaceThreadData *threadDataArray , lineData * lines,pthread_mutex_t  *changeMutexes) {
    int rc;
    for (int i = 0; i < replaceThreadNum; ++i) {
        threadDataArray[i].TheradIndex = i + 1;
        threadDataArray[i].lines = lines;
        threadDataArray[i].changeMutexes = changeMutexes;
        rc = pthread_create(&replaceThreads[i], NULL, &runReplace, (void *) &threadDataArray[i]);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
}

void createWriteThreads( pthread_t * writeThreads,writeThreadData * threadDataArray,lineData * lines,int lineBytes[]){
    int rc;
    for (int i = 0; i < writeThreadNum; ++i) {
        threadDataArray[i].TheradIndex = i + 1;
        threadDataArray[i].lines = lines;
        threadDataArray[i].lineBytes = lineBytes;
        rc = pthread_create(&writeThreads[i], NULL, &runWrite, (void *) &threadDataArray[i]);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
}
//join functions for every thread types
void joinReadThreads(pthread_t *readThreads) {
    for (int j = 0; j < readThreadNum; ++j) {
        pthread_join(readThreads[j], NULL);
    }
}

void joinUpperThreads(pthread_t *upperThreads) {
    for (int j = 0; j < upperThreadNum; ++j) {
        pthread_join(upperThreads[j], NULL);
    }
}

void joinReplaceThreads(pthread_t *replaceThreads) {
    for (int j = 0; j < replaceThreadNum; ++j) {
        pthread_join(replaceThreads[j], NULL);
    }
}

void joinWriteThreads( pthread_t * writeThreads){
    for (int j = 0; j < writeThreadNum; ++j) {
        pthread_join(writeThreads[j], NULL);
    }
}

void initializeMutexAndSemaphores(pthread_mutex_t * changeMutexes){
    //unlinks sempahores
    sem_unlink("/upperSemaphore");
    sem_unlink("/replaceSemaphore");
    sem_unlink("/upperWriteSemaphore");
    sem_unlink("/replaceWriteSemaphore");
    //initialize the semaphores
    replaceSemaphore = sem_open("/replaceSemaphore", O_CREAT | O_EXCL, 0644, 0);
    upperSemaphore = sem_open("/upperSemaphore", O_CREAT | O_EXCL, 0644, 0);
    upperWriteSemaphore = sem_open("/upperWriteSemaphore", O_CREAT | O_EXCL, 0644, 0);
    replaceWriteSemaphore = sem_open("/replaceWriteSemaphore", O_CREAT | O_EXCL, 0644, 0);

    //initialize the changeMutexes
    for (int j = 0; j <totalLine ; ++j) {
        pthread_mutex_init(&changeMutexes[j], NULL);
    }
}

int main(int argc, char *argv[]) {
     checkArguments(argc,argv); // check the given arguments

     totalLine=getTotalLineCount(); // get the total line of txt file
     int lineBytes[totalLine+1]; //line bytes is stores the starting byte of each line
     calculateLineBytes(lineBytes); //calculate the starting bytes of lines
     lineData lines[totalLine]; //create array for keep lines on txt
     pthread_mutex_t indexChangeMutex[totalLine] ; // create mutex for protect every index of array for simulataneously replace and upper operation

    initializeMutexAndSemaphores(indexChangeMutex); // initialize the semaphores and mutexes

    //create thread id array and data array for threads
    pthread_t readThreads[readThreadNum];
    readThreadData readThreadDataArray[readThreadNum];

    pthread_t upperThreads[upperThreadNum];
    upperThreadData upperThreadDataArray[upperThreadNum];

    pthread_t replaceThreads[replaceThreadNum];
    replaceThreadData replaceThreadDataArray[replaceThreadNum];

    pthread_t writeThreads[writeThreadNum];
    writeThreadData threadDataArray[writeThreadNum];

    //create the threads
    createReadThreads(readThreads, readThreadDataArray,lines,lineBytes);
    createUpperThreads(upperThreads, upperThreadDataArray,lines,indexChangeMutex);
    createReplaceThreads(replaceThreads, replaceThreadDataArray,lines,indexChangeMutex);
    createWriteThreads(writeThreads,threadDataArray,lines,lineBytes);

    //join the threads
    joinReadThreads(readThreads);
    joinUpperThreads(upperThreads);
    joinReplaceThreads(replaceThreads);
    joinWriteThreads(writeThreads);


    return 0;
}