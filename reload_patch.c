#define ENABLE_RELOAD_PATCH 1
#define LNX_DLL_PATCH_VERBOSE 1

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// TODO: Put something more general here
// #define ASSERT(expr) {if(!(expr)){fprintf(stderr, "Assertion failed: %s, line %d (%s)\n", __FILE__, __LINE__, #expr); __builtin_debugtrap();}}
#define ASSERT(expr) {if(!(expr)){fprintf(stderr, "Assertion failed: %s, line %d (%s)\n", __FILE__, __LINE__, #expr); assert(false);}}

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

typedef uint8_t Byte;
typedef uint32_t U32;
typedef uint64_t U64;
#define U64MAX 0xffffffffffffffffUL

#include <elf.h>
#include <link.h>
// These two lines to avoid defining _GNU_SOURCE
int dlinfo(void *handle, int request, void *info);
#define RTLD_DI_LINKMAP 2

static Byte * read_entire_file(char *file_name){
    Byte *result = 0;

    FILE *f = fopen(file_name, "rb");
    if(f){
        fseek(f, 0, SEEK_END);
        ssize_t size = ftell(f);
        fseek(f, 0, SEEK_SET);

        result = calloc(1, size);
        if(result){
            ssize_t bytes_read = fread(result, 1, size, f);
            if(bytes_read != size){
                free(result);
                result = 0;
            }
        }
        fclose(f);
    }
    return result;
}

struct SymbolReplacement{
    char *name;
    void *neg_addr_prev;            // Negated addresses of symbols to avoid patching them by mistake
    void *neg_addr_cur;
    struct SymbolReplacement *next;
    bool take_into_account;
};

#define FILE_MAGIC_NUMBER(a, b, c, d) (((U32)(a) << 0) | ((U32)(b) << 8) | ((U32)(c) << 16) | ((U32)(d) << 24))
#define ELF_MAGIC_VALUE FILE_MAGIC_NUMBER(127, 'E', 'L', 'F')
#define NEGATE_PTR(addr) (void *)~((U64)(addr))
#define NEGATE_U64(addr) (~((U64)(addr)))

