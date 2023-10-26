// Author: APD team, except where source was noted

#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define CONTOUR_CONFIG_COUNT    16
#define FILENAME_MAX_SIZE       50
#define STEP                    8
#define SIGMA                   200
#define RESCALE_X               2048
#define RESCALE_Y               2048

#define CLAMP(v, min, max) if(v < min) { v = min; } else if(v > max) { v = max; }

// find min function of 2 numbers
// used for [start, end] interval (for each thread) 
int min(int a, int b) {
    return a < b ? a : b;
}

// structure that holds all the "independent" information needed for each thread
// it s the same for all threads
typedef struct info {
    ppm_image **contour_map;
    ppm_image **image;
    ppm_image **new_image;
    int step_x;
    int step_y;
    unsigned char sigma;
    unsigned char **grid;
    int N;
    int flag_rescale;
    pthread_barrier_t barrier;

} info;

// structure that holds the "dependend" information needed for each thread
// it s different for each thread (just the id for this homework)
typedef struct thread_arg {
    info *details;
    int id;
} thread_arg;


// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
// !! MODIFIED LOCATION OF CONTOURS !!
ppm_image **init_contour_map() {
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "../checker/contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y) {
    for (int i = 0; i < contour->x; i++) {
        for (int j = 0; j < contour->y; j++) {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

void *sample_grid_thread(void *arg) {
    
    // 1. Extract arguments
    thread_arg *argument = (thread_arg *)arg;
    info *info_thread = argument->details;
    int thread_id = argument->id;

    // 2. Check if we need to rescale image
    if(info_thread->flag_rescale) {
        uint8_t sample[3];

        int total_number = (*info_thread->new_image)->x;
        int start = thread_id * (double)total_number / info_thread->N;
        int end = min((thread_id + 1) * (double)total_number / info_thread->N, total_number);

        // use bicubic interpolation for scaling
        for (int i = start; i < end; i++) {
            for (int j = 0; j < (*info_thread->new_image)->y; j++) {
                float u = (float)i / (float)((*info_thread->new_image)->x - 1);
                float v = (float)j / (float)((*info_thread->new_image)->y - 1);
                sample_bicubic(*info_thread->image, u, v, sample);

                (*info_thread->new_image)->data[i * (*info_thread->new_image)->y + j].red = sample[0];
                (*info_thread->new_image)->data[i * (*info_thread->new_image)->y + j].green = sample[1];
                (*info_thread->new_image)->data[i * (*info_thread->new_image)->y + j].blue = sample[2];
            }
        }

         // wait for all threads to rescale image
        pthread_barrier_wait(&info_thread->barrier);

        // thread 0 update the new image
        if(!thread_id) {
            free((*info_thread->image)->data);
            free((*info_thread->image));

            *info_thread->image = *info_thread->new_image;
        }
        
    }

     // wait for thread 0 to switch image
    pthread_barrier_wait(&info_thread->barrier);

    // 3. Sample grid
    int p = (*info_thread->image)->x / info_thread->step_x;
    int q = (*info_thread->image)->y / info_thread->step_y;
    int N = info_thread->N;

    // set interval for each thread
    int start = thread_id * (double)(p + 1) / N;
    int end = min((thread_id + 1) * (double)(p + 1) / N, (p + 1));

    // alloc memory for grid
    for (int i = start; i < end; i++) {
        info_thread->grid[i] = (unsigned char *)calloc(q + 1, sizeof(unsigned char));
        if (!info_thread->grid[i]) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

     // wait for all threads to allocate memory
    pthread_barrier_wait(&info_thread->barrier);

    start = thread_id * (double)p / N;
    end = min((thread_id + 1) * (double)p / N, p);

    // sample grid function
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            ppm_pixel curr_pixel = (*info_thread->image)->data[i * info_thread->step_x * (*info_thread->image)->y + j * info_thread->step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > info_thread->sigma) {
                info_thread->grid[i][j] = 0;
            } else {
                info_thread->grid[i][j] = 1;
            }
        }
    }

    // sample grid function
    for (int i = start; i < end; i++) {
        ppm_pixel curr_pixel = (*info_thread->image)->data[i * info_thread->step_x * (*info_thread->image)->y + (*info_thread->image)->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > info_thread->sigma) {
            info_thread->grid[i][q] = 0;
        } else {
            info_thread->grid[i][q] = 1;
        }
    }

    // pthread_barrier_wait(&barrier); // wait for all threads to finish sampling
    
    int start2 = thread_id * (double)q / N;
    int end2 = min((thread_id + 1) * (double)q / N, q);

    // sample grid function
    for (int j = start2; j < end2; j++) {
        ppm_pixel curr_pixel = (*info_thread->image)->data[((*info_thread->image)->x - 1) * (*info_thread->image)->y + j * info_thread->step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > info_thread->sigma) {
            info_thread->grid[p][j] = 0;
        } else {
            info_thread->grid[p][j] = 1;
        }
    }

    // 4. March function
    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * info_thread->grid[i][j] + 4 * info_thread->grid[i][j + 1] + 2 * info_thread->grid[i + 1][j + 1] + 1 * info_thread->grid[i + 1][j];
            update_image(*info_thread->image, info_thread->contour_map[k], i * info_thread->step_x, j * info_thread->step_y);
        }
    }
    
    pthread_exit(NULL);
}

// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x) {
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    for (int i = 0; i <= image->x / step_x; i++) {
        free(grid[i]);
    }
    free(grid);

    free(image->data);
    free(image);
}

int main(int argc, char *argv[]) {
    
    // 0. Check arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    // 1. Check nr of threads
    int N = atoi(argv[3]);
    if(!N) {
        fprintf(stderr, "Invalid number of threads\n");
        return 1;
    }
    
    // 1.1 Init threads
    pthread_t threads[N];
    thread_arg argument[N];

    // 2. Read input image
    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;
    int step_y = STEP;

    // 3. Initialize contour map
    ppm_image **contour_map = init_contour_map();

    info info_thread;
    info_thread.image = &image;
    info_thread.step_x = step_x;
    info_thread.step_y = step_y;
    info_thread.sigma = SIGMA;
    info_thread.grid = NULL;
    info_thread.N = N;
    info_thread.contour_map = contour_map;
    pthread_barrier_init(&info_thread.barrier, NULL, N);


    // 4. Check for rescale
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y) {
        info_thread.flag_rescale = 0;
        info_thread.new_image = NULL;
        info_thread.grid = malloc((image->x / step_x + 1) * sizeof(unsigned char*));
    } else {
        info_thread.flag_rescale = 1;
        info_thread.grid = malloc((RESCALE_X / step_x + 1) * sizeof(unsigned char*));

        ppm_image *final_image = malloc(sizeof(ppm_image));
        if (!final_image) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
        final_image->x = RESCALE_X;
        final_image->y = RESCALE_Y;
        final_image->data = (ppm_pixel*)malloc(final_image->x * final_image->y * sizeof(ppm_pixel));
        if (!final_image) {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }

        info_thread.new_image = &final_image;
    }

    // 5. Create threads
    for (int i = 0; i < N; i++) {
        argument[i].details = &info_thread;
        argument[i].id = i;

        if (pthread_create(&threads[i], NULL, sample_grid_thread, &argument[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // 6. Wait for threads to finish
    for (int i = 0; i < N; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(1);
        }
    }

    // 7. Write output image
    write_ppm(*info_thread.image, argv[2]);

    // 8. Free resources
    free_resources(*info_thread.image, contour_map, argument[0].details->grid, step_x);
    pthread_barrier_destroy(&info_thread.barrier);

    return 0;
}
