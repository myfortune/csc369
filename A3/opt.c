#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

#define MAXLINE 256

extern int memsize;

extern int debug;

extern struct frame *coremap;

extern char *tracefile;

int next_occurrence(addr_t vaddr, int line_num,1 FILE *file);

char *POSITION_FILE = "pos.txt";
long offset;
typedef struct node {
    int next_pos; // position of next occurrence
    struct node *next;
} Node;

Node *head;
Node *tail;

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
    int i, next_pos, max_next_pos = 0, frame;
	for (i = 0; i < memsize; i++) {
        next_pos = coremap[i].next_pos;
        if (next_pos == -1) { // never occurring again, no need to continue
            frame = i;
            break;
        }
        if (next_pos > max_next_pos) {
            max_next_pos = next_pos;
            frame = i;
        }
    }
    printf("evicted %d\n", frame);
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
    char buff[MAXLINE];
    int next_line_num;
    int frame = p->frame >> PAGE_SHIFT;

    FILE *positions = fopen(POSITION_FILE, "r");
    if (!positions) {
        perror("Error opening POSITION_FILE:");
        exit(1);
    }

    fseek(positions, offset, SEEK_SET);

    fgets(buff, MAXLINE, positions);
    sscanf(buff, "%d", &next_line_num);
    coremap[frame].next_pos = next_line_num;
    printf("next line %d\n", next_line_num);
    offset = ftell(positions);
    fclose(positions);
//    coremap[frame].next_pos = head->next_pos;
//    Node *prev = head;
//
//    if (head->next) {
//        head = head->next;
//    }
//
//    free(prev);
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {
    char buff[MAXLINE];
    char type;
    addr_t vaddr;
    int line_num = 0;
    int next_line_num = 0;
//    head = NULL;
//    tail = NULL;

    FILE *file = fopen(tracefile, "r");
    if (!file) {
        perror("Error opening tracefile:");
        exit(1);
    }

    FILE *positions = fopen(POSITION_FILE, "w");
    if (!positions) {
        perror("Error opening POSITION_FILE:");
        exit(1);
    }

    while (fgets(buff, MAXLINE, file)) {
        if(buff[0] != '=') {
            sscanf(buff, "%c %lx", &type, &vaddr); // read from buf

            long pos = ftell(file);
            next_line_num = next_occurrence(vaddr, line_num, file);
            fseek(file, pos, SEEK_SET); // restore the position
            line_num++;
            fprintf(positions, "%d\n", next_line_num);
            printf("%d", next_line_num);
//            Node *node = malloc(sizeof(Node));
//            node->next_pos = next_line_num;
//            node->next = NULL;
//
//            if (!head) { // first node
//                head = node;
//            } else {
//                tail->next = node;
//            }
//            tail = node;
        }
    }
    fclose(positions);
    fclose(file);
    printf("done init\n");
}

/**
 *
 * @param vaddr the virtual address to search for
 * @param line_num the current line number of this vaddr
 * @return the line number of next occurrence of this vaddr; -1 if not found
 */
int next_occurrence(addr_t vaddr, int line_num, FILE *file) {
    char buff[MAXLINE];
    char next_type;
    addr_t next_vaddr;
    //int next_line_num = 0;

    while (fgets(buff, MAXLINE, file)) {
        if (buff[0] != '=') {
            sscanf(buff, "%c %lx", &next_type, &next_vaddr); // read from buf
            if (next_vaddr == vaddr) {
                //fclose(file);
                return line_num;
            }
            line_num++;
        }
    }
    // didn't find the next occurrence
    //fclose(file);
    return -1;
}
}