static void patch_so_symbols(void *so_handle){
    static struct SymbolReplacement symbol_replacement_sentinel = {0};
    for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
        sr->neg_addr_prev = sr->neg_addr_cur;           // Cycle cur to prev
        sr->neg_addr_cur = 0;
        sr->take_into_account = 0;
    }

    /* Parse .so and put addresses of symbols on sr->neg_addr_cur */ {
        struct link_map * link_map = 0;
        int ret = dlinfo(so_handle, RTLD_DI_LINKMAP, &link_map);                ASSERT(ret != -1);

        char *so_path = link_map->l_name;
        Byte *contents = read_entire_file(so_path);                             ASSERT(contents);
        // Info for non-exported, local symbols is only available on the .symtab section, 
        // which is not loaded into memory through dlopen (which looks at the .dynsym section). 
        // That's why we resort to parsing the .so file
        Elf64_Ehdr *elf_header = (Elf64_Ehdr *) contents;

        // Make sure this is the binary type we think it is
        ASSERT(*(U32*)elf_header->e_ident == ELF_MAGIC_VALUE);
        ASSERT(elf_header->e_ident[EI_CLASS] == ELFCLASS64);
        ASSERT(elf_header->e_ident[EI_DATA] == ELFDATA2LSB);
        ASSERT(elf_header->e_machine == EM_X86_64);
        ASSERT(elf_header->e_ident[EI_VERSION] == EV_CURRENT);
        ASSERT(elf_header->e_type == ET_DYN);

        Elf64_Shdr *symtab_header, *strtab_header; {
            symtab_header = 0;  // Contains info about both local and global symbols
            strtab_header = 0;  // String section for stuff other than section names

            Elf64_Shdr *first_elf_section_header = (Elf64_Shdr *)((Byte *)elf_header + elf_header->e_shoff);
            ASSERT(elf_header->e_shstrndx != SHN_UNDEF); // Section index containing the string table
            Elf64_Shdr *section_header_string_table = first_elf_section_header + elf_header->e_shstrndx;
            ASSERT(section_header_string_table->sh_type == SHT_STRTAB);
            char *section_string_table = (char *)elf_header + section_header_string_table->sh_offset;

            U32 section_count = elf_header->e_shnum;
            for(U32 i_section = 0; i_section < section_count; ++i_section){
                Elf64_Shdr *elf_section_header = first_elf_section_header + i_section;
                char *section_name = section_string_table + elf_section_header->sh_name;
#if 0
                printf("Section %d, type %d name: \"%.*s\"\n", i_section, elf_section_header->sh_type, SE(section_name));
#endif
                if(!strcmp(section_name, ".symtab")){
                    symtab_header = elf_section_header;
                }
                else if(!strcmp(section_name, ".strtab")){
                    strtab_header = elf_section_header;
                }
            }
            ASSERT(symtab_header && strtab_header);
        }

        char *string_table = (char *)elf_header + strtab_header->sh_offset;

        char *symtab = (char *)elf_header + symtab_header->sh_offset;
        ASSERT(symtab_header->sh_size % symtab_header->sh_entsize == 0);
        U32 entry_count = symtab_header->sh_size / symtab_header->sh_entsize;
        Elf64_Sym *entries = (Elf64_Sym *)symtab;
        ASSERT(symtab_header->sh_entsize == sizeof(*entries));

        // TODO: Comment
        for(U32 i_entry = 0; i_entry < entry_count; ++i_entry){
            Elf64_Sym *entry = entries + i_entry;
#if 0
            if(ELF64_ST_TYPE(entry->st_info) == STT_FUNC || ELF64_ST_TYPE(entry->st_info) == STT_OBJECT)
#endif
            if(entry->st_value && entry->st_name){
                char *symbol_name = string_table + entry->st_name;

                struct SymbolReplacement *insert_after = &symbol_replacement_sentinel;
                for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
                    int cmp = strcmp(sr->name, symbol_name);
                    if(!cmp){
                        void *addr = (Byte *)link_map->l_addr + entry->st_value;
                        sr->neg_addr_cur = NEGATE_PTR(addr);
                        insert_after = 0;
                        if(sr->neg_addr_prev != sr->neg_addr_cur){
                            sr->take_into_account = true;
                        }
                        break;
                    }
                    else if(cmp < 0){
                        insert_after = sr;
                    }
                    else{
                        break;
                    }
                }

                if(insert_after){
                    struct SymbolReplacement *sr = calloc(1, sizeof(struct SymbolReplacement)); {
                        sr->name = strdup(symbol_name);
                        void *addr = (Byte *)link_map->l_addr + entry->st_value;
                        sr->neg_addr_cur = NEGATE_PTR(addr);
                        sr->next = insert_after->next;
                    }
                    insert_after->next = sr;
                }
            }
        }

        free(contents);
    }

    void *min_addr, *max_addr; {
        min_addr = (void *)U64MAX; 
        max_addr = 0;

        for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
            if(sr->take_into_account){
                void *addr = NEGATE_PTR(sr->neg_addr_prev);
                min_addr = MIN(min_addr, addr);
                max_addr = MAX(max_addr, addr);
            }
        }
        min_addr = (Byte *) min_addr - 1;      // This is to avoid patching of min_addr and max_addr
        max_addr = (Byte *) max_addr + 1;      // TODO: Check if it's necessary
    }

    FILE *f = fopen("/proc/self/maps", "rb"); // 'man proc', section on /proc/[pid]/maps

    char line[4096];
    U64 memory_beg, memory_end;
    char rwxp[4];
    U64 offset;
    U32 dev_major, dev_minor;
    U64 inode;
    char path[4096];

    while(!feof(f)){
        fgets(line, sizeof(line), f);

        int ret = sscanf(line, "%lx-%lx %4c %lx %x:%x %lu %s", 
                         &memory_beg, &memory_end, rwxp, &offset, &dev_major, &dev_minor, &inode, path);
        
        if(ret == 8 && rwxp[0] == 'r' && rwxp[1] == 'w' && rwxp[2] == '-' && !strcmp(path, "[heap]")){
            // Interpret contents of memory segment as a pointer and see if it falls inside the library range
            void **first = (void **)memory_beg;
            void **last = (void **) ((Byte *)memory_end - sizeof(void *));

            // Could speed this up
            for(void **addrp = first; addrp <= last; addrp = (void **)((Byte *)addrp+1)) {
                if(*addrp >= min_addr && *addrp <= max_addr){
                    for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
                        if(sr->take_into_account && NEGATE_U64(sr->neg_addr_prev) == (U64)*addrp){
                            ASSERT(sr->neg_addr_cur && sr->neg_addr_prev != sr->neg_addr_cur);
#if LNX_DLL_PATCH_VERBOSE
                            printf("(%p, %s) ", (void*)addrp, sr->name);
#endif
                            *addrp = NEGATE_PTR(sr->neg_addr_cur);
                        }
                    }
                }
            }
        }
    }
    fclose(f);
}
