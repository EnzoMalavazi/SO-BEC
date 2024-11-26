#include <stdio.h>
#include <stddef.h>

#define MEMORY_POOL_SIZE 1048576

typedef struct {
    char mem_pool[MEMORY_POOL_SIZE]; // Pool de memória
    char* place;                    // Ponteiro para a próxima posição livre
} Allocator;

// Inicializa o alocador
void aloca_init(Allocator* aloca) {
      aloca->place = aloca->mem_pool;
}

// Aloca um bloco de memória
void* alocar(Allocator* aloca, size_t size) {
    if ((aloca->place + size) > (aloca->mem_pool + MEMORY_POOL_SIZE)) {
        // Sem espaço suficiente no pool de memória
        return NULL;
    }
    void* ret = aloca->place;
      aloca->place += size;

    // Imprime o endereço alocado
    printf("Alocado %zu bytes em: %p\n", size, ret);

    return ret;
}

// "Reseta" o alocador para reutilizar a memória
void desaloca(Allocator* aloca) {
      aloca->place = aloca->mem_pool;
    printf("Alocador resetado. Memória disponível novamente.\n");
}

int main() {
    // Instancia e inicializa o alocador
    Allocator aloca;
    aloca_init(&aloca);

    // Aloca memória para diferentes tamanhos
    char* block1 = (char*)alocar(&aloca, 100); // 100 bytes
    int* block2 = (int*)alocar(&aloca, 200);  // 200 bytes

    if (block1 && block2) {
        printf("Memória alocada com sucesso!\n");
    } else {
        printf("Falha na alocação de memória.\n");
    }

    // Resetando o alocador para reutilizar o espaço
      desaloca(&aloca);

    return 0;
}
