#include<stdio.h>
#include<malloc.h>
#include<string.h>
#include<ctype.h>
#include<stdlib.h>

#define POOL_SIZE (1024 * 1024)
#define MAX_CONTENT_SIZE 1024

// Block tracking structure
typedef struct MemoryBlock {
    int offset;
    int blockSize;
    int available;
    struct MemoryBlock *next;
} MEMBLOCK;

// Global memory storage
char *mainPool = NULL;
MEMBLOCK *blockList = NULL;

// Inode structure
typedef struct inode
{
    unsigned int inodeNo;
    unsigned int userId;
    unsigned int groupId;
    unsigned int linkCount;
    unsigned int referenceCount;
    unsigned int fileSize;
    char fileType[20];
    char *dataPtr;
    int memOffset;
    unsigned fileAccessPermission;
    struct inode *next;
} INODE;

// File Table Structure
typedef struct FileTable
{
    int cnt;
    int fileOffset;
    int fileMode;
    INODE *inodeEntry;
    struct FileTable *next;
} FILETABLE;

// UFDT Structure
typedef struct UFDTable
{
    int fdIndex;
    FILETABLE *fileTableEntry;
    struct UFDTable *next;
} UFDT;

struct SuperBlock
{
    int totalBlock;
    int usedBlock;
    int totalInode;
    int usedInode;
} S;

// Function prototypes
void setupMemoryPool();
int findContiguousSpace(int requiredSize);
void releaseSpace(int position, int size);
void defragmentMemory();
int makeInode(INODE **inode_head, FILETABLE **ft_head, UFDT **ufdt_head, char fname[], unsigned int perm);
void makeFile(char **dataPtr, int *memOffset, char *fname);
void makeFT(FILETABLE ***ft_ptr, UFDT ***ufdt_ptr, INODE *inode);
void makeUFDT(UFDT ****ufdt_ptr, FILETABLE *ft);
void showfd(UFDT *head);

// Setup memory storage
void setupMemoryPool() {
    mainPool = (char *)malloc(POOL_SIZE);
    if (mainPool == NULL) {
        printf("Failed to allocate memory pool!\n");
        exit(1);
    }
    memset(mainPool, 0, POOL_SIZE);
    
    blockList = (MEMBLOCK *)malloc(sizeof(MEMBLOCK));
    blockList->offset = 0;
    blockList->blockSize = POOL_SIZE;
    blockList->available = 1;
    blockList->next = NULL;
    
    printf("\n Virtual disk of 1 MB initialized successfully\n");
}

// Allocate space using first-fit
int findContiguousSpace(int requiredSize) {
    MEMBLOCK *curr = blockList;
    
    while (curr != NULL) {
        if (curr->available && curr->blockSize >= requiredSize) {
            int allocatedPos = curr->offset;
            
            if (curr->blockSize == requiredSize) {
                curr->available = 0;
            } else {
                // Split block
                MEMBLOCK *newBlock = (MEMBLOCK *)malloc(sizeof(MEMBLOCK));
                newBlock->offset = curr->offset + requiredSize;
                newBlock->blockSize = curr->blockSize - requiredSize;
                newBlock->available = 1;
                newBlock->next = curr->next;
                
                curr->blockSize = requiredSize;
                curr->available = 0;
                curr->next = newBlock;
            }
            
            printf("\n Allocated %d bytes at offset %d\n", requiredSize, allocatedPos);
            return allocatedPos;
        }
        curr = curr->next;
    }
    
    printf("\n No contiguous space available for %d bytes\n", requiredSize);
    return -1;
}

