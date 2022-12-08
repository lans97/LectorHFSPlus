#include <ncurses.h>
#include "hfsplus.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define MBR_ADDR 0x01BE
#define PART_BOOT (0x00 + MBR_ADDR)
#define PART_START (0x01 + MBR_ADDR)
#define PART_TYPE (0x04 + MBR_ADDR)
#define PART_END (0x05 + MBR_ADDR)
#define PART_LBA (0x08 + MBR_ADDR)
#define PART_NSECTORS (0x0c + MBR_ADDR)

typedef struct partition{
    unsigned char boot;
    unsigned int head_s;
    unsigned int cylinder_s;
    unsigned int sector_s;
    unsigned char type;
    unsigned int head_e;
    unsigned int cylinder_e;
    unsigned int sector_e;
    unsigned int lba;
    unsigned int n_sectors;
    HFSPlusVolumeHeader *header;
} partition_t;

partition_t _Parts[4];
unsigned char _CurrPart = 0;
unsigned char _CurrBlock = 0;
int _BlockSize;
int _NodeSize;

int fd;
char *_Map;
char *mapFile(char *filePath);
void readPart(char* diskfile, unsigned int number, partition_t *partStruct);
unsigned int chs2lba(int cylinder, int head, int sector);
void Part_Print(partition_t *part);
void Header_Print(HFSPlusVolumeHeader *header);
void ForkData_Print(HFSPlusForkData *fdstruct);
void ExtentRecord_Print(HFSPlusExtentRecord *erstruct);
void NodeDescriptor_Print(size_t start);
void BTHeader_Print(size_t start);
void LeafNode_Print(size_t start, int records, int next);
void IndexNode_Print(size_t start, int records);

