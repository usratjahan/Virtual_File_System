
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_FILES 100
#define MAX_BLOCKS 100
#define MAX_FILENAME 50
#define MAX_FILESIZE 20
#define MAX_CONTENT 512
#define MAX_DIRECTORY 50
#define MAX_USERS 10
#define MAX_USERNAME 30
#define MAX_PASSWORD 30

// --- File allocation structures ---
typedef struct {
    char name[MAX_FILENAME];
    int size;
    int blocks[MAX_FILESIZE]; // store allocated block numbers
    int blockCount;
    int allocationType; // 1=Contiguous, 2=Linked, 3=Indexed
    int isUsed;
} AllocFile;

AllocFile allocFiles[MAX_FILES];
int allocFileCount = 0;
int disk[MAX_BLOCKS]; // 0=free, 1=occupied
int next[MAX_BLOCKS]; // for linked allocation

// --- User and VFS structures ---
typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    char role[10]; // e.g., "admin", "guest", "user"
} User;

typedef struct {
    int id;
    char name[MAX_FILENAME];
    char directory[MAX_DIRECTORY];
    char content[MAX_CONTENT];
    int size;
    int isUsed;
    int isOpen;
    char owner[MAX_USERNAME];
    int perm_read;
    int perm_write;
    int perm_execute;
    int allocationIndex; // index in allocFiles
} File;

User users[MAX_USERS];
int userCount = 0;
File fileSystem[MAX_FILES];
int fileCount = 0;

User *currentUser = NULL;

// --- Initialize users ---
void initUsers() {
    strcpy(users[0].username, "admin"); strcpy(users[0].password, "admin123"); strcpy(users[0].role, "admin");
    strcpy(users[1].username, "guest"); strcpy(users[1].password, "guest"); strcpy(users[1].role, "guest");
    strcpy(users[2].username, "user1"); strcpy(users[2].password, "pass1"); strcpy(users[2].role, "user");
    userCount = 3;
}

// --- Authentication ---
User* authenticate(const char* username, const char* password) {
    for (int i=0;i<userCount;i++) {
        if(strcmp(users[i].username, username)==0 && strcmp(users[i].password, password)==0)
            return &users[i];
    }
    return NULL;
}

void login() {
    char username[MAX_USERNAME], password[MAX_PASSWORD];
    while(1) {
        printf("Login\nUsername: "); fgets(username, MAX_USERNAME, stdin); username[strcspn(username,"\n")]=0;
        printf("Password: "); fgets(password, MAX_PASSWORD, stdin); password[strcspn(password,"\n")]=0;
        currentUser = authenticate(username,password);
        if(currentUser) { printf("Welcome %s! Role: %s\n", currentUser->username, currentUser->role); break;}
        else printf("Invalid username/password. Try again.\n");
    }
}

void logout() {
    printf("User '%s' logged out.\n", currentUser->username);
    currentUser = NULL;
    login();
}

// --- Permissions ---
int hasPermission(File *file, char op) {
    if(strcmp(currentUser->username,file->owner)==0) {
        if(op=='r' && file->perm_read) return 1;
        if(op=='w' && file->perm_write) return 1;
        if(op=='x' && file->perm_execute) return 1;
        return 0;
    } else if(strcmp(currentUser->role,"admin")==0) return 1;
    else { if(op=='r' && file->perm_read) return 1; return 0;}
}

// --- Disk operations ---
void initializeDisk() {
    for(int i=0;i<MAX_BLOCKS;i++){ disk[i]=0; next[i]=-1; }
}

int allocateLinked(int size, int blocks[]) {
    int prev=-1, allocated=0;
    for(int i=0;i<MAX_BLOCKS && allocated<size;i++){
        if(disk[i]==0){
            disk[i]=1;
            blocks[allocated]=i;
            if(prev!=-1) next[prev]=i;
            prev=i;
            allocated++;
        }
    }
    if(allocated<size) return 0;
    next[prev]=-1;
    return 1;
}

void freeBlocks(AllocFile f) {
    for(int i=0;i<f.blockCount;i++){ disk[f.blocks[i]]=0; next[f.blocks[i]]=-1; }
}

