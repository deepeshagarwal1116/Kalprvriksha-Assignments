#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_POOL_SIZE 1
#define MAX_POOL_SIZE 1000000
#define INITIAL_CAPACITY 8
#define NO_ID 0

typedef enum { BLOCK_FREE, BLOCK_ALLOCATED } BlockStatus;
typedef enum { FIRST_FIT = 1, BEST_FIT, WORST_FIT } Strategy;
typedef enum { MENU_ALLOCATE = 1, MENU_RELEASE, MENU_RESIZE, MENU_MAP, MENU_STATS, MENU_STRATEGY, MENU_EXIT } MenuChoice;
typedef enum { RESULT_OK, RESULT_INVALID_SIZE, RESULT_UNKNOWN_ID, RESULT_ALREADY_FREE, RESULT_NO_SPACE, RESULT_NO_MEMORY } Result;

typedef struct {
    int start;
    int size;
    int id;
    BlockStatus status;
} Block;

typedef struct {
    unsigned char *pool;
    int poolSize;
    Block *blocks;
    int count;
    int capacity;
    int nextId;
    Strategy strategy;
    int lastFragmentedRequest;
} MemoryManager;

typedef struct {
    int allocatedBytes;
    int freeBytes;
    int allocatedBlocks;
    int freeBlocks;
    int largestFreeBlock;
} Statistics;

static const char *STRATEGY_NAMES[] = { "", "First Fit", "Best Fit", "Worst Fit" };

