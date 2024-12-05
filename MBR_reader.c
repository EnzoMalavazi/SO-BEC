#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define MBR_TAM 512 
#define PART_TABLE 446 
#define BOOT_SIG 510

typedef struct {
    uint8_t boot_flag;      // (0x80 = ativa, 0x00 = inativa)
    uint8_t chs_start[3]; 
    uint8_t type;         
    uint8_t chs_end[3];     
    uint32_t lba_start;     
    uint32_t sector_count;  
} PartEntry;

void print_partition_info(PartEntry *entry, int index) {
    printf("Partição %d:\n", index + 1);
    printf("  Ativa: %s\n", (entry->boot_flag == 0x80) ? "Sim" : "Não");
    printf("  Tipo: 0x%02X\n", entry->type);
    printf("  LBA Inicial: %u\n", entry->lba_start);
    printf("  Setores: %u\n", entry->sector_count);
    printf("  Tamanho: %u MB\n", (entry->sector_count * 512) / (1024 * 1024));
    printf("\n");
}

int main(int argc, char *argv[]) {
    FILE *file = fopen(argv[1], "rb");
    
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_mbr>\n", argv[0]);
        return 1;
    }
    uint8_t mbr[MBR_TAM];
    if (fread(mbr, 1, MBR_TAM, file) != MBR_TAM) {
        fprintf(stderr, "Erro ao ler o arquivo MBR\n");
        fclose(file);
        return 1;
    }
    
    if (!file) {
        perror("Erro ao abrir");
        return 1;
    }
    fclose(file);

    // Verifica assinatura 
    if (mbr[BOOT_SIG] != 0x55 || mbr[BOOT_SIG + 1] != 0xAA) {
        fprintf(stderr, "Erro, não tem MBR válida.\n");
        return 1;
    }

    printf("Assinatura de Boot: 0x%02X%02X (válida)\n\n", mbr[BOOT_SIG], mbr[BOOT_SIG + 1]);

    // Lê a tabela de partições
    PartEntry *part_table = (PartEntry *)(mbr + PART_TABLE);
    for (int i = 0; i < 4; i++) {
        print_partition_info(&part_table [i], i);
    }

    return 0;
}
