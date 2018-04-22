/*
 * fiwix/include/fiwix/i386elf.h
 */

#ifndef _FIWIX_ELF_H
#define _FIWIX_ELF_H

typedef unsigned long	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned long	Elf32_Off;
typedef long		Elf32_Sword;
typedef unsigned long	Elf32_Word;

#define	ELFMAG0		0x7f		/* EI_MAG */
#define	ELFMAG1		'E'
#define	ELFMAG2		'L'
#define	ELFMAG3		'F'
#define	ELFMAG		"\177ELF"
#define	SELFMAG		4

#define EI_NIDENT	16

typedef struct elf32_hdr{
  unsigned char	e_ident[EI_NIDENT];	/* ELF "magic number" */
  Elf32_Half	e_type;			/* File type */
  Elf32_Half	e_machine;		/* Target machine */
  Elf32_Word	e_version;		/* File version */
  Elf32_Addr	e_entry;  		/* Entry point virtual address */
  Elf32_Off	e_phoff;		/* Program header table file offset */
  Elf32_Off	e_shoff;		/* Section header table file offset */
  Elf32_Word	e_flags;		/* File flags */
  Elf32_Half	e_ehsize;		/* Sizeof Ehdr (ELF header) */
  Elf32_Half	e_phentsize;		/* Sizeof Phdr (Program header) */
  Elf32_Half	e_phnum;		/* Number Phdrs (Program header) */
  Elf32_Half	e_shentsize;		/* Sizeof Shdr (Section header) */
  Elf32_Half	e_shnum;		/* Number Shdrs (Section header) */
  Elf32_Half	e_shstrndx;		/* Shdr string index */
} Elf32_Ehdr;

#define	EI_MAG0		0		/* e_ident[] indexes */
#define	EI_MAG1		1
#define	EI_MAG2		2
#define	EI_MAG3		3
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define	EI_PAD		7

#define	ELFCLASSNONE	0		/* EI_CLASS */
#define	ELFCLASS32	1
#define	ELFCLASS64	2
#define	ELFCLASSNUM	3

#define ELFDATANONE	0		/* e_ident[EI_DATA] */
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2
#define	ELFDATANUM	3

/* ELF file types */
#define ET_NONE   0
#define ET_REL    1
#define ET_EXEC   2
#define ET_DYN    3
#define ET_CORE   4
#define ET_LOPROC 5
#define ET_HIPROC 6

#define EM_386    3

#define EV_NONE		0		/* e_version, EI_VERSION */
#define EV_CURRENT	1
#define EV_NUM		2

typedef struct elf32_phdr{
  Elf32_Word	p_type;			/* Entry type */
  Elf32_Off	p_offset;		/* File offset */
  Elf32_Addr	p_vaddr;		/* Virtual address */
  Elf32_Addr	p_paddr;		/* Physical address */
  Elf32_Word	p_filesz;		/* File size */
  Elf32_Word	p_memsz;		/* Memory size */
  Elf32_Word	p_flags;		/* Entry flags */
  Elf32_Word	p_align;		/* Memory & file alignment */
} Elf32_Phdr;

/* segment types stored in the image headers */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_NUM     7
#define PT_LOPROC  0x70000000
#define PT_HIPROC  0x7fffffff

/* permission types on sections in the program header, p_flags. */
#define PF_R	0x4
#define PF_W	0x2
#define PF_X	0x1

#define PF_MASKPROC	0xf0000000

typedef struct {
  Elf32_Word	sh_name;		/* Section name, index in string tbl */
  Elf32_Word	sh_type;		/* Type of section */
  Elf32_Word	sh_flags;		/* Miscellaneous section attributes */
  Elf32_Addr	sh_addr;		/* Section virtual addr at execution */
  Elf32_Off	sh_offset;		/* Section file offset */
  Elf32_Word	sh_size;		/* Size of section in bytes */
  Elf32_Word	sh_link;		/* Index of another section */
  Elf32_Word	sh_info;		/* Additional section information */
  Elf32_Word	sh_addralign;		/* Section alignment */
  Elf32_Word	sh_entsize;		/* Entry size if section holds table */
} Elf32_Shdr;

/* sh_type */
#define SHT_NULL	0
#define SHT_PROGBITS	1
#define SHT_SYMTAB	2
#define SHT_STRTAB	3
#define SHT_RELA	4
#define SHT_HASH	5
#define SHT_DYNAMIC	6
#define SHT_NOTE	7
#define SHT_NOBITS	8
#define SHT_REL		9
#define SHT_SHLIB	10
#define SHT_DYNSYM	11
#define SHT_NUM		12

#define SHT_LOPROC	0x70000000
#define SHT_HIPROC	0x7fffffff
#define SHT_LOUSER	0x80000000
#define SHT_HIUSER	0xffffffff

/* sh_flags */
#define SHF_WRITE	0x1
#define SHF_ALLOC	0x2
#define SHF_EXECINSTR	0x4