// --- Create file with allocation ---
void createFile() {
    if(fileCount>=MAX_FILES){ printf("File system full!\n"); return;}
    if(allocFileCount>=MAX_FILES){ printf("Allocation table full!\n"); return; }

    char name[MAX_FILENAME], dir[MAX_DIRECTORY];
    printf("Enter folder name: "); fgets(dir, MAX_DIRECTORY, stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter file name: "); fgets(name, MAX_FILENAME, stdin); name[strcspn(name,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++)
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].name,name)==0 && strcmp(fileSystem[i].directory,dir)==0)
            {printf("File already exists!\n"); return;}

    int size, allocType;
    printf("Enter file size (blocks): "); scanf("%d",&size);
    printf("Choose allocation type: 1-Contiguous 2-Linked 3-Indexed: "); scanf("%d",&allocType);
    getchar();

    AllocFile af; af.size=size; af.allocationType=allocType; af.isUsed=1;
    int success=0;

    switch(allocType){
        case 1: { // Contiguous
            int start; printf("Enter starting block (0-%d): ", MAX_BLOCKS-1); scanf("%d",&start); getchar();
            if(start+size>MAX_BLOCKS){ printf("Not enough space from this block!\n"); return;}
            for(int i=start;i<start+size;i++) if(disk[i]==1){printf("Block %d occupied!\n",i); return;}
            for(int i=0;i<size;i++){ af.blocks[i]=start+i; disk[start+i]=1;}
            af.blockCount=size; success=1; break;
        }
        case 2: success=allocateLinked(size,af.blocks); af.blockCount=size; break;
        case 3: { // Indexed
            int index; printf("Enter index block (0-%d): ", MAX_BLOCKS-1); scanf("%d",&index); getchar();
            if(disk[index]==1){ printf("Index block occupied!\n"); return;}
            disk[index]=1; af.blocks[0]=index;
            int allocated=0;
            for(int i=0;i<MAX_BLOCKS && allocated<size;i++){
                if(disk[i]==0){ af.blocks[allocated+1]=i; disk[i]=1; allocated++; }
            }
            if(allocated<size){ printf("Not enough blocks!\n"); disk[index]=0; for(int j=1;j<=allocated;j++) disk[af.blocks[j]]=0; return;}
            af.blockCount=size+1; success=1; break;
        }
        default: printf("Invalid allocation type!\n"); return;
    }

    if(success){
        allocFiles[allocFileCount]=af;
        fileSystem[fileCount].id=fileCount+1;
        strcpy(fileSystem[fileCount].directory,dir);
        strcpy(fileSystem[fileCount].name,name);
        fileSystem[fileCount].content[0]='\0';
        fileSystem[fileCount].size=0;
        fileSystem[fileCount].isUsed=1;
        fileSystem[fileCount].isOpen=1;
        strcpy(fileSystem[fileCount].owner,currentUser->username);
        fileSystem[fileCount].perm_read=1; fileSystem[fileCount].perm_write=1; fileSystem[fileCount].perm_execute=1;
        fileSystem[fileCount].allocationIndex=allocFileCount;
        fileCount++; allocFileCount++;

        printf("File '%s' created with blocks: ", name);
        for(int i=0;i<af.blockCount;i++) printf("%d ",af.blocks[i]);
        printf("\n");
    }
}

// --- Write, Read, Delete, Rename, List, Search, Move/Copy, Open/Close, Resource Stats ---
// (These functions are similar to your VFS functions, but integrate allocation via fileSystem[i].allocationIndex)

void writeFile() {
    char name[MAX_FILENAME], dir[MAX_DIRECTORY], data[MAX_CONTENT];
    printf("Enter folder name: "); fgets(dir,MAX_DIRECTORY,stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter file name: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,dir)==0 && strcmp(fileSystem[i].name,name)==0){
            if(!hasPermission(&fileSystem[i],'w')) {printf("No write permission.\n"); return;}
            if(fileSystem[i].isOpen){
                printf("Enter content: "); fgets(data,MAX_CONTENT,stdin); data[strcspn(data,"\n")]=0;
                strncpy(fileSystem[i].content,data,MAX_CONTENT-1); fileSystem[i].size=strlen(fileSystem[i].content);
                printf("Content written.\n");
            } else printf("File is closed.\n");
            return;
        }
    }
    printf("File not found.\n");
}

