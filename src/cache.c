#include "cache.h"
#include "trace.h"
#include <math.h>

int write_xactions = 0;
int read_xactions = 0;

char compulsory[12] = " compulsory\0";
char capacity[10] = " capacity\0";
char conflict[10] = " conflict\0";
char hit[5] = " hit\0";

struct cacheBlock {
    uint8_t validBit;
    uint8_t dirtyBit;
    uint32_t index;
    uint32_t tag;
    int priority;
};

struct historyAddress {
    uint32_t address;
    struct historyAddress *previousHisotoryAddr;
};

uint32_t getIndex(uint32_t address, uint8_t offsetSize, uint8_t indexSize) {
    uint32_t index = 0;
    index = (address >> offsetSize) & (~(0xffffffff << indexSize));
    return index;
}

uint32_t getTag(uint32_t address, uint32_t offsetSize, uint8_t indexSize , uint8_t tagSize) {
    uint32_t tag = 0;
    tag = (address >> (offsetSize + indexSize)) & (~(0xffffffff << tagSize));
    return tag;
}

/* check if this address access is a compulsory one */
int checkCompulsory(uint32_t address, struct historyAddress *currentHistoryAddr, int start, uint8_t offsetSize) {
    uint32_t historyAddress;
    for (int i = start; i >= 0; i--) {
        historyAddress = currentHistoryAddr->address;
        if ((address >> offsetSize) == (historyAddress >> offsetSize)) {
            return -1;
        }
        currentHistoryAddr = currentHistoryAddr->previousHisotoryAddr;
    }

    return 1;
}


/* used in LRU policy. 
 * this function check the existed maxium priority of all blocks,
 * and return maxPriority + 1 to the newly accessed one */
int getBlockPriority(struct cacheBlock set[], uint32_t ways) {
    int maxPriority = set[0].priority;
    for (int i = 1; i < ways; i++) {
        if (set[i].priority > maxPriority) {
            maxPriority = set[i].priority;
        }
    }

    return (maxPriority + 1);
}

/* used in LRU policy.
 * this function finds the block with the minimum priority and return 
 * its location */
int replaceLocationWithLRU(struct cacheBlock set[], uint32_t ways) {
    int replaceLocation = 0;
    int minPriority = set[0].priority;
    for (int i = 1; i < ways; i++) {
        if (set[i].priority < minPriority) {
            replaceLocation = i;
            minPriority = set[i].priority;
        }
    }

    return replaceLocation;
}

/* this function check whether the address accessed is in 
 * the imagined fully-associative cache */
int foundInFullyCache(struct cacheBlock fullyCaches[], uint32_t tag, int validBlockNumber, int lru, uint32_t ways) {
    for (int i = 0; i < validBlockNumber; i++) {
        if (fullyCaches[i].validBit == 1 && tag == fullyCaches[i].tag) {
            if (lru == 1) {
                fullyCaches[i].priority = getBlockPriority(fullyCaches, ways);
            }
            return 1;  // found
        }
    }

    return 0;
}

/* update the imagined fully-associative cache */
void updateFullyCache(struct cacheBlock fullyCaches[], uint32_t tag, int *replace, int *validBlockNumber, int totallyBlockNum, int lru) {
    if (foundInFullyCache(fullyCaches, tag, totallyBlockNum, lru, totallyBlockNum) == 1) {
        return;
    }

    int lruReplace;

    if (*validBlockNumber < totallyBlockNum) {
        fullyCaches[*validBlockNumber].validBit = 1;
        fullyCaches[*validBlockNumber].tag = tag;
        if (lru == 1) {
            fullyCaches[*validBlockNumber].priority = getBlockPriority(fullyCaches, totallyBlockNum);
        }
        *validBlockNumber += 1;
    } else {
        /* the fully-associative cache is full */
        /* printf("validBlockNumber %d. replace location: %d\n", *validBlockNumber, *replace); */
        if (lru == 1) {
            lruReplace = replaceLocationWithLRU(fullyCaches, totallyBlockNum);
            fullyCaches[lruReplace].tag = tag;
            fullyCaches[lruReplace].priority = getBlockPriority(fullyCaches, totallyBlockNum);
        } else {

            fullyCaches[*replace].tag = tag;
            *replace = (*replace + 1) % (totallyBlockNum);
        }
    }
}



