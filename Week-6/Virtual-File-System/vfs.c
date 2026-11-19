#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define NUM_BLOCKS 1024
#define MAX_NAME_LENGTH 50
#define INPUT_BUFFER_SIZE 8192

typedef enum { NODE_FILE, NODE_DIRECTORY } NodeType;

typedef struct FreeBlock {
    int index;
    struct FreeBlock *prev;
    struct FreeBlock *next;
} FreeBlock;

typedef struct FileNode {
    char name[MAX_NAME_LENGTH + 1];
    NodeType type;
    struct FileNode *parent;
    struct FileNode *next;      // circular list of siblings
    struct FileNode *prev;
    struct FileNode *children;  // directories only: first child, NULL when empty
    FreeBlock **blockPointers;  // files only: the block nodes this file holds, in order
    int blockCount;
    int contentSize;
} FileNode;

typedef struct {
    unsigned char (*disk)[BLOCK_SIZE];
    FreeBlock *freeHead;
    FreeBlock *freeTail;
    int freeCount;
    FileNode *root;
    FileNode *cwd;
    int isRunning;
} Vfs;

typedef struct {
    const char *name;
    void (*run)(Vfs *vfs, char *args);
} Command;

/* ---------- free block list (doubly linked) ---------- */

void appendFreeBlock(Vfs *vfs, FreeBlock *block)
{
    block->next = NULL;
    block->prev = vfs->freeTail;

    if (vfs->freeTail != NULL) {
        vfs->freeTail->next = block;
    } else {
        vfs->freeHead = block;
    }
    vfs->freeTail = block;
    vfs->freeCount++;
}

// unlinks the head of the free list and hands the node to the caller; caller must check freeCount
FreeBlock *takeFreeBlock(Vfs *vfs)
{
    FreeBlock *head = vfs->freeHead;

    vfs->freeHead = head->next;
    if (vfs->freeHead != NULL) {
        vfs->freeHead->prev = NULL;
    } else {
        vfs->freeTail = NULL;
    }
    vfs->freeCount--;

    head->next = NULL;
    head->prev = NULL;
    return head;
}

void freeBlockChain(FreeBlock *block)
{
    while (block != NULL) {
        FreeBlock *next = block->next;
        free(block);
        block = next;
    }
}

// moves a file's own block nodes back to the tail of the free list (no allocation, cannot fail)
void releaseFileBlocks(Vfs *vfs, FileNode *file)
{
    for (int i = 0; i < file->blockCount; i++) {
        appendFreeBlock(vfs, file->blockPointers[i]);
    }

    free(file->blockPointers);
    file->blockPointers = NULL;
    file->blockCount = 0;
    file->contentSize = 0;
}

/* ---------- directory tree (circular sibling lists) ---------- */

FileNode *createNode(const char *name, NodeType type)
{
    FileNode *node = calloc(1, sizeof(FileNode));
    if (node == NULL) {
        return NULL;
    }

    strcpy(node->name, name);
    node->type = type;
    return node;
}

void freeNode(FileNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->children != NULL) {
        node->children->prev->next = NULL;  // break the circle so the walk ends
        FileNode *child = node->children;
        while (child != NULL) {
            FileNode *next = child->next;
            freeNode(child);
            child = next;
        }
    }

    for (int i = 0; i < node->blockCount; i++) {
        free(node->blockPointers[i]);
    }
    free(node->blockPointers);
    free(node);
}

void insertChild(FileNode *dir, FileNode *node)
{
    node->parent = dir;

    if (dir->children == NULL) {
        node->next = node;
        node->prev = node;
        dir->children = node;
        return;
    }

    FileNode *first = dir->children;
    FileNode *last = first->prev;
    node->prev = last;
    node->next = first;
    last->next = node;
    first->prev = node;
}

void removeChild(FileNode *dir, FileNode *node)
{
    if (node->next == node) {
        dir->children = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        if (dir->children == node) {
            dir->children = node->next;
        }
    }

    node->next = NULL;
    node->prev = NULL;
}

FileNode *findChild(const FileNode *dir, const char *name)
{
    FileNode *child = dir->children;
    if (child == NULL) {
        return NULL;
    }

    do {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    } while (child != dir->children);

    return NULL;
}