void readFile() {
    char name[MAX_FILENAME], dir[MAX_DIRECTORY];
    printf("Enter folder name: "); fgets(dir,MAX_DIRECTORY,stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter file name: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,dir)==0 && strcmp(fileSystem[i].name,name)==0){
            if(!hasPermission(&fileSystem[i],'r')){printf("No read permission.\n"); return;}
            if(fileSystem[i].isOpen){
                AllocFile af = allocFiles[fileSystem[i].allocationIndex];
                printf("Blocks allocated: ");
                if(af.allocationType==2){ int b=af.blocks[0]; while(b!=-1){ printf("%d ",b); b=next[b]; } }
                else for(int j=0;j<af.blockCount;j++) printf("%d ",af.blocks[j]);
                printf("\nContent:\n%s\n",fileSystem[i].content);
            } else printf("File is closed.\n");
            return;
        }
    }
    printf("File not found.\n");
}


void deleteFile() {
    char name[MAX_FILENAME], dir[MAX_DIRECTORY];
    printf("Enter folder name: "); fgets(dir,MAX_DIRECTORY,stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter file name: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,dir)==0 && strcmp(fileSystem[i].name,name)==0){
            if(strcmp(currentUser->username,fileSystem[i].owner)!=0 && strcmp(currentUser->role,"admin")!=0){printf("No permission.\n"); return;}
            freeBlocks(allocFiles[fileSystem[i].allocationIndex]); allocFiles[fileSystem[i].allocationIndex].isUsed=0;
            fileSystem[i].isUsed=0; fileCount--;
            printf("File deleted.\n"); return;
        }
    }
    printf("File not found.\n");
}

void renameFile() {
    char name[MAX_FILENAME], dir[MAX_DIRECTORY], newName[MAX_FILENAME];
    printf("Enter folder name: "); fgets(dir,MAX_DIRECTORY,stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter current file name: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;
    printf("Enter new file name: "); fgets(newName,MAX_FILENAME,stdin); newName[strcspn(newName,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,dir)==0 && strcmp(fileSystem[i].name,name)==0){
            if(strcmp(currentUser->username,fileSystem[i].owner)!=0 && strcmp(currentUser->role,"admin")!=0){printf("No permission.\n"); return;}
            strcpy(fileSystem[i].name,newName); printf("File renamed.\n"); return;
        }
    }
    printf("File not found.\n");
}

void listFiles() {
    printf("\nFiles accessible to you (%s):\n",currentUser->username);
    int found=0;
    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && (strcmp(currentUser->role,"admin")==0 || strcmp(fileSystem[i].owner,currentUser->username)==0)){
            AllocFile af = allocFiles[fileSystem[i].allocationIndex];
            printf("[%d] %s/%s Size:%d Owner:%s [%s] Perm:%c%c%c AllocType:%s Blocks: ",
                   fileSystem[i].id,fileSystem[i].directory,fileSystem[i].name,fileSystem[i].size,
                   fileSystem[i].owner,fileSystem[i].isOpen?"OPEN":"CLOSED",
                   fileSystem[i].perm_read?'r':'-',fileSystem[i].perm_write?'w':'-',fileSystem[i].perm_execute?'x':'-',
                   af.allocationType==1?"Contiguous":af.allocationType==2?"Linked":"Indexed");

            if(af.allocationType==2){ // Linked
                int b=af.blocks[0]; while(b!=-1){ printf("%d ",b); b=next[b]; }
            } else {
                for(int j=0;j<af.blockCount;j++) printf("%d ",af.blocks[j]);
            }
            printf("\n");
            found=1;
        }
    }
    if(!found) printf("No files found.\n");
}


void searchFile() {
    char name[MAX_FILENAME]; printf("Enter file name to search: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;
    int found=0;
    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].name,name)==0 &&
           (strcmp(currentUser->role,"admin")==0 || strcmp(fileSystem[i].owner,currentUser->username)==0)){
            printf("Found: %s/%s Owner:%s\n",fileSystem[i].directory,fileSystem[i].name,fileSystem[i].owner);
            found=1;
        }
    }
    if(!found) printf("File not found or no permission.\n");
}

