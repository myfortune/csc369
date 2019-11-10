#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "traffic.h"

extern struct intersection isection;
extern struct car *in_cars[];
extern struct car *out_cars[];

/**
 * Populate the car lists by parsing a file where each line has
 * the following structure:
 *
 * <id> <in_direction> <out_direction>
 *
 * Each car is added to the list that corresponds with 
 * its in_direction
 * 
 * Note: this also updates 'inc' on each of the lanes
 */
void parse_schedule(char *file_name) {
    int id;
    struct car *cur_car;
    enum direction in_dir, out_dir;
    FILE *f = fopen(file_name, "r");

    /* parse file */
    while (fscanf(f, "%d %d %d", &id, (int*)&in_dir, (int*)&out_dir) == 3) {
        /* construct car */
        cur_car = malloc(sizeof(struct car));
        cur_car->id = id;
        cur_car->in_dir = in_dir;
        cur_car->out_dir = out_dir;

        /* append new car to head of corresponding list */
        cur_car->next = in_cars[in_dir];
        in_cars[in_dir] = cur_car;
        isection.lanes[in_dir].inc++;
    }

    fclose(f);
}

/**
 *
 * Do all of the work required to prepare the intersection
 * before any cars start coming
 * 
 */
void init_intersection() {
    int i;
    for (i = 0; i < 4; i++) {
        pthread_mutex_init(&isection.quad[i], NULL);
    }
    init_lane();
}

/**
 * initialize all four lanes
 */
void init_lane(){
    int i;
    for (i = 0; i < 4; i++) {
        struct lane *l = &isection.lanes[i];
        pthread_mutex_init(&l->lock, NULL);
        pthread_cond_init(&l->producer_cv, NULL);
        pthread_cond_init(&l->consumer_cv, NULL);

        l->inc = 0;
        l->passed = 0;

        // initialize buffer
        int j;
        l->buffer = malloc(LANE_LENGTH * sizeof(struct car *));
        for (j = 0; j < LANE_LENGTH; j++){
            l->buffer[j] = NULL;
        }

        l->head = 0;
        l->tail = -1;
        l->capacity = LANE_LENGTH;
        l->in_buf = 0;
    }
}

/**
 *
 * Populates the corresponding lane with cars as room becomes
 * available. Ensure to notify the cross thread as new cars are
 * added to the lane.
 * 
 */
void *car_arrive(void *arg) {
    struct lane *l = &isection.lanes[*(int*) arg];
    pthread_mutex_lock(&l->lock);

    int counter = l->inc;
    while (counter > 0) {
        while (l->in_buf == l->capacity) { // buffer is full
            pthread_cond_wait(&l->producer_cv, &l->lock);
        }

        // finds the first car to be added
        int i;
        struct car *first = in_cars[*(int*) arg];
        for (i = 1; i < counter; i++) {
            first = first->next;
        }

        // add the new car to buffer
        l->tail = (l->tail + 1) % l->capacity;
        l->buffer[l->tail] = first;
        l->in_buf++;
        counter--;

        //then notify the cross thread that a new car is added
        pthread_cond_signal(&l->consumer_cv);   
    }
    pthread_mutex_unlock(&l->lock);

    return NULL;
}

/**
 *
 * Moves cars from a single lane across the intersection. Cars
 * crossing the intersection must abide the rules of the road
 * and cross along the correct path. Ensure to notify the
 * arrival thread as room becomes available in the lane.
 *
 * Note: After crossing the intersection the car should be added
 * to the out_cars list that corresponds to the car's out_dir
 * 
 * Note: For testing purposes, each car which gets to cross the 
 * intersection should print the following three numbers on a 
 * new line, separated by spaces:
 *  - the car's 'in' direction, 'out' direction, and id.
 * 
 * You may add other print statements, but in the end, please 
 * make sure to clear any prints other than the one specified above, 
 * before submitting your final code. 
 */