void printPathParts(const FileNode *node)
{
    if (node->parent == NULL) {
        return;
    }
    printPathParts(node->parent);
    printf("/%s", node->name);
}

void printPath(const FileNode *node)
{
    if (node->parent == NULL) {
        printf("/");
    } else {
        printPathParts(node);
    }
}

/* ---------- setup and teardown ---------- */

void destroyVfs(Vfs *vfs)
{
    freeNode(vfs->root);
    freeBlockChain(vfs->freeHead);
    free(vfs->disk);

    vfs->root = NULL;
    vfs->cwd = NULL;
    vfs->freeHead = NULL;
    vfs->freeTail = NULL;
    vfs->disk = NULL;
    vfs->freeCount = 0;
}

int initVfs(Vfs *vfs)
{
    memset(vfs, 0, sizeof(Vfs));

    vfs->disk = malloc(NUM_BLOCKS * sizeof(*vfs->disk));
    vfs->root = createNode("", NODE_DIRECTORY);
    if (vfs->disk == NULL || vfs->root == NULL) {
        destroyVfs(vfs);
        return -1;
    }

    for (int i = 0; i < NUM_BLOCKS; i++) {
        FreeBlock *block = malloc(sizeof(FreeBlock));
        if (block == NULL) {
            destroyVfs(vfs);
            return -1;
        }
        block->index = i;
        appendFreeBlock(vfs, block);
    }

    vfs->cwd = vfs->root;
    return 0;
}

/* ---------- input parsing ---------- */

void clearInputLine(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

// reads one line without its newline; returns 1 on success, 0 if too long (discarded), -1 on EOF
int readLine(char *buffer, int size)
{
    if (fgets(buffer, size, stdin) == NULL) {
        return -1;
    }

    char *newline = strchr(buffer, '\n');
    if (newline != NULL) {
        *newline = '\0';
    } else if (!feof(stdin)) {
        clearInputLine();
        return 0;
    }

    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\r') {
        buffer[length - 1] = '\0';
    }
    return 1;
}

int isSpace(char ch)
{
    return ch == ' ' || ch == '\t';
}

// splits off the next space-separated word; returns NULL when none is left
char *nextToken(char **cursor)
{
    char *p = *cursor;
    while (isSpace(*p)) {
        p++;
    }
    if (*p == '\0') {
        *cursor = p;
        return NULL;
    }

    char *start = p;
    while (*p != '\0' && !isSpace(*p)) {
        p++;
    }
    if (*p != '\0') {
        *p = '\0';
        p++;
    }

    *cursor = p;
    return start;
}

// returns the only argument, or prints the usage and returns NULL
char *singleArgument(char *args, const char *usage)
{
    char *arg = nextToken(&args);
    if (arg == NULL || nextToken(&args) != NULL) {
        printf("Usage: %s\n", usage);
        return NULL;
    }
    return arg;
}

int hasNoArguments(char *args, const char *usage)
{
    if (nextToken(&args) != NULL) {
        printf("Usage: %s\n", usage);
        return 0;
    }
    return 1;
}

int isValidName(const char *name)
{
    if (strlen(name) > MAX_NAME_LENGTH) {
        printf("Name is too long (max %d characters).\n", MAX_NAME_LENGTH);
        return 0;
    }
    if (strchr(name, '/') != NULL || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("Invalid name '%s'.\n", name);
        return 0;
    }
    return 1;
}

// strips surrounding spaces and the enclosing quotes; returns NULL unless the text is quoted
char *parseContent(char *text)
{
    while (isSpace(*text)) {
        text++;
    }

    size_t length = strlen(text);
    while (length > 0 && isSpace(text[length - 1])) {
        text[--length] = '\0';
    }
    if (length == 0) {
        return NULL;
    }

    char quote = text[0];
    if (quote != '"' && quote != '\'') {
        return NULL;
    }
    if (length < 2 || text[length - 1] != quote) {
        return NULL;
    }

    text[length - 1] = '\0';
    return text + 1;
}

