#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define BMP_HEADER_SIZE 54
#define MAX_FILENAME 256
#define MAX_FILES 128
#define METADATA_SIZE (MAX_FILES * (MAX_FILENAME + sizeof(size_t) + sizeof(off_t)))

// Representa um arquivo oculto no BMP
struct FileEntry {
    char name[MAX_FILENAME]; // Nome do arquivo
    size_t size;             // Tamanho do arquivo
    off_t offset;            // Offset para os dados no BMP
};

// Definição de armazenamento no sistema de arquivos
struct StegFS {
    int bmp_fd;                  // Descritor de arquivo BMP
    off_t data_start;            // Início dos dados no BMP
    size_t data_size;            // Tamanho dos dados disponíveis
    int file_count;              // Número de arquivos ocultos
    struct FileEntry files[MAX_FILES]; // Metadados dos arquivos ocultos
};
static struct StegFS steg_fs;

int load_bmp_metadata(const char *bmp_path) {
    int i;

    steg_fs.bmp_fd = open(bmp_path, O_RDWR);
    if (steg_fs.bmp_fd < 0) {
        perror("Erro ao abrir o arquivo BMP");
        return -errno;
    }

    char header[BMP_HEADER_SIZE];
    ssize_t read_bytes = read(steg_fs.bmp_fd, header, BMP_HEADER_SIZE);
    if (read_bytes != BMP_HEADER_SIZE) {
        fprintf(stderr, "Erro ao ler o cabeçalho BMP: esperados %d bytes, lidos %zd\n", BMP_HEADER_SIZE, read_bytes);
        close(steg_fs.bmp_fd);
        return -EINVAL;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        fprintf(stderr, "Erro: o arquivo fornecido não é um BMP válido.\n");
        close(steg_fs.bmp_fd);
        return -EINVAL;
    }

    steg_fs.data_start = *(int *)&header[10];
    steg_fs.data_size = lseek(steg_fs.bmp_fd, 0, SEEK_END) - steg_fs.data_start;

    if (steg_fs.data_size <= METADATA_SIZE) {
        fprintf(stderr, "Erro: espaço insuficiente no BMP para esteganografia.\n");
        close(steg_fs.bmp_fd);
        return -EINVAL;
    }

    memset(&steg_fs.files, 0, sizeof(steg_fs.files));

    lseek(steg_fs.bmp_fd, steg_fs.data_start, SEEK_SET);
    ssize_t meta_read = read(steg_fs.bmp_fd, steg_fs.files, METADATA_SIZE);
    if (meta_read < 0) {
        perror("Erro ao ler metadados");
        close(steg_fs.bmp_fd);
        return -EIO;
    }

    steg_fs.file_count = 0;
    for (i = 0; i < MAX_FILES; i++) {
        if (steg_fs.files[i].name[0] != '\0') {
            if (strlen(steg_fs.files[i].name) >= MAX_FILENAME || steg_fs.files[i].size <= 0 ||
                steg_fs.files[i].offset < steg_fs.data_start || 
                steg_fs.files[i].offset + steg_fs.files[i].size > steg_fs.data_start + steg_fs.data_size) {
                fprintf(stderr, "Metadado inválido encontrado: ignorando entrada %d\n", i);
                memset(&steg_fs.files[i], 0, sizeof(struct FileEntry));
                continue;
            }

            steg_fs.file_count++;
        }
    }

    printf("BMP válido: início dos dados = %ld, tamanho utilizável = %ld bytes\n", steg_fs.data_start, steg_fs.data_size);

    for (i = 0; i < steg_fs.file_count; i++) {
        printf("Arquivo %d: %s, tamanho %ld bytes, offset %ld\n",
               i, steg_fs.files[i].name, steg_fs.files[i].size, steg_fs.files[i].offset);
    }

    return 0;
}


int save_metadata() {
    lseek(steg_fs.bmp_fd, steg_fs.data_start, SEEK_SET);
    ssize_t meta_written = write(steg_fs.bmp_fd, steg_fs.files, METADATA_SIZE);
    if (meta_written != METADATA_SIZE) {
        perror("Erro ao salvar metadados");
        return -EIO;
    }
    
    fsync(steg_fs.bmp_fd);

    printf("Gravando metadados no offset %ld, tamanho %ld bytes\n", steg_fs.data_start, METADATA_SIZE);
    for (int i = 0; i < steg_fs.file_count; i++) {
        printf("Arquivo %d: %s, tamanho %ld bytes, offset %ld\n",
               i, steg_fs.files[i].name, steg_fs.files[i].size, steg_fs.files[i].offset);
    }

    return 0;
}

void list_files() {
    printf("Arquivos ocultos:\n");
    for (int i = 0; i < steg_fs.file_count; i++) {
        printf("- %s (%ld bytes)\n", steg_fs.files[i].name, steg_fs.files[i].size);
    }
}

