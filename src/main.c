#include <dlfcn.h>
#include <link.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <curses.h>

#define BUFFER_SIZE 0x1000

typedef void* pointer;
typedef unsigned char byte;

typedef struct buffer_s
{
    pointer addr;
    size_t size;
} buffer_t;

void free_buffer(buffer_t* buf)
{
    if (buf->addr != NULL)
    {
        free(buf->addr);
        buf->size = 0;
    }
}

bool alloc_buffer(size_t size, buffer_t* output_buf)
{
    memset(output_buf, 0, sizeof(buffer_t));

    output_buf->addr = malloc(size);

    if (output_buf->addr != NULL)
    {
        output_buf->size = size;
        return true;
    }

    return false;
}

bool decompress_buffer(buffer_t* input_buf, buffer_t* output_buf)
{
    size_t i;
    size_t count_same_bytes;
    byte current_byte;

    memset(output_buf, 0, sizeof(buffer_t));

    for (i = 0; i < input_buf->size; i += sizeof(count_same_bytes) + 1)
    {
        count_same_bytes = *(size_t*)((uintptr_t)input_buf->addr + i);
        current_byte = *(byte*)((uintptr_t)input_buf->addr + i +
                                sizeof(count_same_bytes));

        if (output_buf->addr == NULL)
        {
            output_buf->addr = malloc(count_same_bytes);
        }
        else
        {
            output_buf->addr = realloc(output_buf->addr,
                                       output_buf->size + count_same_bytes);

            if (output_buf->addr == NULL)
            {
                output_buf->size = 0;
                return false;
            }
        }

        memset((pointer)((uintptr_t)output_buf->addr + output_buf->size),
               *(int*)&current_byte,
               count_same_bytes);

        output_buf->size += count_same_bytes;
    }

    return true;
}

bool compress_buffer(buffer_t* input_buf, buffer_t* output_buf)
{
    byte *current_byte, *next_byte;
    size_t i, j;
    size_t count_same_bytes;
    size_t old_size;

    memset(output_buf, 0, sizeof(buffer_t));

    for (i = 0; i < input_buf->size; i++)
    {
        current_byte = (byte*)((uintptr_t)input_buf->addr + i);
        count_same_bytes = 0;

        for (j = i + 1; j < input_buf->size; j++)
        {
            next_byte = (byte*)((uintptr_t)input_buf->addr + j);

            if (*next_byte == *current_byte)
            {
                count_same_bytes++;
            }
            else
            {
                break;
            }
        }

        // Skip the bytes if we found some.
        i += count_same_bytes;

        old_size = output_buf->size;

        // Number of the same bytes + the byte.
        output_buf->size += sizeof(count_same_bytes) + 1;

        if (output_buf->addr == NULL)
        {
            output_buf->addr = malloc(output_buf->size);
        }
        else
        {
            output_buf->addr = realloc(output_buf->addr, output_buf->size);

            if (output_buf->addr == NULL)
            {
                return false;
            }
        }

        *(size_t*)((uintptr_t)output_buf->addr + old_size) = count_same_bytes +
                                                             1;
        *(byte*)((uintptr_t)output_buf->addr + old_size +
                 sizeof(count_same_bytes)) = *current_byte;
    }

    return true;
}

int main()
{
    buffer_t test, compressed, decompressed;
    size_t i, j;
    byte rand_byte;

    srand(time(0));

    if (!alloc_buffer(BUFFER_SIZE, &test))
    {
        printf("couldn't allocate test buffer\n");
        return 0;
    }


    for (i = 0; i < BUFFER_SIZE; i += 0x10)
    {
        rand_byte = rand() % 0x100;

        for (j = 0; j < 0x10; j++)
            *(byte*)((uintptr_t)test.addr + j + i) = rand_byte;
    }

    printf("\ntest:\n");

    for (i = 0; i < BUFFER_SIZE; i++)
    {
        if (i % 32 == 0)
            printf("\n");
        printf("%02X", *(byte*)((uintptr_t)test.addr + i));
    }

    printf("\n");

    if (!compress_buffer(&test, &compressed))
    {
        printf("couldn't compress test buffer\n");
        return 0;
    }

    printf("\ncompressed:\n");

    for (i = 0; i < compressed.size; i++)
    {
        if (i % 32 == 0)
            printf("\n");
        printf("%02X", *(byte*)((uintptr_t)compressed.addr + i));
    }

    printf("\n");

    if (!decompress_buffer(&compressed, &decompressed))
    {
        printf("couldn't decompress test buffer\n");
        return 0;
    }

    printf("\ndecompressed:\n");
    for (i = 0; i < decompressed.size; i++)
    {
        if (i % 32 == 0)
            printf("\n");
        printf("%02X", *(byte*)((uintptr_t)decompressed.addr + i));
    }

    printf("\n");

    free_buffer(&test);
    free_buffer(&compressed);
    free_buffer(&decompressed);

    return 0;
}
