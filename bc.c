#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/stat.h>

#include <elf.h>

char* progname;

/* TODO: switch to only use inc or dec when a single + - < or > is used */

typedef enum {
    PUT   = 0,
    GET   = 1,
    INC   = 2,
    DEC   = 3,
    RIGHT = 4,
    LEFT  = 5,
    WHILE = 6,
    END   = 7,
    ADD   = 8,
    ADDP  = 9, /* multiple right */
    INIT  = 10,
    EXIT  = 11
} instruction;

const char* inst[12] = {
    [EXIT]  = "\xb8\x3c\x00\x00\x00\xbf\x00\x00\x00\x00\x0f\x05",
    [INIT]  = "\xbe\x00\x00\x00\x20\xba\x01\x00\x00\x00",
    [PUT]   = "\xb8\x01\x00\x00\x00\xbf\x01\x00\x00\x00\x0f\x05",
    [GET]   = "\xb8\x00\x00\x00\x00\xbf\x00\x00\x00\x00\x0f\x05",
    [INC]   = "\xfe\x06",
    [DEC]   = "\xfe\x0e",
    [RIGHT] = "\x48\xff\xc6",
    [LEFT]  = "\x48\xff\xce",
    [ADD]   = "\x80\x06X",
    [ADDP]  = "\x81\xc6XXXX",
    [WHILE] = "\x80\x3e\x00\x0f\x84XXXX",
    [END]   = "\x80\x3e\x00\x0f\x85XXXX",
};

size_t inst_size[14] = {
    [EXIT]  = 12,
    [INIT]  = 10,
    [PUT]   = 12,
    [GET]   = 12,
    [INC]   = 2,
    [DEC]   = 2,
    [RIGHT] = 3,
    [LEFT]  = 3,
    [ADD]   = 3,
    [ADDP]  = 6,
    [WHILE] = 9,
    [END]   = 9,
};


void die(char* msg) {
    fprintf(stderr, "%s: %s.\n", progname, msg);
    exit(1);
}