void *car_cross(void *arg) {
    struct lane *l = &isection.lanes[*(int*) arg];
    pthread_mutex_lock(&l->lock);

    while (l->passed < l->inc) {
        while (l->in_buf == 0) { // buffer is empty
            pthread_cond_wait(&l->consumer_cv, &l->lock);
        }
        int num_lock = 0, i, k, j;

        struct car *curr = l->buffer[l->head];
        int in_dir = curr->in_dir;
        int out_dir = curr->out_dir;

        int *path = compute_path(in_dir, out_dir);

        // finds number of quadrants needed for this car to cross
        // int 5 is placeholder for paths not long enough
        for (i = 0; i < 3; i++) {
            if (path[i] != 5) {
                num_lock++;
            }
        }

        // acquires locks for required quadrants
        for (k = 0; k < num_lock; k++){
            pthread_mutex_lock(&isection.quad[path[k]]);
        }

        // updates counters
        l->head = (l->head + 1) % l->capacity;
        l->passed++;
        l->in_buf--;

        // adds to out_car
        curr->next = out_cars[out_dir];
        out_cars[out_dir] = curr;
        printf("%d %d %d\n", in_dir, out_dir, curr->id);

        for (j = 0; j < num_lock; j++) {
            pthread_mutex_unlock(&isection.quad[path[j]]);
        }

        free(path);
        free(curr);

        // notifies arrival thread that room is available
        pthread_cond_signal(&l->producer_cv);
    }
    pthread_mutex_unlock(&l->lock);

    free(l->buffer);

    return NULL;
}

/**
 *
 * Given a car's in_dir and out_dir return a sorted 
 * list of the quadrants the car will pass through.
 * 
 */
int *compute_path(enum direction in_dir, enum direction out_dir) {
    int *path = malloc(3 * sizeof(int));

    if (in_dir == 0) {
        if (out_dir == 0) {//return [1, 2]
            path[0] = 0;
            path[1] = 1;
            path[2] = 5;
        } else if (out_dir == 1) {//return [2, 3]
            path[0] = 1;
            path[1] = 2;
            path[2] = 5;
        } else if (out_dir == 2) {//return [2, 3, 4]
            path[0] = 1;
            path[1] = 2;
            path[2] = 3;
        } else if (out_dir == 3) {//return [2]
            path[0] = 1;
            path[1] = 5;
            path[2] = 5;
        }
    } else if (in_dir == 1) {
        if (out_dir == 0) {//return [1, 4]
            path[0] = 0;
            path[1] = 3;
            path[2] = 5;
        } else if (out_dir == 1) {//return [3, 4]
            path[0] = 2;
            path[1] = 3;
            path[2] = 5;
        } else if (out_dir == 2) {//return [4]
            path[0] = 3;
            path[1] = 5;
            path[2] = 5;
        } else if (out_dir == 3) {//return [1, 2, 4]
            path[0] = 0;
            path[1] = 1;
            path[2] = 3;
        }
    } else if (in_dir == 2) {
        if (out_dir == 0) {//return [1]
            path[0] = 0;
            path[1] = 5;
            path[2] = 5;
        } else if (out_dir == 1) {//return [1, 2, 3]
            path[0] = 0;
            path[1] = 1;
            path[2] = 2;
        } else if (out_dir == 2) {//return [1, 4]
            path[0] = 0;
            path[1] = 3;
            path[2] = 5;
        } else if (out_dir == 3) {//return [1, 2]
            path[0] = 0;
            path[1] = 1;
            path[2] = 5;
        }
    } else if (in_dir == 3) {
        if (out_dir == 0) {//return [1, 3, 4]
            path[0] = 0;
            path[1] = 2;
            path[2] = 3;
        } else if (out_dir == 1) {//return [3]
            path[0] = 2;
            path[1] = 5;
            path[2] = 5;
        } else if (out_dir == 2) {//return [3, 4]
            path[0] = 2;
            path[1] = 3;
            path[2] = 5;
        } else if (out_dir == 3) {//return [2, 3]
            path[0] = 1;
            path[1] = 2;
            path[2] = 5;
        }
    } else {
        path = NULL;
    }
    return path;
}