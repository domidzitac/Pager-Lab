#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define MAXINT 2147483647

int M; // the machine size in words
int P; // the page size in words
int S; // the size of each process (the references are to virtual addressses 0..S-1
int J; // the "Job mix"
int N; // the number of references for each process;
char R[10]; // the replacement algorithm (LIFO, RANDOM, LRU)

int f; // final argument 0 -, 11 - show random, 1 - debugging
int D = 0; // number of processes
const int q = 3; // Round Robin Quantum 
char* replacementAlgs[] = { "lru", "random", "lifo" };


char randomFile[] = "random-numbers.txt";
FILE* rndFile;
enum ReplacementAlgos
{
	lru,
	rnd,
	lifo
};
enum ReplacementAlgos replacementAlg;
enum ProcessState
{
	Ready,
	Running,
	Finished
};

struct FrameTableEntry
{
	int process;
	int page;
	int loadingTime; // time that the page is loaded in the frame table
	int accessTime; // tiem that the page is last accessed (loadingTime <= accessTime)
};

struct Process
{
	int size;
	double A, B, C;
	int N; // number of references
	int pageFaults;
	int evictions;
	int residencyTime;
	enum ProcessState state;
	int currentWord;
	int nextRef;
};


struct FrameTableEntry** frameTable;
struct Process** procs;
int frameTableSize;

void processCmdLineArgs(int argc, char* argv[])
{
	if (argc < 7 || argc > 8)
	{
		printf("Usage: Pager M P S J N R [debugging level]");
		exit(1);
	}
	M = atoi(argv[1]);
	P = atoi(argv[2]);
	S = atoi(argv[3]);
	J = atoi(argv[4]);
	N = atoi(argv[5]);
	strcpy(R, argv[6]);
		
	if (strcmp(R, "random") == 0)
		replacementAlg = rnd;
	else if (strcmp(R, "lru") == 0)
		replacementAlg = lru;
	else if (strcmp(R, "lifo") == 0)
		replacementAlg = lifo;
	
	if (argc == 8)
		f = atoi(argv[7]);
	else
		f = 0; 


	switch (J)
	{
	case 1:
		D = 1; break;
	case 2:
		D = 4; break;
	case 3:
		D = 4; break;
	case 4:
		D = 4; break;
	default:
		printf("Invalid J (1-4)");
		exit(1); break;
	}
}

void printInput()
{
	printf("The machine size is %d.\n", M);
	printf("The page size is %d.\n", P);
	printf("The process size is %d.\n", S);
	printf("The job mix number is %d.\n", J);
	printf("The number of references per process is %d.\n", N);
	printf("The replacement algorithm is %s.\n", R);
	printf("The level of debugging output is %d\n\n", f);
}


void createDataStructures()
{
	frameTableSize = (M / P);
	frameTable = (struct FrameTableEntry**)malloc(sizeof(struct FrameTableEntry*) * frameTableSize);
	if (frameTable == NULL)
	{
		printf("Not enough memory.");
		exit(1);
	}
	for (int i = 0; i < frameTableSize; i++)
		frameTable[i] = NULL;


	procs = (struct Process**)malloc(sizeof(struct Process*) * D);
	for (int i = 0; i < D; i++)
	{
		if(procs != NULL)
			*(procs+i) = (struct Process*)malloc(sizeof(struct Process));
		else
		{
			printf("NULL pointer");
			return;
		}
		if (procs != NULL && procs[i] != NULL)
		{
			procs[i]->N = N;
			procs[i]->size = S;
			procs[i]->pageFaults = 0;
			procs[i]->evictions = 0;
			procs[i]->residencyTime = 0;
			procs[i]->state = Ready;
			procs[i]->currentWord = -1;
		}
		else
		{
			printf("NULL pointer");
			return;
		}
		
		switch (J)
		{
		case 1:
			procs[i]->A = 1;
			procs[i]->B = 0;
			procs[i]->C = 0;
			break;
		case 2:
			procs[i]->A = 1;
			procs[i]->B = 0;
			procs[i]->C = 0;
			break;
		case 3: // fully random
			procs[i]->A = 0;
			procs[i]->B = 0;
			procs[i]->C = 0;
			break;
		case 4:
			switch (i)
			{
			case 0:
				procs[i]->A = 0.75;
				procs[i]->B = 0.25;
				procs[i]->C = 0;
				break;
			case 1:
				procs[i]->A = 0.75;
				procs[i]->B = 0;
				procs[i]->C = 0.25;
				break;
			case 2:
				procs[i]->A = 0.75;
				procs[i]->B = 0.125;
				procs[i]->C = 0.125;
				break;
			case 3:
				procs[i]->A = 0.5;
				procs[i]->B = 0.125;
				procs[i]->C = 0.125;
				break;
			}
			break;
		}
		

	}
		

}
int AllProcsFinished()
{
	for (int i = 0; i < D; i++)
		if (procs[i]->N > 0)
			return 0;
	return 1;
}

