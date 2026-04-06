#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define CLOCK_RATE 512000000.0

unsigned long long aimos_clock_read(void)
{
    unsigned int tbl, tbu0, tbu1;
    do {
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu0));
        __asm__ __volatile__ ("mftb %0" : "=r"(tbl));
        __asm__ __volatile__ ("mftbu %0" : "=r"(tbu1));
    } while (tbu0 != tbu1);

    return (((unsigned long long)tbu0) << 32) | tbl;
}

// Fill buffer with 1s
void init_buffer(char *buffer, size_t size) {
    memset(buffer, 1, size);
}

// run write and read test
void run_test(const char *filename, int rank, int size, size_t block_size, int num_blocks) {
    MPI_File file;
    MPI_Status status;
    char *buffer = (char *)malloc(block_size);

    init_buffer(buffer, block_size);

    MPI_Offset offset;
    unsigned long long start, end;
    double time_sec, bandwidth;
    double total_data = (double) size * num_blocks * block_size / (1024.0 * 1024.0);

    // write
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &file);
    MPI_Barrier(MPI_COMM_WORLD);
    start = aimos_clock_read();

    for (int b = 0; b < num_blocks; b++) {
        offset = ((MPI_Offset)b * size + rank) * block_size;
        MPI_File_write_at(file, offset, buffer, block_size, MPI_BYTE, &status);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = aimos_clock_read();

    MPI_File_close(&file);

    time_sec = (end - start) / CLOCK_RATE;
    bandwidth = total_data / time_sec;

    if (rank == 0) {
        printf("[WRITE] Block Size: %zu KB | Ranks: %d | Time: %.6f s | BW: %.2f MB/s\n", block_size / 1024, size, time_sec, bandwidth);
    }

    // read
    MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    MPI_Barrier(MPI_COMM_WORLD);
    start = aimos_clock_read();

    for (int b = 0; b < num_blocks; b++) {
        offset = ((MPI_Offset)b * size + rank) * block_size;
        MPI_File_read_at(file, offset, buffer, block_size, MPI_BYTE, &status);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end = aimos_clock_read();

    MPI_File_close(&file);

    time_sec = (end - start) / CLOCK_RATE;
    bandwidth = total_data / time_sec;

    if (rank == 0) {
        printf("[READ ] Block Size: %zu KB | Ranks: %d | Time: %.6f s | BW: %.2f MB/s\n", block_size / 1024, size, time_sec, bandwidth);
    }
    free(buffer);
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Block sizes (bytes)
    size_t block_sizes[] = {
        1 << 20,
        2 << 20,
        4 << 20,
        8 << 20,
        16 << 20
    };

    int num_block_sizes = 5;
    int num_blocks = 32;

    char filename[256];

    for (int i = 0; i < num_block_sizes; i++) {

        size_t block_size = block_sizes[i];

        // ===== CHANGE PATH HERE =====
        // For scratch:
        sprintf(filename, "testfile_%zu.dat", block_size);

        // For NVMe (example):
        // sprintf(filename, "/mnt/nvme/.../testfile_%zu.dat", block_size);

        if (rank == 0) {
            printf("\n=== Running test: Block Size = %zu KB ===\n", block_size / 1024);
        }

        run_test(filename, rank, size, block_size, num_blocks);

        MPI_Barrier(MPI_COMM_WORLD);

        // Delete file after test
        if (rank == 0) {
            remove(filename);
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}