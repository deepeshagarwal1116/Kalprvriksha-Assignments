#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_POOL_SIZE 1
#define MAX_POOL_SIZE 1000000
#define INITIAL_CAPACITY 8
#define INPUT_BUFFER_SIZE 64
#define NO_ID 0

typedef enum {
    BLOCK_FREE,
    BLOCK_ALLOCATED
} BlockStatus;

typedef enum {
    FIRST_FIT = 1,
    BEST_FIT = 2,
    WORST_FIT = 3
} Strategy;

typedef enum {
    MENU_ALLOCATE = 1,
    MENU_RELEASE,
    MENU_RESIZE,
    MENU_SHOW_MAP,
    MENU_SHOW_STATS,
    MENU_CHANGE_STRATEGY,
    MENU_EXIT
} MenuChoice;

typedef enum {
    RESULT_OK,
    RESULT_INVALID_SIZE,
    RESULT_UNKNOWN_ID,
    RESULT_ALREADY_FREE,
    RESULT_NO_SPACE,
    RESULT_NO_MEMORY
} Result;

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

/* ---------- input ---------- */

void clearInputLine(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

int isBlankChar(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

// reads one whole-line integer; re-prompts on non-numeric input; returns 0 on success, -1 on EOF
int readInt(const char *prompt, int *value)
{
    char line[INPUT_BUFFER_SIZE];

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
            return -1;

        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            clearInputLine();
            printf("Invalid input! Please enter a whole number.\n");
            continue;
        }

        char *end;
        errno = 0;
        long parsed = strtol(line, &end, 10);
        int hasDigits = (end != line);

        while (isBlankChar(*end))
            end++;

        if (!hasDigits || *end != '\0' || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) {
            printf("Invalid input! Please enter a whole number.\n");
            continue;
        }

        *value = (int)parsed;
        return 0;
    }
}

// like readInt, but also re-prompts until the value is within [min, max]
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

const char *strategyName(Strategy strategy)
{
    switch (strategy) {
    case FIRST_FIT:
        return "First Fit";
    case BEST_FIT:
        return "Best Fit";
    case WORST_FIT:
        return "Worst Fit";
    }
    return "Unknown";
}

int readStrategy(Strategy *strategy)
{
    int choice;
    if (readIntInRange("Select allocation strategy (1. First Fit, 2. Best Fit, 3. Worst Fit): ",
                       FIRST_FIT, WORST_FIT, &choice) != 0)
        return -1;

    *strategy = (Strategy)choice;
    printf("Strategy set to %s.\n", strategyName(*strategy));
    return 0;
}

/* ---------- block metadata array ---------- */

// grows the metadata array with realloc so it can hold at least `needed` blocks
Result ensureCapacity(MemoryManager *manager, int needed)
{
    if (needed <= manager->capacity)
        return RESULT_OK;

    int newCapacity = manager->capacity * 2;
    while (newCapacity < needed)
        newCapacity *= 2;

    Block *grown = realloc(manager->blocks, newCapacity * sizeof(Block));
    if (grown == NULL)
        return RESULT_NO_MEMORY;

    manager->blocks = grown;
    manager->capacity = newCapacity;
    return RESULT_OK;
}

// caller must have ensured capacity for one more block
void insertBlockAt(MemoryManager *manager, int index, Block block)
{
    memmove(manager->blocks + index + 1, manager->blocks + index,
            (manager->count - index) * sizeof(Block));
    *(manager->blocks + index) = block;
    manager->count++;
}

void removeBlockAt(MemoryManager *manager, int index)
{
    memmove(manager->blocks + index, manager->blocks + index + 1,
            (manager->count - index - 1) * sizeof(Block));
    manager->count--;
}

int findAllocationIndex(const MemoryManager *manager, int id)
{
    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;
        if (block->status == BLOCK_ALLOCATED && block->id == id)
            return i;
    }
    return -1;
}

// cuts block `index` down to `size` and inserts the rest as a free block right after it;
// caller must have ensured capacity; returns 1 if a block was inserted, 0 otherwise
int splitBlock(MemoryManager *manager, int index, int size)
{
    Block *block = manager->blocks + index;
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
        Block *current = manager->blocks + i;
        Block *next = manager->blocks + i + 1;

        if (current->status == BLOCK_FREE && next->status == BLOCK_FREE) {
            current->size += next->size;
            removeBlockAt(manager, i + 1);
        } else {
            i++;
        }
    }
}