int main(int argc, char *argv[]){

    if(argc != 2){
        perror("Usage: diskView <DISK IMAGE FILE>");
        exit(EXIT_FAILURE);
    }

    // Init ncurses and settings
    initscr();
    raw();
    noecho();
    curs_set(0);
    cbreak();
    keypad(stdscr, TRUE);
    leaveok(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    int input;
    unsigned char part_bool[4];

    if ((_Map = mapFile(argv[1])) == NULL){
        endwin(); // ncurses cleanup
        exit(EXIT_FAILURE);
    }

    for(int i = 0; i < 4; i++){
        readPart(_Map, i, &_Parts[i]);
        if(_Parts[i].head_s != 0){
            _Parts[i].header = (HFSPlusVolumeHeader*) (_Map + (_Parts[i].lba * 512) + 1024);
            part_bool[i] = 1;
        }else {
            part_bool[i] = 0;
        }
    }

    while (input != 'q'){
        input = getch();
        move(0, 0);
        printw("Part [%d]\n", _CurrPart);
        Part_Print(&_Parts[_CurrPart]);
        refresh();
        switch(input){
            case KEY_RIGHT:
                if(_CurrPart < 3)
                    _CurrPart ++;
                break;
            case KEY_LEFT:
                if(_CurrPart > 0)
                    _CurrPart--;
                break;
            case KEY_UP:
                break;
            case KEY_DOWN:
                break;
            case 10:
                if (part_bool[_CurrPart])
                    Header_Print(_Parts[_CurrPart].header);
                break;
            default:
        }
    }
    endwin();

    close(fd);

    return 0;
}

char *mapFile(char *filePath) {
    /* Abre archivo */
    fd = open(filePath, O_RDONLY);
    if (fd == -1) {
    	perror("Error abriendo el archivo");
	    return(NULL);
    }
    /* Mapea archivo */
    struct stat st;
    fstat(fd,&st);
    int fs = st.st_size;
    char *map = mmap(0, fs, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
    	close(fd);
	    perror("Error mapeando el archivo");
	    return(NULL);
    }
  return map;
}

void readPart(char* diskfile, unsigned int number, partition_t *partStruct){
    int offset = number * 16;
    // Bootable flag
    partStruct->boot = (unsigned char) diskfile[PART_BOOT + offset];
    // Start
    partStruct->head_s = (unsigned int)(
        (unsigned char)diskfile[PART_START + offset]);
    partStruct->cylinder_s = (unsigned int)(
        ((unsigned char)diskfile[PART_START + offset + 1] & 0xc0) << 2 |
        ((unsigned char)diskfile[PART_START + offset + 2]) );
    partStruct->sector_s = (unsigned char)(
        (unsigned char)diskfile[PART_START + offset + 1] & 0x3f );
    partStruct->type = (unsigned int)diskfile[PART_TYPE + offset];
    // End
    partStruct->head_e = (unsigned int)(
        (unsigned char)diskfile[PART_END + offset] );
    partStruct->cylinder_e = (unsigned int)(
        ((unsigned char)diskfile[PART_END + offset + 1] & 0xc0) << 2 |
        ((unsigned char)diskfile[PART_END + offset + 2]) );
    partStruct->sector_e = (unsigned char)(
        (unsigned char)diskfile[PART_END + offset + 1] & 0x3f );
    // Logical Base Address
    partStruct->lba = (unsigned int)(
        (unsigned char)diskfile[PART_LBA + offset] |
        (unsigned char)diskfile[PART_LBA + offset + 1] << 8 |
        (unsigned char)diskfile[PART_LBA + offset + 2] << 16|
        (unsigned char)diskfile[PART_LBA + offset + 3] << 24);
    // Número de sectores
    partStruct->n_sectors = (unsigned int)(
        (unsigned char)diskfile[PART_NSECTORS + offset] |
        (unsigned char)diskfile[PART_NSECTORS + offset + 1] << 8 |
        (unsigned char)diskfile[PART_NSECTORS + offset + 2] << 16|
        (unsigned char)diskfile[PART_NSECTORS + offset + 3] << 24);
}

void Part_Print(partition_t *part) {
    printw("Boot: %x\n", part->boot);
    printw("Type: %x\n", part->type);
    printw("Start:\n");
    printw("\tCylinder: %d\n", part->cylinder_s);
    printw("\tHead: %d\n", part->head_s);
    printw("\tSector: %d\n", part->sector_s);
    printw("End:\n");
    printw("\tCylinder: %d\n", part->cylinder_e);
    printw("\tHead: %d\n", part->head_e);
    printw("\tSector: %d\n", part->sector_e);
    printw("Logic Base Address: %d\n", part->lba);
    printw("N of Sectors: %d\n", part->n_sectors);
}

void Header_Print(HFSPlusVolumeHeader *header) {
    int input = getch();
    _BlockSize = BIG_ENDIAN_LONG(header->blockSize);

    clear();
    while (input != 'q'){
        move(0, 0);
        printw("Version: %u\n", BIG_ENDIAN_SHORT(header->version));

        printw("\n");

        printw("FileCount: %u\n", BIG_ENDIAN_LONG(header->fileCount));
        printw("FolderCount: %u\n", BIG_ENDIAN_LONG(header->folderCount));

        printw("\n");

        printw("BlockSize: %u\n", _BlockSize);
        printw("TotalBlocks: %u\n", BIG_ENDIAN_LONG(header->totalBlocks));
        printw("FreeBlocks: %u\n", BIG_ENDIAN_LONG(header->freeBlocks));

        if (input == 10)
            ForkData_Print(&header->catalogFile);

        refresh();
        input = getch();
    }
    clear();
}

void ForkData_Print(HFSPlusForkData *fdstruct){
    int input = getch();
    clear();
    while(input != 'q'){
        move(0,0);
        printw("LogicalSize: %llu\n", BIG_ENDIAN_LONGLONG(fdstruct->logicalSize));
        printw("ClumpSize: %x\n", BIG_ENDIAN_LONG(fdstruct->logicalSize));
        printw("TotalBlocks: %x\n", BIG_ENDIAN_LONG(fdstruct->logicalSize));

        if(input == 10)
            ExtentRecord_Print(&(fdstruct->extents));

        refresh();
        input = getch();
    }
    clear();
}

void ExtentRecord_Print(HFSPlusExtentRecord *erstruct){
    int input = getch();
    clear();
    while(input != 'q'){
        move(0,0);
        printw("StartBlock %d: %x\n", _CurrBlock, BIG_ENDIAN_LONG(erstruct[_CurrBlock]->startBlock));
        printw("BlockCount %d: %x\n", _CurrBlock, BIG_ENDIAN_LONG(erstruct[_CurrBlock]->blockCount));

        switch (input) {
        case KEY_RIGHT:
            if(_CurrBlock < 7)
                _CurrBlock++;
            break;
        case KEY_LEFT:
            if(_CurrBlock > 0)
                _CurrBlock--;
            break;
        case 10:
            NodeDescriptor_Print(BIG_ENDIAN_LONG(erstruct[_CurrBlock]->startBlock) * _BlockSize + (size_t)_Map + (_Parts[_CurrPart].lba * 512));
            break;
        }

        refresh();
        input = getch();
    }
    clear();
}

void NodeDescriptor_Print(size_t start){
    BTNodeDescriptor *nd = (BTNodeDescriptor*) start;

    int input = getch();
    clear();

    while(input != 'q'){
        move(0, 0);
        printw("%lu\n", (size_t)nd - (size_t)_Map );
        printw("B Link: %d\n", BIG_ENDIAN_LONG(nd->bLink));
        printw("F Link: %d\n", BIG_ENDIAN_LONG(nd->fLink));
        printw("Kind: %d\n", nd->kind);
        printw("Height: %d\n", nd->height);
        printw("Num Records: %d\n", BIG_ENDIAN_SHORT(nd->numRecords));
        printw("Reserved %d\n", BIG_ENDIAN_SHORT(nd->reserved));

        switch (input) {
        case 10:
            switch (nd->kind) {
            case kBTHeaderNode:
                BTHeader_Print(start);
                break;
            case kBTLeafNode:
                LeafNode_Print(start, BIG_ENDIAN_SHORT(nd->numRecords), BIG_ENDIAN_LONG(nd->fLink));
                break;
            case kBTIndexNode:
                IndexNode_Print(start, BIG_ENDIAN_SHORT(nd->numRecords));
                break;
            }
            break;
        }
        input = getch();
    }
    clear();
}

void BTHeader_Print(size_t start){
    BTHeaderRec *btree = (BTHeaderRec*) (start + 12);
    int input = getch();

    _NodeSize = BIG_ENDIAN_SHORT(btree->nodeSize);

    clear();
    while(input != 'q'){
        move(0, 0);
        printw("%lu\n", (size_t)btree - (size_t)_Map);
        printw("Attributes: %x\n", BIG_ENDIAN_LONG(btree->attributes));
        printw("Clump Size: %u\n", BIG_ENDIAN_LONG(btree->clumpSize));
        printw("First Leaf Node: %d\n", BIG_ENDIAN_LONG(btree->firstLeafNode));
        printw("Last Leaf Node: %d\n", BIG_ENDIAN_LONG(btree->lastLeafNode));
        printw("Leaf Records: %d\n", BIG_ENDIAN_LONG(btree->leafRecords));
        printw("Tree Depth: %d\n", BIG_ENDIAN_SHORT(btree->treeDepth));
        printw("Free Nodes: %d\n", BIG_ENDIAN_LONG(btree->freeNodes));
        printw("Root Node: %x\n", BIG_ENDIAN_LONG(btree->rootNode));
        printw("Node Size: %d\n", _NodeSize);

        switch (input) {
        case 10:
            NodeDescriptor_Print((BIG_ENDIAN_LONG(btree->rootNode) * _NodeSize) + start);
            break;
        case 'l':
            NodeDescriptor_Print((BIG_ENDIAN_LONG(btree->firstLeafNode) * _NodeSize) + start);
            break;
        case KEY_RIGHT:
            break;
        case KEY_LEFT:
            break;
        }

        input = getch();
    }
    clear();

}

void IndexNode_Print(size_t start, int records){
    size_t dir = start;
    size_t offsetDir = dir + _NodeSize;
    wchar_t unicode[255];
    int offset;
    unsigned char select = 1;

    HFSPlusCatalogKey *ck;
    int i;
    int input = getch();
    clear();

    while(input != 'q'){
        move(0, 0);
        offset = BIG_ENDIAN_SHORT(*(int*)(offsetDir - select*2));
        ck = (dir + offset);
        printw("OD: %lu\n", (offsetDir - (select*2) - (size_t)_Map));
        printw("select: %d\n", select);
        printw("Offset: %x\n", offset);
        printw("Key Length: %u\n", BIG_ENDIAN_SHORT(ck->keyLength));
        printw("Parent ID: %u\n", BIG_ENDIAN_LONG(ck->parentID));
        printw("Node Name Length: %d\n", BIG_ENDIAN_SHORT(ck->nodeName.length));

        for (i = 0; i < BIG_ENDIAN_SHORT(ck->nodeName.length); i++){
            unicode[i] = BIG_ENDIAN_SHORT(ck->nodeName.unicode[i-1]);
        }
        unicode[i] = 0;

        printw("Unicode: %ls\n", unicode);

        switch (input) {
        case 10:
            break;
        case KEY_RIGHT:
            if (select < records)
                select ++;
            break;
        case KEY_LEFT:
            if (select > 1)
                select --;
            break;
        }
        input = getch();
    }
    clear();
}

void LeafNode_Print(size_t start, int records, int next){
    size_t dir = start;
    size_t offsetDir = dir + _NodeSize;
    wchar_t unicode[255];
    int offset;
    unsigned char select = 1;

    HFSPlusCatalogKey *ck;
    int i;
    int input = getch();
    clear();

    while(input != 'q'){
        move(0, 0);
        offset = BIG_ENDIAN_SHORT(*(int*)(offsetDir - select*2));
        ck = (HFSPlusCatalogKey*) (dir + offset);
        printw("OD: %lu\n", (offsetDir - (select*2) - (size_t)_Map));
        printw("select: %d\n", select);
        printw("Offset: %x\n", offset);
        printw("Key Length: %u\n", BIG_ENDIAN_SHORT(ck->keyLength));
        printw("Parent ID: %lu\n", BIG_ENDIAN_LONG(ck->parentID));
        printw("Node Name Length: %d\n", BIG_ENDIAN_SHORT(ck->nodeName.length));

        for (i = 0; i < BIG_ENDIAN_SHORT(ck->nodeName.length); i++){
            unicode[i] = BIG_ENDIAN_SHORT(ck->nodeName.unicode[i-1]);
        }
        unicode[i] = 0;

        printw("Unicode: %ls\n", unicode);

        switch (input) {
        case 10:
            break;
        case KEY_RIGHT:
            if (select < records)
                select ++;
            break;
        case KEY_LEFT:
            if (select > 1)
                select --;
            break;
        case '1':
            NodeDescriptor_Print(next * _NodeSize + 0xb01e00 + _Map);
        }
        input = getch();
    }
    clear();
}