// Free space and merge blocks
void releaseSpace(int position, int size) {
    MEMBLOCK *curr = blockList;
    MEMBLOCK *prev = NULL;
    
    while (curr != NULL) {
        if (curr->offset == position && curr->blockSize == size) {
            curr->available = 1;
            
            // Merge with next if available
            if (curr->next != NULL && curr->next->available) {
                MEMBLOCK *temp = curr->next;
                curr->blockSize += temp->blockSize;
                curr->next = temp->next;
                free(temp);
            }
            
            // Merge with previous if available
            if (prev != NULL && prev->available) {
                prev->blockSize += curr->blockSize;
                prev->next = curr->next;
                free(curr);
            }
            
            printf("\n Deallocated %d bytes at offset %d\n", size, position);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// Show memory layout
void showMemoryMap() {
    MEMBLOCK *curr = blockList;
    int num = 0;
    
    printf("\n\n\t=== Memory Map ===");
    printf("\n\tBlock\tOffset\tSize\tStatus");
    printf("\n\t-------------------------------------");
    
    while (curr != NULL) {
        printf("\n\t%d\t%d\t%d\t%s", 
               num++, 
               curr->offset, 
               curr->blockSize, 
               curr->available ? "FREE" : "USED");
        curr = curr->next;
    }
    printf("\n\t-------------------------------------\n");
}


// Create file content in pool
void makeFile(char **dataPtr, int *memOffset, char *fname)
{
    char buffer[MAX_CONTENT_SIZE];
    char ch = '\0';
    int idx = 0, contentLen;
    
    printf("\n\t\tEnter the content :\n");
    printf("\n\t\tPlease enter ctrl+d to stop writing contents :-\n");
    
    while ((ch = getchar()) != EOF && idx < MAX_CONTENT_SIZE - 1)
    {
        buffer[idx++] = ch;
    }
    buffer[idx] = '\0';
    clearerr(stdin);
    
    contentLen = strlen(buffer) + 1;
    
    // Find space in pool
    *memOffset = findContiguousSpace(contentLen);
    
    if (*memOffset == -1) {
        printf("\n Failed to allocate space in memory pool\n");
        *dataPtr = NULL;
        return;
    }
    
    *dataPtr = mainPool + (*memOffset);
    strcpy(*dataPtr, buffer);
    
    printf("\n FILE IS CREATED at offset %d\n", *memOffset);
}

// Initialize UFDT
void makeUFDT(UFDT ****ufdt_ptr, FILETABLE *ft)
{
    static int descriptor = 3;
    UFDT *node = NULL, *temp = NULL;
    
    node = (UFDT *)malloc(sizeof(UFDT));
    node->fdIndex = descriptor++;
    node->fileTableEntry = ft;
    node->next = NULL;
    
    if (***ufdt_ptr == NULL)
    {
        ***ufdt_ptr = node;
        printf("\n USER FILE DESCRIPTOR IS INITIALISED...\n");
    }
    else
    {
        for (temp = (***ufdt_ptr); temp->next != NULL; temp = temp->next);
        temp->next = node;
        printf("\n USER FILE DESCRIPTOR IS INITIALISED...\n");
    }
}

// Initialize file table
void makeFT(FILETABLE ***ft_ptr, UFDT ***ufdt_ptr, INODE *inode)
{
    FILETABLE *node = NULL, *temp = NULL;
    
    printf("\n FILE TABLE IS CREATING....\n");
    node = (FILETABLE *)malloc(sizeof(FILETABLE));
    node->cnt = 1;
    node->fileOffset = 0;
    node->fileMode = 6;
    node->inodeEntry = inode;
    node->next = NULL;
    
    if (**ft_ptr == NULL)
        **ft_ptr = node;
    else
    {
        for (temp = (**ft_ptr); temp->next != NULL; temp = temp->next);
        temp->next = node;
    }
    
    printf("\n FILE TABLE IS CREATED SUCCESSFULLY...\n");
    makeUFDT(&ufdt_ptr, node);
}

// Create inode entry
int makeInode(INODE **inode_head, FILETABLE **ft_head, UFDT **ufdt_head, char fname[], unsigned int perm)
{
    INODE *node = NULL, *temp = NULL;
    static int inodeCounter = 0;
    
    if ((S.usedInode < S.totalInode) && (S.usedBlock < S.totalBlock))
    {
        printf("\n INODE IS CREATING in IIT.......\n");
        node = (INODE *)malloc(sizeof(INODE));
        node->inodeNo = ++inodeCounter;
        node->userId = 10;
        node->groupId = 10;
        node->linkCount = 1;
        node->referenceCount = 1;
        node->fileSize = 0;
        strcpy(node->fileType, "regular");
        node->fileAccessPermission = perm;
        node->dataPtr = NULL;
        node->memOffset = -1;
        
        printf("\n INODE is CREATED SUCCESSFULLY in IIT.\n");
        
        makeFile(&(node->dataPtr), &(node->memOffset), fname);
        
        if (node->memOffset == -1) {
            printf("\n Failed to allocate space for file\n");
            free(node);
            return -1;
        }
        
        S.usedBlock++;
        S.usedInode++;
        
        node->fileSize = strlen(node->dataPtr);
        node->next = NULL;
        
        if (*inode_head == NULL)
            (*inode_head) = node;
        else
        {
            for (temp = *inode_head; temp->next != NULL; temp = temp->next);
            temp->next = node;
        }
        
        makeFT(&ft_head, &ufdt_head, node);
        return 0;
    }
    else
    {
        printf("\nFile System has no enough memory");
        return -1;
    }
}

// Display file descriptors
void showfd(UFDT *head)
{
    UFDT *uptr = NULL;
    INODE *iptr = NULL;
    FILETABLE *fptr = NULL;
    
    printf("\n\t\tFile FD\tOffset\tSize");
    for (uptr = head; uptr != NULL; uptr = uptr->next)
    {
        fptr = (FILETABLE *)(uptr->fileTableEntry);
        iptr = (INODE *)(fptr->inodeEntry);
        printf("\n\t\t%d\t%d\t%d", uptr->fdIndex, iptr->memOffset, iptr->fileSize);
    }
}

// Read operation
int performRead(int fd, UFDT *ufdt_head)
{
    INODE *iptr = NULL;
    FILETABLE *fptr = NULL;
    char buffer[1024] = {'\0'};
    int found = 0, bytesToRead = 0;
    UFDT *uptr = ufdt_head;
    
    for (; uptr != NULL; uptr = uptr->next)
    {
        fptr = (FILETABLE *)(uptr->fileTableEntry);
        iptr = (INODE *)(fptr->inodeEntry);
        if (uptr->fdIndex == fd)
        {
            found = 1;
            break;
        }
    }
    
    if (found == 0)
    {
        printf("\n\t\tWrong file descriptor");
        return -1;
    }
    
    if ((iptr->fileAccessPermission == 744) || (iptr->fileAccessPermission == 766))
    {
        printf("\nHow many bytes of data do you want to see?\n");
        scanf("%d", &bytesToRead);
        
        if (bytesToRead < 0)
        {
            printf("\nFile size should be positive.");
            return -1;
        }
        
        if (iptr->fileSize < bytesToRead)
        {
            strncpy(buffer, iptr->dataPtr, iptr->fileSize);
            buffer[iptr->fileSize] = '\0';
            printf("\n\t\tFile content:\n\t\t\t%s", buffer);
            return iptr->fileSize;
        }
        
        printf("\n\t\tFile content:");
        strncpy(buffer, iptr->dataPtr, bytesToRead);
        buffer[bytesToRead] = '\0';
        printf("\n\t\t\t%s", buffer);
        return bytesToRead;
    }
    else
    {
        printf("\n\t\tYou do not have access to read this file");
        return -1;
    }
}

// Write operation
int performWrite(int fd, UFDT *ufdt_head)
{
    INODE *iptr = NULL;
    FILETABLE *fptr = NULL;
    int found = 0, option, idx = 0, newContentSize;
    char buffer[1024], ch = '\0';
    int oldPos, oldLen;
    UFDT *uptr = ufdt_head;
    
    for (; uptr != NULL; uptr = uptr->next)
    {
        fptr = (FILETABLE *)(uptr->fileTableEntry);
        iptr = (INODE *)(fptr->inodeEntry);
        if (uptr->fdIndex == fd)
        {
            found = 1;
            break;
        }
    }
    
    if (found == 0)
    {
        printf("\n\t\t\tWrong file descriptor");
        return -1;
    }
    
    if ((iptr->fileAccessPermission == 722) || (iptr->fileAccessPermission == 766))
    {
        printf("\n\t\tDo you want to\n\t\t1.Overwrite the file\n\t\t2.Append to the file\n");
        scanf("%d", &option);
        
        if ((option > 2) || (option < 1))
        {
            printf("\n\t\tWrong choice");
            return -1;
        }
        
        printf("\n\t\tEnter Contents you want to write to the file:");
        printf("\n\t\tEnter ctrl+d to stop writing:-\n");
        
        while ((ch = getchar()) != EOF && idx < 1023)
        {
            buffer[idx++] = ch;
        }
        buffer[idx] = '\0';
        clearerr(stdin);
        
        oldPos = iptr->memOffset;
        oldLen = iptr->fileSize;
        
        switch (option)
        {
        case 1: // Overwrite
            newContentSize = strlen(buffer) + 1;
            
            releaseSpace(oldPos, oldLen + 1);
            
            iptr->memOffset = findContiguousSpace(newContentSize);
            if (iptr->memOffset == -1)
            {
                printf("\n Failed to allocate space\n");
                return -1;
            }
            
            iptr->dataPtr = mainPool + iptr->memOffset;
            strcpy(iptr->dataPtr, buffer);
            iptr->fileSize = strlen(buffer);
            return iptr->fileSize;
            
        case 2: // Append
            newContentSize = oldLen + strlen(buffer) + 1;
            
            if (newContentSize > 1024)
            {
                printf("\n File size exceeds maximum limit\n");
                return -1;
            }
            
            releaseSpace(oldPos, oldLen + 1);
            
            iptr->memOffset = findContiguousSpace(newContentSize);
            if (iptr->memOffset == -1)
            {
                printf("\n Failed to allocate space\n");
                return -1;
            }
            
            // Copy old + new data
            char *prevData = (char *)malloc(oldLen + 1);
            strncpy(prevData, mainPool + oldPos, oldLen);
            prevData[oldLen] = '\0';
            
            iptr->dataPtr = mainPool + iptr->memOffset;
            strcpy(iptr->dataPtr, prevData);
            strcat(iptr->dataPtr, buffer);
            iptr->fileSize = strlen(iptr->dataPtr);
            
            free(prevData);
            return strlen(buffer);
        }
    }
    else
    {
        printf("\n\t\tAccess Denied");
        return -1;
    }
}

// List files
void showFiles(UFDT *ufdt_head)
{
    INODE *iptr = NULL;
    FILETABLE *fptr = NULL;
    UFDT *uptr;
    
    printf("\n\t\tFile FD");
    for (uptr = ufdt_head; uptr != NULL; uptr = uptr->next)
    {
        fptr = (FILETABLE *)(uptr->fileTableEntry);
        iptr = (INODE *)(fptr->inodeEntry);
        printf("\n\t\t%d", uptr->fdIndex);
    }
}

// Remove file
void removeFile(INODE **inode_head, FILETABLE **ft_head, UFDT **ufdt_head, int fd)
{
    UFDT *uptr = NULL, *uprev = NULL;
    INODE *iptr = NULL, *iprev = NULL, *inode_del = NULL;
    FILETABLE *fptr = NULL, *fprev = NULL, *ft_del = NULL;
    UFDT *ufdt_del = NULL;
    int option, found = 0;
    int position, length;
    
    // Locate entry
    for (uptr = *ufdt_head; uptr != NULL; uprev = uptr, uptr = uptr->next)
    {
        if (uptr->fdIndex == fd)
        {
            found = 1;
            ufdt_del = uptr;
            ft_del = (FILETABLE *)(uptr->fileTableEntry);
            inode_del = (INODE *)(ft_del->inodeEntry);
            position = inode_del->memOffset;
            length = inode_del->fileSize;
            break;
        }
    }
    
    if (found == 0)
    {
        printf("\n\t\tInvalid File descriptor !!");
        return;
    }
    
    printf("\n\tDo you want to delete the file with fd %d?\n\tPress 1 for yes / 0 for no:\n", fd);
    scanf("%d", &option);
    
    if (option == 1)
    {
        releaseSpace(position, length + 1);
        
        // Remove inode
        iprev = NULL;
        for (iptr = *inode_head; iptr != NULL; iprev = iptr, iptr = iptr->next)
        {
            if (iptr == inode_del)
            {
                if (iptr == *inode_head)
                    *inode_head = iptr->next;
                else
                    iprev->next = iptr->next;
                free(iptr);
                break;
            }
        }
        
        // Remove file table entry
        fprev = NULL;
        for (fptr = *ft_head; fptr != NULL; fprev = fptr, fptr = fptr->next)
        {
            if (fptr == ft_del)
            {
                if (fptr == *ft_head)
                    *ft_head = fptr->next;
                else
                    fprev->next = fptr->next;
                free(fptr);
                break;
            }
        }
        
        // Remove UFDT entry
        uprev = NULL;
        for (uptr = *ufdt_head; uptr != NULL; uprev = uptr, uptr = uptr->next)
        {
            if (uptr == ufdt_del)
            {
                if (uptr == *ufdt_head)
                    *ufdt_head = uptr->next;
                else
                    uprev->next = uptr->next;
                free(uptr);
                break;
            }
        }
        
        S.usedInode--;
        S.usedBlock--;
        
        printf("\n\t\tFile has been deleted successfully.");
    }
}

// Main function
int main()
{
    char filename[255] = {'\0'}, command[10], confirm;
    int choice, permChoice, descriptor;
    unsigned int permission;
    INODE *inode_list = NULL;
    FILETABLE *ft_list = NULL;
    UFDT *ufdt_list = NULL;
    
    setupMemoryPool();
    
    S.totalBlock = 1024;
    S.usedBlock = 0;
    S.totalInode = 1024;
    S.usedInode = 0;
    
    printf("\t///////////////////////////////////\n");
    printf("\t//      Virtual File System      //\n");
    printf("\t///////////////////////////////////\n");
    
    while (1)
    {
        printf("\n\t----Operation menu----\n");
        printf("\t1. create  - Create new file\n");
        printf("\t2. read    - Read content from existing file\n");
        printf("\t3. write   - Write data to existing file\n");
        printf("\t4. list    - List files with File Descriptor\n");
        printf("\t5. delete  - Delete existing file\n");
        printf("\t6. memmap  - Display memory allocation map\n");
        printf("\t7. quit    - Exit FileSystem\n");
        
        printf("\n\tEnter operation code: ");
        scanf("%d", &choice);
        
        switch (choice)
        {
        case 1: // Create file
            printf("\n\t\tEnter filename: ");
            scanf("%s", filename);
            
            printf("\n\t\tPermission:");
            printf("\n\t\t1.read only   2.write only  3.read and write: ");
            scanf("%d", &permChoice);
            
            switch (permChoice)
            {
            case 1:
                permission = 744;
                break;
            case 2:
                permission = 722;
                break;
            case 3:
                permission = 766;
                break;
            default:
                printf("\n\t\tInvalid permission code");
                continue;
            }
            
            makeInode(&inode_list, &ft_list, &ufdt_list, filename, permission);
            break;
            
        case 2: // Read file
            if (ufdt_list == NULL)
            {
                printf("\n No files in the system\n");
                break;
            }
            showfd(ufdt_list);
            printf("\n\tEnter file descriptor: ");
            scanf("%d", &descriptor);
            performRead(descriptor, ufdt_list);
            break;
            
        case 3: // Write to file
            if (ufdt_list == NULL)
            {
                printf("\n No files in the system\n");
                break;
            }
            showfd(ufdt_list);
            printf("\n\tEnter file descriptor: ");
            scanf("%d", &descriptor);
            performWrite(descriptor, ufdt_list);
            break;
            
        case 4: // List files
            if (ufdt_list == NULL)
            {
                printf("\n No files in the system\n");
                break;
            }
            showFiles(ufdt_list);
            break;
            
        case 5: // Delete file
            if (ufdt_list == NULL)
            {
                printf("\n No files in the system\n");
                break;
            }
            showfd(ufdt_list);
            printf("\n\tEnter file descriptor: ");
            scanf("%d", &descriptor);
            removeFile(&inode_list, &ft_list, &ufdt_list, descriptor);
            break;
            
        case 6: // Memory map
            showMemoryMap();
            break;
            
        case 7: // Exit
            printf("\tDo you want to exit? (Y/N): ");
            confirm = getchar();
            confirm = getchar();
            if (confirm == 'Y' || confirm == 'y')
            {
                free(mainPool);
                exit(0);
            }
            break;
            
        default:
            printf("\n\t\tInvalid choice");
            break;
        }
    }
    
    return 0;
}