/*
  Print help message to user
*/
void printHelp(const char * prog) {
	printf("%s [-h] | [-s <size>] [-w <ways>] [-l <line>] [-t <trace>]\n", prog);
	printf("options:\n");
	printf("-h: print out help text\n");
	printf("-s <cache size>: set the total size of the cache in KB\n");
	printf("-w <ways>: set the number of ways in each set\n");
	printf("-l <line size>: set the size of each cache line in bytes\n");
	printf("-t <trace>: use <trace> as the input file for memory traces\n");
  /* EXTRA CREDIT
  printf("-lru: use LRU replacement policy instead of FIFO\n");
  */
}
/*
	Main function. Feed to options to set the cache
	
	Options:
	-h : print out help message
	-s : set L1 cache Size (B)
	-w : set L1 cache ways
	-l : set L1 cache line size
*/

int main(int argc, char* argv[])
{
	int i;
	uint32_t size = 32; //total size of L1$ (KB)
	uint32_t ways = 1; //# of ways in L1. Default to direct-mapped
	uint32_t line = 32; //line size (B)
  uint32_t sets = -1; //number of sets
  int lru = 0;
    //offset bit
    int offsetBits = 0;
    //index bit
    int indexBits = 0;
    //tag bit
    int tagBits = 0;

  // hit and miss counts
  int totalHits = 0;
  int totalMisses = 0;

  char * filename;
//    char * outputptr;
    //strings to compare
	const char helpString[] = "-h";
	const char sizeString[] = "-s";
	const char waysString[] = "-w";
	const char lineString[] = "-l";
	const char traceString[] = "-t";
    const char lruString[] = "-lru";
	
  if (argc == 1) {
    // No arguments passed, show help
    printHelp(argv[0]);
    return 1;
  }
  
	//parse command line
	for(i = 1; i < argc; i++)
	{
		//check for help
		if(!strcmp(helpString, argv[i]))
		{
			//print out help text and terminate
      printHelp(argv[0]);
            
			return 1; //return 1 for help termination
		}
		//check for size
		else if(!strcmp(sizeString, argv[i]))
		{
			//take next string and convert to int
			i++; //increment i so that it skips data string in the next loop iteration
			//check next string's first char. If not digit, fail
			if(isdigit(argv[i][0]))
			{
				size = atoi(argv[i]);
			}
			else
			{
				printf("Incorrect formatting of size value\n");
				return -1; //input failure
			}
		}
		//check for ways
		else if(!strcmp(waysString, argv[i]))
		{
			//take next string and convert to int
			i++; //increment i so that it skips data string in the next loop iteration
			//check next string's first char. If not digit, fail
			if(isdigit(argv[i][0]))
			{
				ways = atoi(argv[i]);
			}
			else
			{
				printf("Incorrect formatting of ways value\n");
				return -1; //input failure
			}
		}
		//check for line size
		else if(!strcmp(lineString, argv[i]))
		{
			//take next string and convert to int
			i++; //increment i so that it skips data string in the next loop iteration
			//check next string's first char. If not digit, fail
			if(isdigit(argv[i][0]))
			{
				line = atoi(argv[i]);
			}
			else
			{
				printf("Incorrect formatting of line size value\n");
				return -1; //input failure
			}
		}
    else if (!strcmp(traceString, argv[i])) {
      filename = argv[++i];
    }
    else if (!strcmp(lruString, argv[i])) {
      // Extra Credit: Implement Me!
			/* printf("Unrecognized argument. Exiting.\n"); */
        lru = 1;
			/* return -1; */
    }
		//unrecognized input
		else{
			printf("Unrecognized argument. Exiting.\n");
			return -1;
		}
	}
	
  /* TODO: Probably should intitalize the cache */
    
    // sets = total size/ (associativiy * line size);
    sets = size * 1024 / (line * ways);
    // offset = log2(line size)
    offsetBits = log2(line);
    // index = log2(sets)
    indexBits = log2(sets);
    // tag = 32-offset-index
    tagBits = 32 - offsetBits - indexBits;

   //initialize entire caches in 2D array
  struct cacheBlock caches[sets][ways];
  for (i = 0; i < sets; i++) {
      for (int j = 0; j < ways; j++) {
          caches[i][j].validBit = 0;
          caches[i][j].dirtyBit = 0;
        }
    }


	/* TODO: Now we read the trace file line by line */
    FILE *fp;
    // each line has <14 chars
    char lineInput[24];
    int lineCounter = 0;
    // open file from argv[2]
    fp = fopen(filename,"r");
    // While is not end of file
    while(fgets(lineInput,sizeof lineInput,fp)!=NULL){
            lineCounter ++;
    }
   // printf("total number of lines are %d\n", lineCounter);
    
    char *string[lineCounter];
    //printf("breakpoint0\n");
    
    // set pointer to the beginnig of the file
    fseek(fp,0,SEEK_SET);
    
    // now transfer files to input Array
    for(i=0;i<lineCounter;i++){
        fgets(lineInput,sizeof(lineInput),fp);
        /* there is a '\n' at the end of every lineInput */
        string[i]=malloc(sizeof(lineInput));
        strcpy(string[i],lineInput);
        string[i][12] = '\0';
    }
    fclose(fp);
//    outputptr = strtok(filename,"/");
//    outputptr = strtok(NULL, "/");
    char outputparameters[40];
    strcpy(outputparameters,filename);
    strcat(outputparameters,".simulated");
    //printf("%s",outputparameters);
    fp = fopen(outputparameters,"w");
    
 /* Convert String into instruction array and 32bit address array */
    char loadstore[lineCounter];
    int replaceLocation[sets];
    for (i = 0; i < sets; i++) {
        replaceLocation[i] = 0;
    }

    uint32_t index = 0;
    uint32_t tag = 0;
    uint32_t address;
    struct historyAddress *previousHisotoryAddr = NULL;
    struct historyAddress *newHistoryAddr = NULL;
    
    /* inialize entire cache if it was fully associative*/
    int fullyCaches_size = sets * ways;
    struct cacheBlock fullyCaches[fullyCaches_size];
    for(i = 0; i < sets * ways ; i++){
        fullyCaches[i].validBit = 0;
        fullyCaches[i].dirtyBit = 0;
    }
    int replaceFully = 0;  //indicate which block to be replaced in a fully-assoc cache
    int validBlockNum = 0;  // how many blocks in the fully-cache have been used

    int lruReplace = 0; // record which block to be replaced in LRU mode
    
    char tempString[11];
    /* printf("breakpoint1\n"); */
    
    
    /* start to analyze each address */
    /* Huge For Loop Alert*/
    /* varirable address is each physical address needs to be analyzed, loadstore[] is instruction type*/
    /* tag is actuay tag in each address, fully tag is pesudo-tag by assuming the offset bit is 0*/
    
    for(i = 0; i<lineCounter; i++){
        strncpy(tempString, string[i]+2,10);
        address = strtol(tempString, NULL, 0);
        loadstore[i] = string[i][0];
        
        newHistoryAddr = malloc(sizeof(struct historyAddress));
        newHistoryAddr->address = address;
        newHistoryAddr->previousHisotoryAddr = previousHisotoryAddr;
        index = getIndex(address, offsetBits, indexBits);
        tag = getTag(address, offsetBits, indexBits, tagBits);

        int fullyindexBit = 0;
        int fulltagBit = 32-fullyindexBit-offsetBits;
        uint32_t fullytag = getTag(address, offsetBits, fullyindexBit, fulltagBit);
        
        //********* Real Logic Starts Here *************
        
        /* first, check if this address is an unseen one */
        if (checkCompulsory(address, previousHisotoryAddr, i - 1, offsetBits) == 1) {
            // it is a compulsory miss;
            strcat(string[i], compulsory);
//            printf("%s\n", string[i]);
            // put it in the cache
            int k;
            for (k = 0; k < ways; k++) {
                if (caches[index][k].validBit == 0) {
                    caches[index][k].index = index;
                    caches[index][k].tag = tag;
                    caches[index][k].validBit = 1;
                    // if it's miss and set is not full
                    // just assign dirtyBit to its value due to different instruction
                    if(loadstore[i] == 's'){
                        caches[index][k].dirtyBit = 1;
                    }
                    if(loadstore[i] == 'l'){
                        caches[index][k].dirtyBit = 0;
                    }

                    if (lru == 1) {
                        caches[index][k].priority = getBlockPriority(caches[index], ways);
                    }
                    break;
                }
            }
            // still compulsory miss, but the set is full, call replaceLocation() to update the cache
                if (k == ways) {
                    // if it's miss, and previous dirty bit is 1, count the write back
                    // ONLY difference here is in load instruction, set the dirtyBit to 0
                    if (lru == 1) {
                        replaceLocation[index] = replaceLocationWithLRU(caches[index], ways);
                    }

                    if(loadstore[i] == 's'){
                        if(caches[index][replaceLocation[index]].dirtyBit == 1){
                            write_xactions ++;
                        }

                        caches[index][replaceLocation[index]].dirtyBit = 1;
                    }
                    if(loadstore[i] == 'l'){
                        if(caches[index][replaceLocation[index]].dirtyBit == 1){
                            write_xactions ++;
                        }
                        caches[index][replaceLocation[index]].dirtyBit = 0;
                        
                    }

                if (lru == 1) {
                    lruReplace = replaceLocationWithLRU(caches[index], ways);
                    caches[index][lruReplace].priority = getBlockPriority(caches[index], ways);
                    caches[index][lruReplace].index = index;
                    caches[index][lruReplace].tag = tag;
                } else {
                    caches[index][replaceLocation[index]].index = index;
                    caches[index][replaceLocation[index]].tag = tag;
                    replaceLocation[index] = (replaceLocation[index] + 1) % ways;
                }
                    
            }
            totalMisses++;
        }
        // not a compulsory miss: three conditions: hit/capacity/conflict
        else {
            // first case: hit
            int k;
            for (k = 0; k < ways; k++) {
                if (tag == caches[index][k].tag) {
                    strcat(string[i], hit);
//                    printf("%s\n", string[i]);
                    totalHits++;
                    // if hit, and the instruction is store, change dirty bit to 1, but load does not change anything since it hits!
                    if(loadstore[i] == 's'){
                        caches[index][k].dirtyBit = 1;
                    }
 
                    if (lru == 1) {
                        caches[index][k].priority = getBlockPriority(caches[index], ways);
                    }
                    break;
                }
            }

            if (k == ways) {
                // miss
                // if it's miss, and previous dirty bit is 1, count the write back
                // ONLY difference here is in load instruction, set the dirtyBit to 0

                if (lru == 1) {
                    replaceLocation[index] = replaceLocationWithLRU(caches[index], ways);
                }

                if(loadstore[i] == 's'){
                    if(caches[index][replaceLocation[index]].dirtyBit == 1){
                        write_xactions ++;
                    }
                    caches[index][replaceLocation[index]].dirtyBit = 1;
                }
                if(loadstore[i] == 'l'){
                    if(caches[index][replaceLocation[index]].dirtyBit == 1){
                        write_xactions ++;
                    }
                    caches[index][replaceLocation[index]].dirtyBit = 0;
                    
                }

              if (lru == 1) {
                  lruReplace = replaceLocationWithLRU(caches[index], ways);
                  caches[index][lruReplace].index = index;
                  caches[index][lruReplace].tag = tag;
                  caches[index][lruReplace].priority = getBlockPriority(caches[index], ways);
              } else {
                  caches[index][replaceLocation[index]].index = index;
                  caches[index][replaceLocation[index]].tag = tag;
                  replaceLocation[index] = (replaceLocation[index] + 1) % ways;
              }

                // check if the address is in the imagined fully-assoc cache, if yes, conflict miss
                if (foundInFullyCache(fullyCaches, fullytag, validBlockNum, lru, ways) == 1) {
                    strcat(string[i], conflict);
//                    printf("%s\n", string[i]);
                }
                // capacity miss
                else{
                    strcat(string[i], capacity);
//                    printf("%s\n", string[i]);
                   
                }
                                totalMisses++;
            }
            
        }
        
        updateFullyCache(fullyCaches, fullytag, &replaceFully, &validBlockNum, fullyCaches_size, lru);
        /* start to build fully associate caches*/
        
        
        // at end of analyzing each address, make newHistroyAdreess to previous histroy adress
        previousHisotoryAddr = newHistoryAddr;
        fprintf(fp, "%s\n", string[i]);
    }
    // end of for loop

    /* TODO: Now we output the file */
    fclose(fp);
   /* Print results */
    printf("Ways: %u; Sets: %u; Line Size: %uB\n",ways, sets, line);
    printf("Tag: %d bits; Index: %d bits; Offset: %d bits\n", tagBits, indexBits, offsetBits);
    printf("Miss Rate: %8lf%%\n", ((double) totalMisses) / ((double) totalMisses + (double) totalHits) * 100.0);
    printf("Read Transactions: %d\n", totalMisses);
    printf("Write Transactions: %d\n", write_xactions);


  /* TODO: Cleanup */
 
    previousHisotoryAddr = newHistoryAddr->previousHisotoryAddr;
    for (i = 0; i < lineCounter; i++) {
        // free the cache
        free(string[i]);

        // free the linked list that holds all the accessed addresses
        free(newHistoryAddr);
        newHistoryAddr = previousHisotoryAddr;
        previousHisotoryAddr = previousHisotoryAddr->previousHisotoryAddr;
    }
}