int main(int argc, char** argv) {

    progname = strrchr(argv[0], '/');
    if (progname[0] == '\0' || progname[1] == '\0')
        progname = argv[1];
    else
        progname++;

    if (argc < 3)
        die("not enough args");

    FILE* fp = fopen(argv[1], "rb");

    if (!fp)
        die("unable to open file");

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* file = malloc(length);
    if (!file)
        die("cannot allocate memory");

    if (fread(file, 1, length, fp) != length)
        die("could not read file");

    fclose(fp);

    instruction* inst_arr = malloc(length * sizeof(instruction));
    if (!inst_arr)
        die("cannot allocate memory");
    size_t inst_cnt = 0;

    for (int i = 0; i < length; i++) switch(file[i]) {
        case '-':
            inst_arr[inst_cnt] = DEC;
            inst_cnt++;
            break;
        case '+':
            inst_arr[inst_cnt] = INC;
            inst_cnt++;
            break;
        case '<':
            inst_arr[inst_cnt] = LEFT;
            inst_cnt++;
            break;
        case '>':
            inst_arr[inst_cnt] = RIGHT;
            inst_cnt++;
            break;
        case ',':
            inst_arr[inst_cnt] = GET;
            inst_cnt++;
            break;
        case '.':
            inst_arr[inst_cnt] = PUT;
            inst_cnt++;
            break;
        case '[':
            inst_arr[inst_cnt] = WHILE;
            inst_cnt++;
            break;
        case ']':
            inst_arr[inst_cnt] = END;
            inst_cnt++;
            break;
    }

    free(file);

    inst_arr = realloc(inst_arr, inst_cnt * sizeof(instruction));
    if (!inst_arr)
        die("cannot allocate memory");

    size_t code_size = inst_size[INIT] + inst_size[EXIT];
    for (int i = 0; i < inst_cnt; i++)
        code_size += inst_size[inst_arr[i]];

    uint8_t* code = malloc(code_size);
    memcpy(code, inst[INIT], inst_size[INIT]);
    code_size = inst_size[INIT];

    for (int i = 0; i < inst_cnt; i++) {
        size_t offset;
        switch (inst_arr[i]) {
            case INC:
            case DEC:;
                int8_t addend8 = 0;
                offset = 0;
                while (inst_arr[i + offset] == INC || inst_arr[i + offset] == DEC) {
                    addend8 += inst_arr[i + offset] == INC ? 1 : -1;
                    offset++;
                }
                switch (addend8) {
                    case 0:
                        break;
                    case 1:
                        memcpy(code + code_size, inst[INC], inst_size[INC]);
                        code_size += inst_size[INC];
                        break;
                    case -1:
                        memcpy(code + code_size, inst[DEC], inst_size[DEC]);
                        code_size += inst_size[DEC];
                        break;
                    default:
                        memcpy(code + code_size, inst[ADD], inst_size[ADD]);
                        code_size += inst_size[ADD];
                        /* assumes your compiler signs the same way as asm */
                        *(int8_t*)(code + code_size - 1) = addend8;
                        break;
                }
                i += offset - 1;
                break;
            case RIGHT:
            case LEFT:;
                int32_t addend32 = 0;
                offset = 0;
                while (inst_arr[i + offset] == RIGHT || inst_arr[i + offset] == LEFT) {
                    addend32 += inst_arr[i + offset] == RIGHT ? 1 : -1;
                    offset++;
                }

                switch (addend32) {
                    case 0:
                        break;
                    case 1:
                        memcpy(code + code_size, inst[RIGHT], inst_size[RIGHT]);
                        code_size += inst_size[RIGHT];
                        break;
                    case -1:
                        memcpy(code + code_size, inst[LEFT], inst_size[LEFT]);
                        code_size += inst_size[LEFT];
                        break;
                    default:
                        memcpy(code + code_size, inst[ADDP], inst_size[ADDP]);
                        code_size += inst_size[ADDP];
                        /* assumes your compiler signs the same way as asm */
                        *(int32_t*)(code + code_size - 4) = addend32;
                        break;
                }
                i += offset - 1;
                break;
            case WHILE:
            case END:;
                memset(code + code_size + 1, inst_arr[i] == WHILE ? '[' : ']',
                        inst_size[inst_arr[i]]);
                *(code + code_size) = '*';
                code_size += inst_size[inst_arr[i]];
                break;
            default:
                memcpy(code + code_size, inst[inst_arr[i]], inst_size[inst_arr[i]]);
                code_size += inst_size[inst_arr[i]];
                break;
        }
    }

    /* if inst_size[WHILE] is less than 8 this would break */
    for (int i = 0; i < code_size; i += 8) {
        if (code[i] == '[') {
            uint8_t val = '[';
            /* get to start of brackets */
            while (code[i] != '*') i--;

            size_t whiles = 1;
            size_t ends = 0;
            int32_t offset = inst_size[WHILE];

            while (whiles != ends) {
                if (code[i + offset] == '[' || code[i + offset] == ']') {
                    val = code[i + offset];
                    if (val == '[') whiles++; else ends++;
                    while (code[i + offset] != '*') offset--;
                    offset += inst_size[WHILE];
                } else
                    offset += 8;
            }

            memcpy(code + i, inst[WHILE], inst_size[WHILE]);
            *(int32_t*)(code + i + inst_size[WHILE] - 4) = offset - inst_size[END];

            memcpy(code + i + offset - inst_size[END], inst[END], inst_size[END]);
            *(int32_t*)(code + i + offset - 4) = -offset + inst_size[WHILE];


        } else if (code[i] == ']')
            die("non matching ']'");
    }

    memcpy(code + code_size, inst[EXIT], inst_size[EXIT]);
    code_size += inst_size[EXIT];

    FILE* outfp = fopen(argv[2], "wb");
    if (!outfp)
        die("unable to open output file");

    Elf64_Ehdr ehdr;

    ehdr.e_ident[EI_MAG0] = 0x7f;
    ehdr.e_ident[EI_MAG1] = 'E';
    ehdr.e_ident[EI_MAG2] = 'L';
    ehdr.e_ident[EI_MAG3] = 'F';
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr.e_ident[EI_ABIVERSION] = 0;

    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = 0x1000;
    ehdr.e_phoff = sizeof(Elf64_Ehdr); ehdr.e_shoff = 0;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 2;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = SHN_UNDEF;

    Elf64_Phdr phdrs[2];

    phdrs[0].p_type  = PT_LOAD;
    phdrs[0].p_offset = 0x1000;
    phdrs[0].p_vaddr = 0x1000;
    phdrs[0].p_paddr = 0;
    phdrs[0].p_align = 0x1000;
    phdrs[0].p_flags = PF_X | PF_R;
    phdrs[0].p_filesz = code_size;
    phdrs[0].p_memsz  = code_size;

    phdrs[1].p_type  = PT_LOAD;
    phdrs[1].p_offset = 0;
    phdrs[1].p_vaddr = 0x20000000;
    phdrs[1].p_paddr = 0;
    phdrs[1].p_align = 0x1000;
    phdrs[1].p_flags = PF_W | PF_R;
    phdrs[1].p_filesz = 0;
    phdrs[1].p_memsz  = 30000;

    fwrite(&ehdr, sizeof(Elf64_Ehdr), 1, outfp);
    fwrite(&phdrs[0], sizeof(Elf64_Phdr), 1, outfp);
    fwrite(&phdrs[1], sizeof(Elf64_Phdr), 1, outfp);
    fseek(outfp, 0x1000, SEEK_SET);
    fwrite(code, 1, code_size, outfp);

    fclose(outfp);

    chmod(argv[2], S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP |
            S_IROTH | S_IXOTH );
}