int read_file(const char *filename) {
    for (int i = 0; i < steg_fs.file_count; i++) {
        if (strcmp(filename, steg_fs.files[i].name) == 0) {
            char *buffer = malloc(steg_fs.files[i].size);
            if (!buffer) return -ENOMEM;

            lseek(steg_fs.bmp_fd, steg_fs.files[i].offset, SEEK_SET);
            ssize_t bytes_read = read(steg_fs.bmp_fd, buffer, steg_fs.files[i].size);
            if (bytes_read != steg_fs.files[i].size) {
                fprintf(stderr, "Erro ao ler o arquivo '%s': lidos %zd bytes de %ld esperados\n", filename, bytes_read, steg_fs.files[i].size);
                free(buffer);
                return -EIO;
            }

            write(STDOUT_FILENO, buffer, steg_fs.files[i].size);
            free(buffer);
            return 0;
        }
    }
    fprintf(stderr, "Erro: arquivo '%s' não encontrado.\n", filename);
    return -ENOENT;
}

int write_file(const char *filename, const char *content) {
    size_t content_size = strlen(content);

    // Verifique se já existe um arquivo com o mesmo nome
    for (int i = 0; i < steg_fs.file_count; i++) {
        if (strcmp(steg_fs.files[i].name, filename) == 0) {
            fprintf(stderr, "Erro: arquivo '%s' já existe.\n", filename);
            return -EEXIST;
        }
    }

    // Encontre espaço livre
    off_t offset = steg_fs.data_start + METADATA_SIZE;
    for (int i = 0; i < steg_fs.file_count; i++) {
        off_t end = steg_fs.files[i].offset + steg_fs.files[i].size;
        if (end > offset) {
            offset = end;
        }
    }

    // Verifique se há espaço suficiente
    if (offset + content_size > steg_fs.data_start + steg_fs.data_size) {
        fprintf(stderr, "Erro: espaço insuficiente no BMP para gravar o arquivo.\n");
        return -ENOSPC;
    }

    // Adicione o novo arquivo aos metadados
    struct FileEntry *new_file = &steg_fs.files[steg_fs.file_count];
    strncpy(new_file->name, filename, MAX_FILENAME - 1);
    new_file->size = content_size;
    new_file->offset = offset;
    steg_fs.file_count++;

    // Grava o conteúdo no BMP
    lseek(steg_fs.bmp_fd, offset, SEEK_SET);
    write(steg_fs.bmp_fd, content, content_size);

    // Salva os metadados atualizados
    save_metadata();

    printf("Arquivo '%s' gravado com sucesso.\n", filename);
    return 0;
}

int delete_file(const char *filename) {
    // Procura o arquivo nos metadados
    int i;
    for (i = 0; i < steg_fs.file_count; i++) {
        if (strcmp(steg_fs.files[i].name, filename) == 0) {
            // Arquivo encontrado, agora vamos "apagar" a entrada dele
            memset(&steg_fs.files[i], 0, sizeof(struct FileEntry));

            // Recalcula o número de arquivos
            steg_fs.file_count--;

            // Atualiza os metadados no BMP
            if (save_metadata() != 0) {
                fprintf(stderr, "Erro ao salvar os metadados após a exclusão do arquivo.\n");
                return -EIO;
            }

            printf("Arquivo '%s' apagado com sucesso.\n", filename);
            return 0;
        }
    }

    fprintf(stderr, "Erro: arquivo '%s' não encontrado.\n", filename);
    return -ENOENT;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <BMP file> <comando> [args...]\n", argv[0]);
        fprintf(stderr, "Comandos disponíveis:\n");
        fprintf(stderr, "  ls                      - Lista arquivos ocultos\n");
        fprintf(stderr, "  cat <arquivo>           - Exibe o conteúdo de um arquivo oculto\n");
        fprintf(stderr, "  write <arquivo> <texto> - Escreve um arquivo oculto\n");
        fprintf(stderr, "  delete <arquivo>        - Apaga um arquivo oculto\n");  // Novo comando
        return 1;
    }

    const char *bmp_path = argv[1];
    const char *command = argv[2];

    if (load_bmp_metadata(bmp_path) != 0) {
        return 1;
    }

    if (strcmp(command, "ls") == 0) {
        list_files();
    } else if (strcmp(command, "cat") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Uso: %s cat <arquivo>\n", argv[0]);
            return 1;
        }
        if (read_file(argv[3]) != 0) {
            return 1;
        }
    } else if (strcmp(command, "write") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Uso: %s write <arquivo> <texto>\n", argv[0]);
            return 1;
        }
        if (write_file(argv[3], argv[4]) != 0) {
            return 1;
        }
    } else if (strcmp(command, "delete") == 0) {  // Novo comando
        if (argc < 4) {
            fprintf(stderr, "Uso: %s delete <arquivo>\n", argv[0]);
            return 1;
        }
        if (delete_file(argv[3]) != 0) {
            return 1;
        }
    } else {
        fprintf(stderr, "Comando desconhecido: %s\n", command);
        return 1;
    }

    close(steg_fs.bmp_fd);
    return 0;
}