void AddPage2FT(int page, int currentProcess, int word, int time)
{
	// find highest free frame
	int frame; // no of frame that will be used for page
	int i, ret; 
	int maxLifo; // frame that was last in (for LIFO replacement alg)
	int minLRU; // last recently used (for LRU replacement alg);
	int randomFrame; // frame that is evicted (for random replaclment alg)
	int evictedProcess;
	for(frame = frameTableSize - 1; frame >= 0; frame--)
		if (frameTable[frame] == NULL)
		{
			break;
		}


	// if found add the page to that frame
	if (frame >= 0)
	{
		frameTable[frame] = (struct FrameTableEntry*)malloc(sizeof(struct FrameTableEntry));
		frameTable[frame]->page = page;
		frameTable[frame]->process = currentProcess;
		frameTable[frame]->loadingTime = time;
		frameTable[frame]->accessTime = time;
		if (f == 1 || f == 11)
			printf("%d references word %d (page %d) at time %d: Fault, using free frame %d.\n", currentProcess + 1, word, page, time, frame);
	}
	// otherwise evict one page and add the page to the evicted frame;
	else
	{
		// find frame that will be evicted based on the replacement algorithm
		switch (replacementAlg)
		{
		case rnd:
			ret = fscanf(rndFile, "%d", &randomFrame);
			if (f == 11)
				printf("%d uses random number: %d\n", currentProcess + 1, randomFrame);
			frame = randomFrame % frameTableSize;
			break;
		case lru:
			frame = 0;
			minLRU = frameTable[frame]->accessTime;
			for(i = 1; i < frameTableSize; i++)
				if (frameTable[i]->accessTime <= minLRU)
				{
					minLRU = frameTable[i]->accessTime;
					frame = i;
				}
			break;
		case lifo:
			frame = 0;
			maxLifo = frameTable[frame]->loadingTime;
			for(i = 1; i < frameTableSize; i++)
				if (frameTable[i]->loadingTime > maxLifo)
				{
					maxLifo = frameTable[i]->loadingTime;
					frame = i;
				}
			break;
		default:
			printf("Replacement algorithm not set correctly. Exiting...");
			exit(1);
			break;
		}

		//print debug message
		if (f == 1 || f == 11)
			printf("%d references word %d (page %d) at time %d: Fault, evicting page %d of %d from frame %d.\n", 
				currentProcess + 1, word, page, time, frameTable[frame]->page, frameTable[frame]->process + 1, frame);

		// evict "frame"
		evictedProcess = frameTable[frame]->process;

		procs[evictedProcess]->evictions++;
		procs[evictedProcess]->residencyTime += time - frameTable[frame]->loadingTime;

		free(frameTable[frame]); // free memory of evicted frame

		// load page into "frame"
		frameTable[frame] = (struct FrameTableEntry*)malloc(sizeof(struct FrameTableEntry));
		if (frameTable[frame] != NULL)
		{
			frameTable[frame]->page = page;
			frameTable[frame]->process = currentProcess;
			frameTable[frame]->loadingTime = time;
			frameTable[frame]->accessTime = time;
		}
		else
		{
			printf("Not enough memory. Exiting...");
			exit(1);
		}
		
	}


	
}
int CalculateNextReference(int proc)
{
	int r;
	int ret;
	int nextRef;
	double y;
	ret = fscanf(rndFile, "%d", &r);

	if(f == 11)
		printf("%d uses random number: %d\n", proc + 1, r);

	y = r / (MAXINT + 1.0);
	if (y < procs[proc]->A)
		nextRef = (procs[proc]->currentWord + 1) % S;
	else if(y < procs[proc]->A + procs[proc]->B)
		nextRef = (procs[proc]->currentWord - 5 + S) % S;
	else if(y < procs[proc]->A + procs[proc]->B + procs[proc]->C)
		nextRef = (procs[proc]->currentWord +4) % S;
	else
	{
		// random value in 0..S-1
		ret = fscanf(rndFile, "%d", &r);
		if (f == 11)
			printf("%d uses random number: %d\n", proc + 1, r);
		
		nextRef = r % S;
	}
	return nextRef;
}