// looks up `name` in cwd and checks its type, printing an error if either fails
FileNode *findEntry(const Vfs *vfs, const char *name, NodeType type)
{
    FileNode *node = findChild(vfs->cwd, name);
    if (node == NULL) {
        printf("'%s' not found.\n", name);
        return NULL;
    }
    if (node->type != type) {
        printf("'%s' is a %s.\n", name, node->type == NODE_DIRECTORY ? "directory, not a file" : "file, not a directory");
        return NULL;
    }
    return node;
}

/* ---------- commands ---------- */

void createEntry(Vfs *vfs, char *args, NodeType type)
{
    int isDirectory = (type == NODE_DIRECTORY);
    char *name = singleArgument(args, isDirectory ? "mkdir <name>" : "create <name>");
    if (name == NULL || !isValidName(name)) {
        return;
    }

    if (findChild(vfs->cwd, name) != NULL) {
        printf("'%s' already exists.\n", name);
        return;
    }

    FileNode *node = createNode(name, type);
    if (node == NULL) {
        printf("Out of memory.\n");
        return;
    }

    insertChild(vfs->cwd, node);
    printf("%s '%s' created successfully.\n", isDirectory ? "Directory" : "File", name);
}

void cmdMkdir(Vfs *vfs, char *args)
{
    createEntry(vfs, args, NODE_DIRECTORY);
}

void cmdCreate(Vfs *vfs, char *args)
{
    createEntry(vfs, args, NODE_FILE);
}

void cmdWrite(Vfs *vfs, char *args)
{
    char *name = nextToken(&args);
    char *content = (name == NULL) ? NULL : parseContent(args);
    if (content == NULL) {
        printf("Usage: write <file> \"content\"\n");
        return;
    }

    FileNode *file = findEntry(vfs, name, NODE_FILE);
    if (file == NULL) {
        return;
    }

    int size = (int)strlen(content);
    int needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int available = vfs->freeCount + file->blockCount;  // the old content is replaced
    if (needed > available) {
        printf("Write failed: disk full (needs %d blocks, only %d available).\n", needed, available);
        return;
    }

    FreeBlock **pointers = NULL;
    if (needed > 0) {
        pointers = malloc(needed * sizeof(FreeBlock *));
        if (pointers == NULL) {
            printf("Out of memory.\n");
            return;
        }
    }

    releaseFileBlocks(vfs, file);

    for (int i = 0; i < needed; i++) {
        int offset = i * BLOCK_SIZE;
        int chunk = (size - offset < BLOCK_SIZE) ? size - offset : BLOCK_SIZE;
        pointers[i] = takeFreeBlock(vfs);
        memcpy(vfs->disk[pointers[i]->index], content + offset, chunk);
    }

    file->blockPointers = pointers;
    file->blockCount = needed;
    file->contentSize = size;
    printf("Data written successfully (size=%d bytes).\n", size);
}

void cmdRead(Vfs *vfs, char *args)
{
    char *name = singleArgument(args, "read <file>");
    FileNode *file = (name == NULL) ? NULL : findEntry(vfs, name, NODE_FILE);
    if (file == NULL) {
        return;
    }

    for (int i = 0; i < file->blockCount; i++) {
        int offset = i * BLOCK_SIZE;
        int chunk = (file->contentSize - offset < BLOCK_SIZE) ? file->contentSize - offset : BLOCK_SIZE;
        fwrite(vfs->disk[file->blockPointers[i]->index], 1, chunk, stdout);
    }
    printf("\n");
}

void cmdDelete(Vfs *vfs, char *args)
{
    char *name = singleArgument(args, "delete <file>");
    FileNode *file = (name == NULL) ? NULL : findEntry(vfs, name, NODE_FILE);
    if (file == NULL) {
        return;
    }

    releaseFileBlocks(vfs, file);
    removeChild(vfs->cwd, file);
    freeNode(file);
    printf("File deleted successfully.\n");
}

void cmdRmdir(Vfs *vfs, char *args)
{
    char *name = singleArgument(args, "rmdir <directory>");
    FileNode *dir = (name == NULL) ? NULL : findEntry(vfs, name, NODE_DIRECTORY);
    if (dir == NULL) {
        return;
    }

    if (dir->children != NULL) {
        printf("Directory '%s' is not empty.\n", name);
        return;
    }

    removeChild(vfs->cwd, dir);
    freeNode(dir);
    printf("Directory removed successfully.\n");
}

