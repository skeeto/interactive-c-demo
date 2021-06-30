#define ENABLE_RELOAD_PATCH 1
#define LNX_DLL_PATCH_VERBOSE 1

#include <elf.h>
#include <link.h>
int dlinfo(void *handle, int request, void *info); // These two lines to avoid defining _GNU_SOURCE
#define RTLD_DI_LINKMAP 2

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t Byte;
typedef uint32_t U32;
typedef uint64_t U64;

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
    void *neg_addr_prev;            // We store SymbolReplacements in the heap, but we don't want to patch them
    void *neg_addr_cur;             // so we negate them to prevent patch_so_symbols from detecting them
    struct SymbolReplacement *next;
};

#define NEGATE_PTR(addr) (void *)~((U64)(addr))
static void patch_so_symbols(void *so_handle){
    static struct SymbolReplacement symbol_replacement_sentinel = {0};
    for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
        sr->neg_addr_prev = sr->neg_addr_cur;
        sr->neg_addr_cur = 0;
    }

    /* Parse .so and put addresses of visible symbols on sr->neg_addr_cur */ {
#define ASSERT_OR_GOTO(message, condition, label) if(!(condition)){ error_message = message; goto label; }
        char *error_message = 0;
        struct link_map * link_map = 0;
        Byte *contents = 0;

        ASSERT_OR_GOTO("patch_so_symbols assumes 64-bit pointers", sizeof(void *) == 8, so_parse_end);

        int ret = dlinfo(so_handle, RTLD_DI_LINKMAP, &link_map);
        ASSERT_OR_GOTO("Unable to dlinfo shared object", ret == 0, so_parse_end);
        char *so_path = link_map->l_name;
        contents = read_entire_file(so_path);
        ASSERT_OR_GOTO("Unable to read shared object", contents != 0, so_parse_end);

        // Info for non-exported, local symbols is only available on the .symtab section of the shared object.
        // dlopen only looks at the .dynsym section. That's why we resort to parsing the .so file
        Elf64_Ehdr *elf_header = (Elf64_Ehdr *) contents;          

        Byte elf_magic_number[4] = {0x7F, 'E', 'L', 'F'};
        ASSERT_OR_GOTO("Unexpected binary type",
                       !memcmp(elf_header->e_ident, elf_magic_number, 4) && elf_header->e_ident[EI_CLASS] == ELFCLASS64 &&
                       elf_header->e_ident[EI_DATA] == ELFDATA2LSB   && elf_header->e_machine == EM_X86_64 &&
                       elf_header->e_ident[EI_VERSION] == EV_CURRENT && elf_header->e_type == ET_DYN, so_parse_end);

        ASSERT_OR_GOTO("Undefined section header string table", elf_header->e_shstrndx != SHN_UNDEF, so_parse_end);

        Elf64_Shdr *symtab_header, *strtab_header; {
            symtab_header = 0;  // Lists local and global symbols
            strtab_header = 0;  // String section for stuff other than section names

            Elf64_Shdr *first_elf_section_header = (Elf64_Shdr *)((Byte *)elf_header + elf_header->e_shoff);
            Elf64_Shdr *section_header_string_table = first_elf_section_header + elf_header->e_shstrndx;

            ASSERT_OR_GOTO("Unexpected section header string table type", 
                           section_header_string_table->sh_type == SHT_STRTAB, so_parse_end);

            char *section_string_table = (char *)elf_header + section_header_string_table->sh_offset;

            U32 section_count = elf_header->e_shnum;
            for(U32 i_section = 0; i_section < section_count; ++i_section){
                Elf64_Shdr *elf_section_header = first_elf_section_header + i_section;
                char *section_name = section_string_table + elf_section_header->sh_name;
                if(!strcmp(section_name, ".symtab")){
                    symtab_header = elf_section_header;
                }
                else if(!strcmp(section_name, ".strtab")){
                    strtab_header = elf_section_header;
                }
            }
            ASSERT_OR_GOTO("Unable to locate .symtab section", symtab_header, so_parse_end);
            ASSERT_OR_GOTO("Unable to locate .strtab section", strtab_header, so_parse_end);
        }

        char *string_table = (char *)elf_header + strtab_header->sh_offset;

        char *symtab = (char *)elf_header + symtab_header->sh_offset;
        ASSERT_OR_GOTO(".symtab size not a multiple of entry size",
                       symtab_header->sh_size % symtab_header->sh_entsize == 0, so_parse_end);
        ASSERT_OR_GOTO(".symtab entry size not equal to sizeof(Elf64_Sym)", 
                       symtab_header->sh_entsize == sizeof(Elf64_Sym), so_parse_end);

        U32 entry_count = symtab_header->sh_size / symtab_header->sh_entsize;
        Elf64_Sym *entries = (Elf64_Sym *)symtab;

        // Add visible symbols to linked list
        for(U32 i_entry = 0; i_entry < entry_count; ++i_entry){
            Elf64_Sym *entry = entries + i_entry;
            char *symbol_name = string_table + entry->st_name;
            bool is_func_or_var = (ELF64_ST_TYPE(entry->st_info) == STT_FUNC ||
                                   ELF64_ST_TYPE(entry->st_info) == STT_OBJECT);
            bool is_named_and_non_reserved = (symbol_name[0] != 0 && symbol_name[0] != '_');
            if(is_func_or_var && is_named_and_non_reserved){
                // Sort asciibetically
                struct SymbolReplacement *insert_after = &symbol_replacement_sentinel;
                for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
                    int cmp = strcmp(sr->name, symbol_name);
                    if(!cmp){
                        void *addr = (Byte *)link_map->l_addr + entry->st_value;
                        sr->neg_addr_cur = NEGATE_PTR(addr);
                        insert_after = 0;
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

        so_parse_end:
        if(error_message){
            fprintf(stderr, "Error parsing shared object: %s\n", error_message);
        }
        if(contents){
            free(contents);
        }
#undef ASSERT_OR_GOTO
    }

    void *min_addr, *max_addr; {
        min_addr = (void *) UINTPTR_MAX;
        max_addr = 0;
        for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
            void *addr = NEGATE_PTR(sr->neg_addr_prev);
            min_addr = addr < min_addr ? addr : min_addr;
            max_addr = addr > max_addr ? addr : max_addr;
        }
    }


    U64 memory_beg, memory_end;
    char rwxp[4];
    U64 offset;
    U32 dev_major, dev_minor;
    U64 inode;
    char path[4096];

    char line[4096];
    FILE *f = fopen("/proc/self/maps", "rb"); // Format described on 'man proc', section on /proc/[pid]/maps
    while(!feof(f)){
        fgets(line, sizeof(line), f);
        int ret = sscanf(line, "%lx-%lx %4c %lx %x:%x %lu %s", 
                         &memory_beg, &memory_end, rwxp, &offset, &dev_major, &dev_minor, &inode, path);

        // Look for r/w [heap] or anonymous mmap mappings
        bool is_heap = (ret == 8 && rwxp[0] == 'r' && rwxp[1] == 'w' && rwxp[2] == '-' && !strcmp(path, "[heap]"));
        bool is_anonymous_mapping = (ret == 7 && rwxp[0] == 'r' && rwxp[1] == 'w' && rwxp[2] == '-');

        if(is_heap || is_anonymous_mapping){
            // Interpret contents of memory segment as a pointer and see if it falls inside the library range
            void **first = (void **)memory_beg;
            void **last = (void **) ((Byte *)memory_end - sizeof(void *));

            for(void **addrp = first; addrp <= last; addrp = (void **)((Byte *)addrp+1)) {
                if(*addrp >= min_addr && *addrp <= max_addr){
                    for(struct SymbolReplacement *sr = symbol_replacement_sentinel.next; sr; sr = sr->next){
                        if(sr->neg_addr_prev && sr->neg_addr_cur && sr->neg_addr_prev != sr->neg_addr_cur &&
                           *addrp == NEGATE_PTR(sr->neg_addr_prev)){
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
#undef NEGATE_PTR