int PageInFT(int page, int currentProcess) // returns frame number or -1 if page not found in Frame Table
{
	int i;
	for (i = 0; i < frameTableSize; i++)
	{
		if (frameTable[i] != NULL)
			if (frameTable[i]->page == page && frameTable[i]->process == currentProcess)
				break;
	}
	
	return (i == frameTableSize) ? -1: i;
}
void simulateDemandPaging()
{
	int time = 1;
	int currentProcess = 0;
	int currentQuantum = q;
	int word, page, frame;

	while (!AllProcsFinished())
	{
		if (procs[currentProcess]->N == 0)
			procs[currentProcess]->state = Finished;
		if (procs[currentProcess]->state != Finished && currentQuantum > 0)
		{
			if (procs[currentProcess]->state == Ready)
			{
				// initiate process
				word = 111 * (currentProcess + 1) % S; // get initial word
				page = word / P; // compute page number of the word in the current process;
				AddPage2FT(page, currentProcess, word, time);
				procs[currentProcess]->state = Running;
				procs[currentProcess]->pageFaults++;
				procs[currentProcess]->N--; // 
				procs[currentProcess]->currentWord = word;
				procs[currentProcess]->nextRef = CalculateNextReference(currentProcess);
				currentQuantum--; time++;
			}
			else
			{
				// process already running
				page = procs[currentProcess]->nextRef / P; 
				frame = PageInFT(page, currentProcess); // returns frame number or -1 if page not found in Frame Table
				if (frame >= 0)
				{
					// hit in frame
					if (f == 1 || f == 11)
						printf("%d references word %d (page %d) at time %d: Hit in frame %d\n", 
							currentProcess + 1, procs[currentProcess]->nextRef, page, time, frame);

					frameTable[frame]->accessTime = time;
					procs[currentProcess]->N--; // 
					procs[currentProcess]->currentWord = procs[currentProcess]->nextRef;
					procs[currentProcess]->nextRef = CalculateNextReference(currentProcess);
					currentQuantum--; time++;
				}
				else
				{
					// page fault

					AddPage2FT(page, currentProcess, procs[currentProcess]->nextRef, time);
					procs[currentProcess]->pageFaults++;
					procs[currentProcess]->N--; // 
					procs[currentProcess]->currentWord = procs[currentProcess]->nextRef;
					procs[currentProcess]->nextRef = CalculateNextReference(currentProcess);
					currentQuantum--; time++;
				}
			}
		}
		else
		{
			currentProcess = (currentProcess + 1) % D;
			currentQuantum = q;
		}
	}
}
void openRandomNumberFile()
{
	rndFile = fopen(randomFile, "r");
	if (rndFile == NULL)
	{
		printf("Could not open %s. Exiting...", randomFile);
		exit(1);
	}
}

// trim trailing zeros from a string; 5.123000 -> 5.123 and 6.000 -> 6.0;
void trimZeros(char d[])
{
	int n;
	n = strlen(d);
	while (n >= 1 && d[n - 1] == '0')
	{
		n--;
		d[n] = '\0';
	}
	if (d[n - 1] == '.')
	{
		d[n] = '0';
		d[n + 1] = '\0';
	}
}
void printSummary()
{
	int totalEvictions = 0, totalFaults = 0, totalResidency = 0;
	char d[30];
	if (f == 1 || f == 11)
		printf("\n");
	for (int i = 0; i < D; i++)
	{
		printf("Process %d had %d faults", i + 1, procs[i]->pageFaults);
		
		if (procs[i]->evictions > 0)
		{
			sprintf(d, "%.15lf", (double)procs[i]->residencyTime / procs[i]->evictions);
			trimZeros(d);
			printf(" and %s average residency.\n", d);
		}
			
		else
		{
			printf(".\n");
			printf("     With no evictions, the average residence is undefined.\n");
		}
		totalEvictions += procs[i]->evictions;
		totalFaults += procs[i]->pageFaults;
		totalResidency += procs[i]->residencyTime;
	}
	printf("\nThe total number of faults is %d", totalFaults);
	if (totalEvictions > 0)
	{
		sprintf(d, "%.15lf", (double)totalResidency / totalEvictions);
		trimZeros(d);
		printf(" and the overall average residency is %s.\n", d);
	}
		
	else
	{
		printf(".\n");
		printf("     With no evictions, the overall average residence is undefined.\n");
	}
		
}
int main(int argc, char* argv[])
{
	processCmdLineArgs(argc, argv);

	openRandomNumberFile();
	
	printInput();


	createDataStructures();

	simulateDemandPaging();

	printSummary();
	
	return 0;
}