void moveOrCopyFile(int isCopy) {
    char srcName[MAX_FILENAME], srcDir[MAX_DIRECTORY], destDir[MAX_DIRECTORY];
    printf("Enter source folder: "); fgets(srcDir,MAX_DIRECTORY,stdin); srcDir[strcspn(srcDir,"\n")]=0;
    printf("Enter file name: "); fgets(srcName,MAX_FILENAME,stdin); srcName[strcspn(srcName,"\n")]=0;
    printf("Enter destination folder: "); fgets(destDir,MAX_DIRECTORY,stdin); destDir[strcspn(destDir,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,srcDir)==0 && strcmp(fileSystem[i].name,srcName)==0){
            if(strcmp(currentUser->username,fileSystem[i].owner)!=0 && strcmp(currentUser->role,"admin")!=0){printf("No permission.\n"); return;}
            if(isCopy){
                for(int j=0;j<MAX_FILES;j++){
                    if(!fileSystem[j].isUsed){
                        fileSystem[j]=fileSystem[i]; fileSystem[j].id=j+1;
                        strcpy(fileSystem[j].directory,destDir); fileSystem[j].isOpen=0; fileCount++;
                        printf("File copied.\n"); return;
                    }
                } printf("No space to copy.\n");
            } else { strcpy(fileSystem[i].directory,destDir); printf("File moved.\n"); }
            return;
        }
    }
    printf("File not found.\n");
}

void toggleFileLock() {
    char name[MAX_FILENAME], dir[MAX_DIRECTORY];
    printf("Enter folder name: "); fgets(dir,MAX_DIRECTORY,stdin); dir[strcspn(dir,"\n")]=0;
    printf("Enter file name: "); fgets(name,MAX_FILENAME,stdin); name[strcspn(name,"\n")]=0;

    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && strcmp(fileSystem[i].directory,dir)==0 && strcmp(fileSystem[i].name,name)==0){
            if(strcmp(currentUser->username,fileSystem[i].owner)!=0 && strcmp(currentUser->role,"admin")!=0){printf("No permission.\n"); return;}
            fileSystem[i].isOpen=!fileSystem[i].isOpen; printf("%s '%s/%s'\n",fileSystem[i].isOpen?"Opened":"Closed",dir,name); return;
        }
    }
    printf("File not found.\n");
}

void resourceStats() {
    int total=0, openFiles=0;
    printf("\nResource Stats for user '%s':\n",currentUser->username);
    for(int i=0;i<MAX_FILES;i++){
        if(fileSystem[i].isUsed && (strcmp(currentUser->role,"admin")==0 || strcmp(fileSystem[i].owner,currentUser->username)==0)){
            total+=fileSystem[i].size; if(fileSystem[i].isOpen) openFiles++;
            printf("- %s/%s Size:%d Open:%s\n",fileSystem[i].directory,fileSystem[i].name,fileSystem[i].size,fileSystem[i].isOpen?"Yes":"No");
        }
    }
    printf("Total storage: %d bytes\nOpen files: %d\n",total,openFiles);
}

void showMenu() {
    printf("\n========== Virtual File System ==========\n");
    printf("User: %s (Role: %s)\n",currentUser->username,currentUser->role);
    printf("1. Create File (with allocation)\n2. Write to File\n3. Read File\n4. Delete File\n5. Rename File\n6. List Accessible Files\n7. Search File\n8. Move File\n9. Copy File\n10. Open/Close File\n11. Show Resource Usage\n12. Logout\n13. Exit\n");
    printf("Choose an option: ");
}

int main() {
    initUsers();
    initializeDisk();
    login();

    int choice;
    while(1){
        showMenu(); scanf("%d",&choice); getchar();
        switch(choice){
            case 1: createFile(); break;
            case 2: writeFile(); break;
            case 3: readFile(); break;
            case 4: deleteFile(); break;
            case 5: renameFile(); break;
            case 6: listFiles(); break;
            case 7: searchFile(); break;
            case 8: moveOrCopyFile(0); break;
            case 9: moveOrCopyFile(1); break;
            case 10: toggleFileLock(); break;
            case 11: resourceStats(); break;
            case 12: logout(); break;
            case 13: printf("Exiting VFS...\n"); return 0;
            default: printf("Invalid choice.\n");
        }
    }
    return 0;
}