/* special section indexes */
#define SHN_UNDEF	0
#define SHN_LORESERVE	0xff00
#define SHN_ABS		0xfff1
#define SHN_COMMON	0xfff2
#define SHN_HIRESERVE	0xffff
#define SHN_LOPROC	0xff00
#define SHN_HIPROC	0xff1f
 
typedef struct elf32_sym{
  Elf32_Word	st_name;		/* Symbol name, index in string tbl */
  Elf32_Addr	st_value;		/* Value of the symbol */
  Elf32_Word	st_size;		/* Associated symbol size */
  unsigned char	st_info;		/* Type and binding attributes */
  unsigned char	st_other;		/* No defined meaning, 0 */
  Elf32_Half	st_shndx;		/* Associated section index */
} Elf32_Sym;

#define ELF32_ST_BIND(info) 	((info) >> 4)
#define ELF32_ST_TYPE(info) 	(((unsigned int) info) & 0xf)

/* This info is needed when parsing the symbol table */
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2
#define STB_NUM    3

#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4
#define STT_NUM     5

typedef struct elf32_rel {
  Elf32_Addr	r_offset;	/* Location at which to apply the action */
  Elf32_Word	r_info;		/* Index and type of relocation */
} Elf32_Rel;

typedef struct elf32_rela{
  Elf32_Addr	r_offset;	/* Location at which to apply the action */
  Elf32_Word	r_info;		/* Index and type of relocation */
  Elf32_Sword	r_addend;	/* Constant addend used to compute value */
} Elf32_Rela;

/* The following are used with relocations */
#define ELF32_R_SYM(info)	((info) >> 8)
#define ELF32_R_TYPE(info) 	((info) & 0xff)

/* This is the info that is needed to parse the dynamic section of the file */
#define DT_NULL		0
#define DT_NEEDED	1
#define DT_PLTRELSZ	2
#define DT_PLTGOT	3
#define DT_HASH		4
#define DT_STRTAB	5
#define DT_SYMTAB	6
#define DT_RELA		7
#define DT_RELASZ	8
#define DT_RELAENT	9
#define DT_STRSZ	10
#define DT_SYMENT	11
#define DT_INIT		12
#define DT_FINI		13
#define DT_SONAME	14
#define DT_RPATH 	15
#define DT_SYMBOLIC	16
#define DT_REL	        17
#define DT_RELSZ	18
#define DT_RELENT	19
#define DT_PLTREL	20
#define DT_DEBUG	21
#define DT_TEXTREL	22
#define DT_JMPREL	23
#define DT_LOPROC	0x70000000
#define DT_HIPROC	0x7fffffff

/* Symbolic values for the entries in the auxiliary table
   put on the initial stack */
#define AT_NULL   0	/* end of vector */
#define AT_IGNORE 1	/* entry should be ignored */
#define AT_EXECFD 2	/* file descriptor of program */
#define AT_PHDR   3	/* program headers for program */
#define AT_PHENT  4	/* size of program header entry */
#define AT_PHNUM  5	/* number of program headers */
#define AT_PAGESZ 6	/* system page size */
#define AT_BASE   7	/* base address of interpreter */
#define AT_FLAGS  8	/* flags */
#define AT_ENTRY  9	/* entry point of program */
#define AT_NOTELF 10	/* program is not ELF */
#define AT_UID    11	/* real uid */
#define AT_EUID   12	/* effective uid */
#define AT_GID    13	/* real gid */
#define AT_EGID   14	/* effective gid */


typedef struct dynamic{
  Elf32_Sword d_tag;			/* entry tabg value */
  union{
    Elf32_Sword	d_val;
    Elf32_Addr	d_ptr;
  } d_un;
} Elf32_Dyn;

#define R_386_NONE	0
#define R_386_32	1
#define R_386_PC32	2
#define R_386_GOT32	3
#define R_386_PLT32	4
#define R_386_COPY	5
#define R_386_GLOB_DAT	6
#define R_386_JMP_SLOT	7
#define R_386_RELATIVE	8
#define R_386_GOTOFF	9
#define R_386_GOTPC	10
#define R_386_NUM	11

/* Notes used in ET_CORE */
#define NT_PRSTATUS	1
#define NT_PRFPREG	2
#define NT_PRPSINFO	3
#define NT_TASKSTRUCT	4

/* Note header in a PT_NOTE section */
typedef struct elf32_note {
  Elf32_Word	n_namesz;	/* Name size */
  Elf32_Word	n_descsz;	/* Content size */
  Elf32_Word	n_type;		/* Content type */
} Elf32_Nhdr;

#define ELF_START_MMAP 0x80000000

extern Elf32_Dyn _DYNAMIC [];
#define elfhdr		elf32_hdr
#define elf_phdr	elf32_phdr
#define elf_note	elf32_note

#endif /* _FIWIX_ELF_H */