void cmdLs(Vfs *vfs, char *args)
{
    if (!hasNoArguments(args, "ls")) {
        return;
    }

    FileNode *child = vfs->cwd->children;
    if (child == NULL) {
        printf("(empty)\n");
        return;
    }

    do {
        printf("%s%s\n", child->name, child->type == NODE_DIRECTORY ? "/" : "");
        child = child->next;
    } while (child != vfs->cwd->children);
}

void cmdCd(Vfs *vfs, char *args)
{
    char *name = singleArgument(args, "cd <directory> | cd .. | cd /");
    if (name == NULL) {
        return;
    }

    FileNode *target;
    if (strcmp(name, "/") == 0) {
        target = vfs->root;
    } else if (strcmp(name, "..") == 0) {
        target = (vfs->cwd->parent != NULL) ? vfs->cwd->parent : vfs->cwd;
    } else {
        target = findEntry(vfs, name, NODE_DIRECTORY);
        if (target == NULL) {
            return;
        }
    }

    vfs->cwd = target;
    printf("Moved to ");
    printPath(vfs->cwd);
    printf("\n");
}

void cmdPwd(Vfs *vfs, char *args)
{
    if (!hasNoArguments(args, "pwd")) {
        return;
    }
    printPath(vfs->cwd);
    printf("\n");
}

void cmdDf(Vfs *vfs, char *args)
{
    if (!hasNoArguments(args, "df")) {
        return;
    }

    int used = NUM_BLOCKS - vfs->freeCount;
    printf("Total Blocks: %d\n", NUM_BLOCKS);
    printf("Used Blocks: %d\n", used);
    printf("Free Blocks: %d\n", vfs->freeCount);
    printf("Disk Usage: %.2f%%\n", 100.0 * used / NUM_BLOCKS);
}

void cmdExit(Vfs *vfs, char *args)
{
    if (hasNoArguments(args, "exit")) {
        vfs->isRunning = 0;
    }
}

static const Command COMMANDS[] = {
    { "mkdir", cmdMkdir },
    { "create", cmdCreate },
    { "write", cmdWrite },
    { "read", cmdRead },
    { "delete", cmdDelete },
    { "rmdir", cmdRmdir },
    { "ls", cmdLs },
    { "cd", cmdCd },
    { "pwd", cmdPwd },
    { "df", cmdDf },
    { "exit", cmdExit },
};

#define COMMAND_COUNT ((int)(sizeof(COMMANDS) / sizeof(COMMANDS[0])))

const Command *findCommand(const char *name)
{
    for (int i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(COMMANDS[i].name, name) == 0) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

void printUnknownCommand(const char *name)
{
    printf("Unknown command '%s'. Commands:", name);
    for (int i = 0; i < COMMAND_COUNT; i++) {
        printf("%s %s", i == 0 ? "" : ",", COMMANDS[i].name);
    }
    printf("\n");
}

void runShell(Vfs *vfs)
{
    char line[INPUT_BUFFER_SIZE];

    vfs->isRunning = 1;
    while (vfs->isRunning) {
        printPath(vfs->cwd);
        printf(" > ");
        fflush(stdout);

        int status = readLine(line, sizeof(line));
        if (status < 0) {
            printf("\n");
            return;
        }
        if (status == 0) {
            printf("Input too long (max %d characters per line).\n", INPUT_BUFFER_SIZE - 2);
            continue;
        }

        char *cursor = line;
        char *command = nextToken(&cursor);
        if (command == NULL) {
            continue;
        }

        const Command *entry = findCommand(command);
        if (entry == NULL) {
            printUnknownCommand(command);
            continue;
        }
        entry->run(vfs, cursor);
    }
}

int main(void)
{
    Vfs vfs;
    if (initVfs(&vfs) != 0) {
        fprintf(stderr, "Failed to initialize the virtual disk.\n");
        return 1;
    }

    printf("Compact VFS - ready. Type 'exit' to quit.\n");
    runShell(&vfs);

    destroyVfs(&vfs);
    printf("Memory released. Exiting program...\n");
    return 0;
}