void clearInputLine(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

// reads a whole-line integer, re-prompting on bad input; returns 0 on success, -1 on EOF
int readInt(const char *prompt, int *value)
{
    while (1) {
        printf("%s", prompt);

        int result = scanf("%d", value);
        if (result == EOF)
            return -1;

        int next = getchar();
        int isWholeLine = (next == '\n' || next == EOF);
        if (!isWholeLine)
            clearInputLine();

        if (result == 1 && isWholeLine)
            return 0;
        printf("Invalid input! Please enter a whole number.\n");
    }
}

int readIntInRange(const char *prompt, int min, int max, int *value)
{
    while (1) {
        if (readInt(prompt, value) != 0)
            return -1;
        if (*value >= min && *value <= max)
            return 0;
        printf("Please enter a value between %d and %d.\n", min, max);
    }
}

int readStrategy(Strategy *strategy)
{
    int choice;
    if (readIntInRange("Select allocation strategy (1. First Fit, 2. Best Fit, 3. Worst Fit): ",
                       FIRST_FIT, WORST_FIT, &choice) != 0)
        return -1;

    *strategy = (Strategy)choice;
    printf("Strategy set to %s.\n", STRATEGY_NAMES[choice]);
    return 0;
}

// grows the metadata array with realloc so it can hold `needed` blocks
Result ensureCapacity(MemoryManager *manager, int needed)
{
    if (needed <= manager->capacity)
        return RESULT_OK;

    int newCapacity = manager->capacity * 2;
    Block *grown = realloc(manager->blocks, newCapacity * sizeof(Block));
    if (grown == NULL)
        return RESULT_NO_MEMORY;

    manager->blocks = grown;
    manager->capacity = newCapacity;
    return RESULT_OK;
}

// caller must have called ensureCapacity first
void insertBlockAt(MemoryManager *manager, int index, Block block)
{
    memmove(&manager->blocks[index + 1], &manager->blocks[index], (manager->count - index) * sizeof(Block));
    manager->blocks[index] = block;
    manager->count++;
}

void removeBlockAt(MemoryManager *manager, int index)
{
    memmove(&manager->blocks[index], &manager->blocks[index + 1], (manager->count - index - 1) * sizeof(Block));
    manager->count--;
}

Result findAllocation(const MemoryManager *manager, int id, int *index)
{
    if (id <= 0 || id >= manager->nextId)
        return RESULT_UNKNOWN_ID;

    for (int i = 0; i < manager->count; i++) {
        if (manager->blocks[i].status == BLOCK_ALLOCATED && manager->blocks[i].id == id) {
            *index = i;
            return RESULT_OK;
        }
    }
    return RESULT_ALREADY_FREE;
}

// shrinks block `index` to `size` and puts the leftover in a new free block after it;
// returns 1 if a block was inserted
int splitBlock(MemoryManager *manager, int index, int size)
{
    Block *block = &manager->blocks[index];
    if (block->size == size)
        return 0;

    Block remainder = { block->start + size, block->size - size, NO_ID, BLOCK_FREE };
    block->size = size;
    insertBlockAt(manager, index + 1, remainder);
    return 1;
}

void mergeFreeBlocks(MemoryManager *manager)
{
    int i = 0;
    while (i < manager->count - 1) {
        if (manager->blocks[i].status == BLOCK_FREE && manager->blocks[i + 1].status == BLOCK_FREE) {
            manager->blocks[i].size += manager->blocks[i + 1].size;
            removeBlockAt(manager, i + 1);
        } else {
            i++;
        }
    }
}

int findFirstFit(const MemoryManager *manager, int size)
{
    for (int i = 0; i < manager->count; i++)
        if (manager->blocks[i].status == BLOCK_FREE && manager->blocks[i].size >= size)
            return i;
    return -1;
}

// smallest (best fit) or largest (worst fit) free block that can hold `size`
int findSizedFit(const MemoryManager *manager, int size, int wantLargest)
{
    int chosen = -1;
    for (int i = 0; i < manager->count; i++) {
        const Block *block = &manager->blocks[i];
        if (block->status != BLOCK_FREE || block->size < size)
            continue;

        if (chosen == -1)
            chosen = i;
        else if (wantLargest ? block->size > manager->blocks[chosen].size
                             : block->size < manager->blocks[chosen].size)
            chosen = i;
    }
    return chosen;
}

int findBestFit(const MemoryManager *manager, int size)
{
    return findSizedFit(manager, size, 0);
}

int findWorstFit(const MemoryManager *manager, int size)
{
    return findSizedFit(manager, size, 1);
}

int findFreeBlock(const MemoryManager *manager, int size)
{
    if (manager->strategy == BEST_FIT)
        return findBestFit(manager, size);
    if (manager->strategy == WORST_FIT)
        return findWorstFit(manager, size);
    return findFirstFit(manager, size);
}

Result allocateMemory(MemoryManager *manager, int size, int *index)
{
    if (size <= 0 || size > manager->poolSize)
        return RESULT_INVALID_SIZE;

    *index = findFreeBlock(manager, size);
    if (*index < 0)
        return RESULT_NO_SPACE;
    if (ensureCapacity(manager, manager->count + 1) != RESULT_OK)
        return RESULT_NO_MEMORY;

    splitBlock(manager, *index, size);
    Block *block = &manager->blocks[*index];
    block->status = BLOCK_ALLOCATED;
    block->id = manager->nextId++;
    memset(manager->pool + block->start, 0, size);
    return RESULT_OK;
}

Result releaseMemory(MemoryManager *manager, int id)
{
    int index;
    Result found = findAllocation(manager, id, &index);
    if (found != RESULT_OK)
        return found;

    manager->blocks[index].status = BLOCK_FREE;
    manager->blocks[index].id = NO_ID;
    mergeFreeBlocks(manager);
    return RESULT_OK;
}

// copies the data into another free block and frees the old one
Result moveAllocation(MemoryManager *manager, int index, int newSize)
{
    int target = findFreeBlock(manager, newSize);
    if (target < 0)
        return RESULT_NO_SPACE;

    if (splitBlock(manager, target, newSize) && target < index)
        index++;

    Block *oldBlock = &manager->blocks[index];
    Block *newBlock = &manager->blocks[target];
    memcpy(manager->pool + newBlock->start, manager->pool + oldBlock->start, oldBlock->size);

    newBlock->status = BLOCK_ALLOCATED;
    newBlock->id = oldBlock->id;
    oldBlock->status = BLOCK_FREE;
    oldBlock->id = NO_ID;
    mergeFreeBlocks(manager);
    return RESULT_OK;
}

// capacity is reserved before any change, so a failed resize leaves everything untouched
Result resizeAllocation(MemoryManager *manager, int id, int newSize)
{
    int index;
    Result found = findAllocation(manager, id, &index);
    if (found != RESULT_OK)
        return found;
    if (newSize <= 0 || newSize > manager->poolSize)
        return RESULT_INVALID_SIZE;
    if (ensureCapacity(manager, manager->count + 1) != RESULT_OK)
        return RESULT_NO_MEMORY;

    Block *block = &manager->blocks[index];
    int change = newSize - block->size;
    if (change == 0)
        return RESULT_OK;

    int hasFreeNext = index + 1 < manager->count && manager->blocks[index + 1].status == BLOCK_FREE;
    Block *next = &manager->blocks[index + 1];

    // grow into the free neighbour, or give the shrunk bytes to it
    if (hasFreeNext && next->size >= change) {
        block->size = newSize;
        next->start += change;
        next->size -= change;
        if (next->size == 0)
            removeBlockAt(manager, index + 1);
        return RESULT_OK;
    }

    // shrink with no free neighbour: the freed bytes become a new free block
    if (change < 0) {
        Block freed = { block->start + newSize, -change, NO_ID, BLOCK_FREE };
        block->size = newSize;
        insertBlockAt(manager, index + 1, freed);
        return RESULT_OK;
    }

    return moveAllocation(manager, index, newSize);
}

Statistics computeStatistics(const MemoryManager *manager)
{
    Statistics stats = { 0, 0, 0, 0, 0 };

    for (int i = 0; i < manager->count; i++) {
        const Block *block = &manager->blocks[i];
        if (block->status == BLOCK_ALLOCATED) {
            stats.allocatedBytes += block->size;
            stats.allocatedBlocks++;
        } else {
            stats.freeBytes += block->size;
            stats.freeBlocks++;
            if (block->size > stats.largestFreeBlock)
                stats.largestFreeBlock = block->size;
        }
    }
    return stats;
}

int isFragmented(const Statistics *stats, int requestSize)
{
    return stats->freeBytes >= requestSize && stats->largestFreeBlock < requestSize;
}

// explains a failed request when enough memory is free in total but not in one piece
void reportFragmentation(MemoryManager *manager, int requestSize)
{
    Statistics stats = computeStatistics(manager);
    if (!isFragmented(&stats, requestSize))
        return;

    manager->lastFragmentedRequest = requestSize;
    printf("External fragmentation: %d bytes are free, but split into blocks of", stats.freeBytes);
    for (int i = 0; i < manager->count; i++)
        if (manager->blocks[i].status == BLOCK_FREE)
            printf(" %d", manager->blocks[i].size);
    printf(" bytes; none can hold %d bytes.\n", requestSize);
}

void printMemoryMap(const MemoryManager *manager)
{
    printf("Memory Map (%d bytes):\n", manager->poolSize);
    for (int i = 0; i < manager->count; i++) {
        const Block *block = &manager->blocks[i];
        if (block->status == BLOCK_ALLOCATED)
            printf("[Allocated #%d: %d bytes, Start: %d]\n", block->id, block->size, block->start);
        else
            printf("[Free: %d bytes, Start: %d]\n", block->size, block->start);
    }
}

void printStatistics(const MemoryManager *manager)
{
    Statistics stats = computeStatistics(manager);

    printf("Total memory: %d bytes\n", manager->poolSize);
    printf("Allocated: %d bytes\n", stats.allocatedBytes);
    printf("Free: %d bytes\n", stats.freeBytes);
    printf("Allocated blocks: %d\n", stats.allocatedBlocks);
    printf("Free blocks: %d\n", stats.freeBlocks);
    printf("Largest free block: %d bytes\n", stats.largestFreeBlock);

    int request = manager->lastFragmentedRequest;
    if (request > 0 && isFragmented(&stats, request))
        printf("Note: The request for %d bytes failed even though %d bytes are free, "
               "because the free memory is fragmented.\n", request, stats.freeBytes);
}

void printError(Result result, int id)
{
    switch (result) {
    case RESULT_INVALID_SIZE:
        printf("Invalid size! It must be between 1 and the pool size.\n");
        break;
    case RESULT_UNKNOWN_ID:
        printf("No allocation with ID %d exists.\n", id);
        break;
    case RESULT_ALREADY_FREE:
        printf("Allocation #%d has already been released.\n", id);
        break;
    case RESULT_NO_SPACE:
        printf("Failed: no contiguous free block is large enough.\n");
        break;
    case RESULT_NO_MEMORY:
        printf("Failed: could not grow the block metadata.\n");
        break;
    case RESULT_OK:
        break;
    }
}

int handleAllocate(MemoryManager *manager)
{
    int size;
    int index;
    if (readInt("Requested size: ", &size) != 0)
        return -1;

    Result result = allocateMemory(manager, size, &index);
    if (result == RESULT_OK) {
        Block *block = &manager->blocks[index];
        printf("Allocation successful. ID: %d, Start: %d, Size: %d\n", block->id, block->start, block->size);
    } else {
        printError(result, 0);
        if (result == RESULT_NO_SPACE)
            reportFragmentation(manager, size);
    }
    return 0;
}

int handleRelease(MemoryManager *manager)
{
    int id;
    if (readInt("Allocation ID: ", &id) != 0)
        return -1;

    Result result = releaseMemory(manager, id);
    if (result == RESULT_OK)
        printf("Allocation released.\n");
    else
        printError(result, id);
    return 0;
}

int handleResize(MemoryManager *manager)
{
    int id;
    int newSize;
    int index;
    if (readInt("Allocation ID: ", &id) != 0 || readInt("New size: ", &newSize) != 0)
        return -1;

    Result result = resizeAllocation(manager, id, newSize);
    if (result == RESULT_OK) {
        findAllocation(manager, id, &index);
        printf("Resize successful. ID: %d, Start: %d, Size: %d\n", id, manager->blocks[index].start, newSize);
    } else {
        printError(result, id);
        if (result == RESULT_NO_SPACE)
            reportFragmentation(manager, newSize);
    }
    return 0;
}

void runMenu(MemoryManager *manager)
{
    while (1) {
        printf("\nMenu:\n1. Allocate memory\n2. Release memory\n3. Resize allocation\n"
               "4. Display memory map\n5. Display memory statistics\n"
               "6. Change allocation strategy\n7. Exit\n");

        int choice;
        int status = 0;
        if (readInt("Choice: ", &choice) != 0)
            return;

        switch (choice) {
        case MENU_ALLOCATE:
            status = handleAllocate(manager);
            break;
        case MENU_RELEASE:
            status = handleRelease(manager);
            break;
        case MENU_RESIZE:
            status = handleResize(manager);
            break;
        case MENU_MAP:
            printMemoryMap(manager);
            break;
        case MENU_STATS:
            printStatistics(manager);
            break;
        case MENU_STRATEGY:
            status = readStrategy(&manager->strategy);
            break;
        case MENU_EXIT:
            return;
        default:
            printf("Invalid choice! Enter a number between %d and %d.\n", MENU_ALLOCATE, MENU_EXIT);
        }

        if (status != 0)
            return;
    }
}

int initManager(MemoryManager *manager, int poolSize)
{
    manager->pool = malloc(poolSize);
    manager->blocks = malloc(INITIAL_CAPACITY * sizeof(Block));
    if (manager->pool == NULL || manager->blocks == NULL) {
        free(manager->pool);
        free(manager->blocks);
        return -1;
    }

    Block wholePool = { 0, poolSize, NO_ID, BLOCK_FREE };
    manager->blocks[0] = wholePool;
    manager->poolSize = poolSize;
    manager->count = 1;
    manager->capacity = INITIAL_CAPACITY;
    manager->nextId = 1;
    manager->lastFragmentedRequest = 0;
    return 0;
}

void destroyManager(MemoryManager *manager)
{
    free(manager->blocks);
    free(manager->pool);
}

int main(void)
{
    int poolSize;
    MemoryManager manager;

    if (readIntInRange("Enter memory pool size: ", MIN_POOL_SIZE, MAX_POOL_SIZE, &poolSize) != 0)
        return 1;

    if (initManager(&manager, poolSize) != 0) {
        fprintf(stderr, "Memory allocation failed!\n");
        return 1;
    }

    if (readStrategy(&manager.strategy) == 0)
        runMenu(&manager);

    destroyManager(&manager);
    return 0;
}