/* ---------- allocation strategies ---------- */

int findFirstFit(const MemoryManager *manager, int size)
{
    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;
        if (block->status == BLOCK_FREE && block->size >= size)
            return i;
    }
    return -1;
}

int findBestFit(const MemoryManager *manager, int size)
{
    int best = -1;
    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;
        if (block->status != BLOCK_FREE || block->size < size)
            continue;
        if (best == -1 || block->size < (manager->blocks + best)->size)
            best = i;
    }
    return best;
}

int findWorstFit(const MemoryManager *manager, int size)
{
    int worst = -1;
    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;
        if (block->status != BLOCK_FREE || block->size < size)
            continue;
        if (worst == -1 || block->size > (manager->blocks + worst)->size)
            worst = i;
    }
    return worst;
}

int findFreeBlock(const MemoryManager *manager, int size)
{
    switch (manager->strategy) {
    case FIRST_FIT:
        return findFirstFit(manager, size);
    case BEST_FIT:
        return findBestFit(manager, size);
    case WORST_FIT:
        return findWorstFit(manager, size);
    }
    return -1;
}

/* ---------- allocate / release / resize ---------- */

int isValidRequestSize(const MemoryManager *manager, int size)
{
    return size > 0 && size <= manager->poolSize;
}

Result allocateMemory(MemoryManager *manager, int size, int *allocatedIndex)
{
    if (!isValidRequestSize(manager, size))
        return RESULT_INVALID_SIZE;

    int index = findFreeBlock(manager, size);
    if (index < 0)
        return RESULT_NO_SPACE;

    if (ensureCapacity(manager, manager->count + 1) != RESULT_OK)
        return RESULT_NO_MEMORY;

    splitBlock(manager, index, size);

    Block *block = manager->blocks + index;
    block->status = BLOCK_ALLOCATED;
    block->id = manager->nextId++;
    memset(manager->pool + block->start, 0, block->size);

    *allocatedIndex = index;
    return RESULT_OK;
}

Result releaseMemory(MemoryManager *manager, int id)
{
    if (id <= 0 || id >= manager->nextId)
        return RESULT_UNKNOWN_ID;

    int index = findAllocationIndex(manager, id);
    if (index < 0)
        return RESULT_ALREADY_FREE;

    Block *block = manager->blocks + index;
    block->status = BLOCK_FREE;
    block->id = NO_ID;
    mergeFreeBlocks(manager);
    return RESULT_OK;
}

Result shrinkAllocation(MemoryManager *manager, int index, int newSize)
{
    int freedBytes = (manager->blocks + index)->size - newSize;
    int nextIndex = index + 1;

    if (nextIndex < manager->count && (manager->blocks + nextIndex)->status == BLOCK_FREE) {
        Block *next = manager->blocks + nextIndex;
        next->start -= freedBytes;
        next->size += freedBytes;
    } else {
        if (ensureCapacity(manager, manager->count + 1) != RESULT_OK)
            return RESULT_NO_MEMORY;

        Block *block = manager->blocks + index;
        Block freed = { block->start + newSize, freedBytes, NO_ID, BLOCK_FREE };
        insertBlockAt(manager, nextIndex, freed);
    }

    (manager->blocks + index)->size = newSize;
    mergeFreeBlocks(manager);
    return RESULT_OK;
}

// copies the allocation into another free region and frees the old one;
// nothing is modified unless the move is guaranteed to succeed
Result moveAllocation(MemoryManager *manager, int index, int newSize)
{
    int target = findFreeBlock(manager, newSize);
    if (target < 0)
        return RESULT_NO_SPACE;

    if (ensureCapacity(manager, manager->count + 1) != RESULT_OK)
        return RESULT_NO_MEMORY;

    if (splitBlock(manager, target, newSize) && target < index)
        index++;

    Block *oldBlock = manager->blocks + index;
    Block *newBlock = manager->blocks + target;

    memcpy(manager->pool + newBlock->start, manager->pool + oldBlock->start, oldBlock->size);
    newBlock->status = BLOCK_ALLOCATED;
    newBlock->id = oldBlock->id;
    oldBlock->status = BLOCK_FREE;
    oldBlock->id = NO_ID;

    mergeFreeBlocks(manager);
    return RESULT_OK;
}

