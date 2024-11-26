#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MBR_SIZE 512
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_TABLE_OFFSET 446
#define BOOT_SIGNATURE_OFFSET 510

typedef struct {
    uint8_t boot_flag;      // Indicador de boot (0x80 = ativa, 0x00 = inativa)
    uint8_t chs_start[3];   // CHS de início
    uint8_t type;           // Tipo da partição
    uint8_t chs_end[3];     // CHS de fim
    uint32_t lba_start;     // LBA do início da partição
    uint32_t sector_count;  // Número de setores da partição
} PartitionEntry;

void print_partition_info(PartitionEntry *entry, int index) {
    printf("Partição %d:\n", index + 1);
    printf("  Ativa: %s\n", (entry->boot_flag == 0x80) ? "Sim" : "Não");
    printf("  Tipo: 0x%02X\n", entry->type);
    printf("  LBA Inicial: %u\n", entry->lba_start);
    printf("  Setores: %u\n", entry->sector_count);
    printf("  Tamanho: %u MB\n", (entry->sector_count * 512) / (1024 * 1024));
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_mbr>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Erro ao abrir arquivo");
        return 1;
    }

    uint8_t mbr[MBR_SIZE];
    if (fread(mbr, 1, MBR_SIZE, file) != MBR_SIZE) {
        fprintf(stderr, "Erro ao ler o arquivo MBR\n");
        fclose(file);
        return 1;
    }
    fclose(file);

    // Verifica a assinatura de boot
    if (mbr[BOOT_SIGNATURE_OFFSET] != 0x55 || mbr[BOOT_SIGNATURE_OFFSET + 1] != 0xAA) {
        fprintf(stderr, "O arquivo não contém uma MBR válida.\n");
        return 1;
    }

    printf("Assinatura de Boot: 0x%02X%02X (válida)\n\n", mbr[BOOT_SIGNATURE_OFFSET], mbr[BOOT_SIGNATURE_OFFSET + 1]);

    // Lê a tabela de partições
    PartitionEntry *partition_table = (PartitionEntry *)(mbr + PARTITION_TABLE_OFFSET);
    for (int i = 0; i < 4; i++) {
        print_partition_info(&partition_table[i], i);
    }

    return 0;
}