Result growAllocation(MemoryManager *manager, int index, int newSize)
{
    int extraBytes = newSize - (manager->blocks + index)->size;
    int nextIndex = index + 1;

    if (nextIndex < manager->count) {
        Block *next = manager->blocks + nextIndex;

        if (next->status == BLOCK_FREE && next->size >= extraBytes) {
            (manager->blocks + index)->size = newSize;

            if (next->size == extraBytes) {
                removeBlockAt(manager, nextIndex);
            } else {
                next->start += extraBytes;
                next->size -= extraBytes;
            }
            return RESULT_OK;
        }
    }

    return moveAllocation(manager, index, newSize);
}

Result resizeAllocation(MemoryManager *manager, int id, int newSize)
{
    if (id <= 0 || id >= manager->nextId)
        return RESULT_UNKNOWN_ID;

    int index = findAllocationIndex(manager, id);
    if (index < 0)
        return RESULT_ALREADY_FREE;

    if (!isValidRequestSize(manager, newSize))
        return RESULT_INVALID_SIZE;

    int oldSize = (manager->blocks + index)->size;
    if (newSize == oldSize)
        return RESULT_OK;
    if (newSize < oldSize)
        return shrinkAllocation(manager, index, newSize);
    return growAllocation(manager, index, newSize);
}

/* ---------- statistics and display ---------- */

Statistics computeStatistics(const MemoryManager *manager)
{
    Statistics stats = { 0, 0, 0, 0, 0 };

    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;

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

int isExternallyFragmented(const Statistics *stats, int requestSize)
{
    return stats->freeBytes >= requestSize && stats->largestFreeBlock < requestSize;
}

void printFreeBlockSizes(const MemoryManager *manager)
{
    int printed = 0;
    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;
        if (block->status != BLOCK_FREE)
            continue;

        printf("%s%d bytes at %d", printed ? ", " : "", block->size, block->start);
        printed = 1;
    }
}

// explains the failure if total free memory would have been enough
void reportFragmentation(MemoryManager *manager, int requestSize)
{
    Statistics stats = computeStatistics(manager);
    if (!isExternallyFragmented(&stats, requestSize))
        return;

    manager->lastFragmentedRequest = requestSize;
    printf("External fragmentation: %d bytes are free in total, but split into %d blocks (",
           stats.freeBytes, stats.freeBlocks);
    printFreeBlockSizes(manager);
    printf(").\nThe largest free block is %d bytes, smaller than the %d bytes requested.\n",
           stats.largestFreeBlock, requestSize);
}

void printMemoryMap(const MemoryManager *manager)
{
    printf("Memory Map (%d bytes):\n", manager->poolSize);

    for (int i = 0; i < manager->count; i++) {
        const Block *block = manager->blocks + i;

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

    int lastRequest = manager->lastFragmentedRequest;
    if (lastRequest > 0 && isExternallyFragmented(&stats, lastRequest)) {
        printf("Note: The request for %d bytes failed even though %d bytes are free, "
               "because the free memory is fragmented.\n", lastRequest, stats.freeBytes);
    }
}

void printFailure(Result result, int id)
{
    switch (result) {
    case RESULT_INVALID_SIZE:
        printf("Invalid size! It must be greater than 0 and no larger than the pool.\n");
        break;
    case RESULT_UNKNOWN_ID:
        printf("No allocation with ID %d exists.\n", id);
        break;
    case RESULT_ALREADY_FREE:
        printf("Allocation #%d has already been released.\n", id);
        break;
    case RESULT_NO_SPACE:
        printf("no contiguous free block is large enough.\n");
        break;
    case RESULT_NO_MEMORY:
        printf("could not grow the block metadata (out of memory).\n");
        break;
    case RESULT_OK:
        break;
    }
}

/* ---------- menu handlers ---------- */

int handleAllocate(MemoryManager *manager)
{
    int size;
    if (readInt("Requested size: ", &size) != 0)
        return -1;

    int index;
    Result result = allocateMemory(manager, size, &index);
    if (result == RESULT_OK) {
        const Block *block = manager->blocks + index;
        printf("Allocation successful. ID: %d, Start: %d, Size: %d\n",
               block->id, block->start, block->size);
        return 0;
    }

    if (result == RESULT_INVALID_SIZE) {
        printFailure(result, 0);
        return 0;
    }

    printf("Allocation failed: ");
    printFailure(result, 0);
    if (result == RESULT_NO_SPACE)
        reportFragmentation(manager, size);
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
        printFailure(result, id);
    return 0;
}

int handleResize(MemoryManager *manager)
{
    int id;
    int newSize;
    if (readInt("Allocation ID: ", &id) != 0)
        return -1;

    int index = findAllocationIndex(manager, id);
    if (id <= 0 || index < 0) {
        printFailure(id > 0 && id < manager->nextId ? RESULT_ALREADY_FREE : RESULT_UNKNOWN_ID, id);
        return 0;
    }

    int oldStart = (manager->blocks + index)->start;
    if (readInt("New size: ", &newSize) != 0)
        return -1;

    Result result = resizeAllocation(manager, id, newSize);
    if (result == RESULT_OK) {
        const Block *block = manager->blocks + findAllocationIndex(manager, id);
        printf("Resize successful. ID: %d, Start: %d, Size: %d", block->id, block->start, block->size);
        if (block->start != oldStart)
            printf(" (moved from %d)", oldStart);
        printf("\n");
        return 0;
    }

    if (result == RESULT_INVALID_SIZE) {
        printFailure(result, id);
        return 0;
    }

    printf("Resize failed, allocation #%d is unchanged: ", id);
    printFailure(result, id);
    if (result == RESULT_NO_SPACE)
        reportFragmentation(manager, newSize);
    return 0;
}

void printMenu(void)
{
    printf("\nMenu:\n");
    printf("1. Allocate memory\n");
    printf("2. Release memory\n");
    printf("3. Resize allocation\n");
    printf("4. Display memory map\n");
    printf("5. Display memory statistics\n");
    printf("6. Change allocation strategy\n");
    printf("7. Exit\n");
}

/* ---------- setup and main loop ---------- */

int initManager(MemoryManager *manager, int poolSize)
{
    manager->pool = malloc(poolSize);
    if (manager->pool == NULL)
        return -1;

    manager->blocks = malloc(INITIAL_CAPACITY * sizeof(Block));
    if (manager->blocks == NULL) {
        free(manager->pool);
        manager->pool = NULL;
        return -1;
    }

    manager->poolSize = poolSize;
    manager->capacity = INITIAL_CAPACITY;
    manager->count = 1;
    manager->nextId = 1;
    manager->lastFragmentedRequest = 0;

    Block whole = { 0, poolSize, NO_ID, BLOCK_FREE };
    *manager->blocks = whole;
    return 0;
}

void destroyManager(MemoryManager *manager)
{
    free(manager->blocks);
    free(manager->pool);
    manager->blocks = NULL;
    manager->pool = NULL;
}

void runMenu(MemoryManager *manager)
{
    while (1) {
        printMenu();

        int choice;
        if (readInt("Choice: ", &choice) != 0)
            return;

        int status = 0;
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
        case MENU_SHOW_MAP:
            printMemoryMap(manager);
            break;
        case MENU_SHOW_STATS:
            printStatistics(manager);
            break;
        case MENU_CHANGE_STRATEGY:
            status = readStrategy(&manager->strategy);
            break;
        case MENU_EXIT:
            printf("Exiting.\n");
            return;
        default:
            printf("Invalid choice! Please enter a number between %d and %d.\n",
                   MENU_ALLOCATE, MENU_EXIT);
        }

        if (status != 0)
            return;
    }
}

int main(void)
{
    int poolSize;
    if (readIntInRange("Enter memory pool size: ", MIN_POOL_SIZE, MAX_POOL_SIZE, &poolSize) != 0) {
        printf("\nNo input received.\n");
        return 1;
    }

    MemoryManager manager;
    if (initManager(&manager, poolSize) != 0) {
        fprintf(stderr, "Memory allocation failed!\n");
        return 1;
    }

    if (readStrategy(&manager.strategy) != 0) {
        printf("\nNo input received.\n");
        destroyManager(&manager);
        return 1;
    }

    runMenu(&manager);
    destroyManager(&manager);
    return 0;
}
