#include "threads/init.h"
#include <console.h>
#include <debug.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "devices/kbd.h"
#include "devices/input.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/process.h"
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#endif
#include "tests/threads/tests.h"
#ifdef VM
#include "vm/vm.h"
#endif
#ifdef FILESYS
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif

/* Page-map-level-4 with kernel mappings only. */
/* 커널 매핑만 있는 페이지 맵 레벨 4. */
uint64_t *base_pml4;

#ifdef FILESYS
/* -f: Format the file system? */
static bool format_filesys;
#endif

/* -q: Power off after kernel tasks complete? */
bool power_off_when_done;

bool thread_tests;

static void bss_init(void);
static void paging_init(uint64_t mem_end);

static char **read_command_line(void);
static char **parse_options(char **argv);
static void run_actions(char **argv);
static void usage(void);

static void print_stats(void);

int main(void) NO_RETURN;

/* Pintos main program. */
int main(void)
{
	uint64_t mem_end;
	char **argv;

	/* Clear BSS and get machine's RAM size. */
	bss_init(); // bss 초기화, ram사이즈 얻기

	/* Break command line into arguments and parse options. */
	argv = read_command_line(); // 읽기
	argv = parse_options(argv); // 비-옵션 인자 반환해서 저장
	/* Initialize ourselves as a thread so we can use locks,
	   then enable console locking. */

	thread_init();	// thread 초기화
	console_init(); // 콘솔락 활성화

	/* Initialize memory system. */
	mem_end = palloc_init(); // 페이지 할당기 초기화 하고 메모리 사이즈 return
	malloc_init();			 // malloc descriptor return
	paging_init(mem_end);	 // 페이징 함수 호출

#ifdef USERPROG // USERPROG 매크로 등록 되어 있을 때 만
	tss_init();
	gdt_init();
#endif

	/* Initialize interrupt handlers. */
	// 인터럽트를 활성화 하고 등록한다
	intr_init();
	timer_init(); // 인터럽트 주기를 생성하고 타이머 인터럽트 등록
	kbd_init();	  // keyboard의 의미?
	input_init(); // input buffer 초기화
#ifdef USERPROG
	exception_init();
	syscall_init();
#endif
	/* Start thread scheduler and enable interrupts. */
	/* 스레드 스케줄러를 시작하고 인터럽트를 활성화합니다. */

	thread_start();		 // 스레드 시작, 안에서 스레드 생성 및 ready 큐에 넣어줌
	serial_init_queue(); // 대기열 기반의 인터럽트 주도 I/O을 위해 시리얼 포트 장치를 초기화,
						 // 인터럽트 주도 I/O를 사용하면 시리얼 장치가 준비될 때까지 CPU 시간을 낭비 x
	timer_calibrate();	 // 타이머 조정

#ifdef FILESYS
	/* Initialize file system. */
	disk_init();
	filesys_init(format_filesys);
#endif

#ifdef VM
	vm_init();
#endif

	printf("Boot complete.\n");

	/* Run actions specified on kernel command line. */
	run_actions(argv); // argv의 모든 요소 실행

	/* Finish up. */
	if (power_off_when_done)
		power_off();
	thread_exit(); // 스레드 종료
}

/* Clear BSS */
/* BSS 지우기 */
static void bss_init(void)
{
	/* The "BSS" is a segment that should be initialized to zeros.
	   It isn't actually stored on disk or zeroed by the kernel
	   loader, so we have to zero it ourselves.

	   The start and end of the BSS segment is recorded by the
	   linker as _start_bss and _end_bss.  See kernel.lds. */
	/* "BSS"는 0으로 초기화해야 하는 세그먼트입니다.
	   실제로 디스크에 저장되거나 커널 로더가 0으로 초기화하지
	   않으므로 우리가 직접 0으로 초기화해야 합니다.

	   BSS 세그먼트의 시작과 끝은 링커에 의해 _start_bss 및
	   _end_bss로 기록됩니다. kernel.lds를 참조하세요. */
	extern char _start_bss, _end_bss;
	memset(&_start_bss, 0, &_end_bss - &_start_bss);
}

/* Populates the page table with the kernel virtual mapping,
 * and then sets up the CPU to use the new page directory.
 * Points base_pml4 to the pml4 it creates. */

/* 커널 가상 매핑으로 페이지 테이블을 채운 다음 CPU가
 * 새 페이지 디렉터리를 사용하도록 설정합니다.
 * base_pml4를 생성한 pml4로 가리킵니다. */
static void paging_init(uint64_t mem_end)
{
	uint64_t *pml4 = base_pml4 = palloc_get_page(PAL_ASSERT | PAL_ZERO);
	extern char start, _end_kernel_text;
	// Maps physical address [0 ~ mem_end] to
	// [LOADER_KERN_BASE ~ LOADER_KERN_BASE + mem_end].

	// 실제 주소 [0 ~ mem_end]를 다음과 같이 매핑합니다.
	// [LOADER_KERN_BASE ~ LOADER_KERN_BASE + mem_end].
	for (uint64_t pa = 0; pa < mem_end; pa += PGSIZE)
	{
		uint64_t va = (uint64_t)ptov(pa);
		int perm = PTE_P | PTE_W;

		if ((uint64_t)&start <= va && va < (uint64_t)&_end_kernel_text)
		{
			perm &= ~PTE_W;
		}

		uint64_t *pte;

		if ((pte = pml4e_walk(pml4, va, 1)) != NULL)
		{
			*pte = pa | perm;
		}
	}

	// reload cr3
	pml4_activate(0);
}

/* Breaks the kernel command line into words and returns them as
   an argv-like array. */
/* 커널 명령줄을 단어로 나누고 이를 argv와 같은 배열로 반환합니다. */
static char **read_command_line(void)
{
	static char *argv[LOADER_ARGS_LEN / 2 + 1];
	char *p, *end;
	int argc;
	int i;

	argc = *(uint32_t *)ptov(LOADER_ARG_CNT);
	p = ptov(LOADER_ARGS);
	end = p + LOADER_ARGS_LEN;
	for (i = 0; i < argc; i++)
	{
		if (p >= end)
			PANIC("command line arguments overflow");

		argv[i] = p;
		p += strnlen(p, end - p) + 1;
	}
	argv[argc] = NULL;

	/* Print kernel command line. */
	/* 커널 명령줄을 출력합니다. */
	printf("Kernel command line:");
	for (i = 0; i < argc; i++)
		if (strchr(argv[i], ' ') == NULL)
			printf(" %s", argv[i]);
		else
			printf(" '%s'", argv[i]);
	printf("\n");

	return argv;
}

/* Parses options in ARGV[]
   and returns the first non-option argument. */
/* ARGV[]의 옵션을 구문 분석하고 첫 번째 옵션이
   아닌 인수를 반환합니다. */
static char **parse_options(char **argv)
{
	for (; *argv != NULL && **argv == '-'; argv++)
	{ // point 배열이 비어있지 않고 첫 요소가 '-'라면 loop
		char *save_ptr;
		char *name = strtok_r(*argv, "=", &save_ptr); //'=' 문자를 만나면 분리됨, save_ptr은 분기된 이후의 문자열 저장
		char *value = strtok_r(NULL, "", &save_ptr);  // 두번 째 호출 부터는 전달된 인자의 분기점 부터 탐색, NULL 전달

		if (!strcmp(name, "-h"))
			usage();
		else if (!strcmp(name, "-q"))
			power_off_when_done = true;
#ifdef FILESYS
		else if (!strcmp(name, "-f"))
			format_filesys = true;
#endif
		else if (!strcmp(name, "-rs"))
			random_init(atoi(value));
		else if (!strcmp(name, "-mlfqs"))
			thread_mlfqs = true;
#ifdef USERPROG
		else if (!strcmp(name, "-ul"))
			user_page_limit = atoi(value);
		else if (!strcmp(name, "-threads-tests"))
			thread_tests = true;
#endif
		else
			PANIC("unknown option `%s' (use -h for help)", name);
	}

	return argv;
}

/* Runs the task specified in ARGV[1]. */
/* ARGV[1]에 지정된 작업을 실행합니다. */
static void run_task(char **argv)
{
	const char *task = argv[1];

	printf("Executing '%s':\n", task);
#ifdef USERPROG
	if (thread_tests)
	{
		run_test(task);
	}
	else
	{
		process_wait(process_create_initd(task));
	}
#else
	run_test(task);
#endif
	printf("Execution of '%s' complete.\n", task);
}

/* Executes all of the actions specified in ARGV[]
   up to the null pointer sentinel. */
/* ARGV[] 에 지정된 모든 액션을 널 포인터 센티널까지
   실행합니다. */
static void run_actions(char **argv)
{
	/* An action. */
	struct action
	{
		char *name;					   /* Action name. */
		int argc;					   /* # of args, including action name. */
		void (*function)(char **argv); /* Function to execute action. */
	};

	/* Table of supported actions. */
	/* 지원되는 작업 표입니다. */
	static const struct action actions[] = {
		{"run", 2, run_task},
#ifdef FILESYS
		{"ls", 1, fsutil_ls},
		{"cat", 2, fsutil_cat},
		{"rm", 2, fsutil_rm},
		{"put", 2, fsutil_put},
		{"get", 2, fsutil_get},
#endif
		{NULL, 0, NULL},
	};

	while (*argv != NULL)
	{
		const struct action *a;
		int i;

		/* Find action name. */
		for (a = actions;; a++)
			if (a->name == NULL)
				PANIC("unknown action `%s' (use -h for help)", *argv);
			else if (!strcmp(*argv, a->name))
				break;

		/* Check for required arguments. */
		for (i = 1; i < a->argc; i++)
			if (argv[i] == NULL)
				PANIC("action `%s' requires %d argument(s)", *argv, a->argc - 1);

		/* Invoke action and advance. */
		a->function(argv);
		argv += a->argc;
	}
}

/* Prints a kernel command line help message and powers off the
   machine. */
/* 커널 명령줄 도움말 메시지를 출력하고 컴퓨터의 전원을 끕니다. */
static void usage(void)
{
	printf("\nCommand line syntax: [OPTION...] [ACTION...]\n"
		   "Options must precede actions.\n"
		   "Actions are executed in the order specified.\n"
		   "\nAvailable actions:\n"
#ifdef USERPROG
		   "  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
		   "  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
		   "  ls                 List files in the root directory.\n"
		   "  cat FILE           Print FILE to the console.\n"
		   "  rm FILE            Delete FILE.\n"
		   "Use these actions indirectly via `pintos' -g and -p options:\n"
		   "  put FILE           Put FILE into file system from scratch disk.\n"
		   "  get FILE           Get FILE from file system into scratch disk.\n"
#endif
		   "\nOptions:\n"
		   "  -h                 Print this help message and power off.\n"
		   "  -q                 Power off VM after actions or on panic.\n"
		   "  -f                 Format file system disk during startup.\n"
		   "  -rs=SEED           Set random number seed to SEED.\n"
		   "  -mlfqs             Use multi-level feedback queue scheduler.\n"
#ifdef USERPROG
		   "  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
	);
	power_off();
}

/* Powers down the machine we're running on,
   as long as we're running on Bochs or QEMU. */
/* 실행 중인 머신의 전원을 끕니다.
   (Bochs 또는 QEMU에서 실행 중인 경우) */
void power_off(void)
{
#ifdef FILESYS
	filesys_done();
#endif

	print_stats();

	printf("Powering off...\n");
	outw(0x604, 0x2000); /* Poweroff command for qemu */
						 /* qemu용 전원 끄기 명령어 */
	for (;;)
		;
}

/* Print statistics about Pintos execution. */
/* 핀토스 실행에 대한 통계를 출력합니다. */
static void print_stats(void)
{
	timer_print_stats();
	thread_print_stats();
#ifdef FILESYS
	disk_print_stats();
#endif
	console_print_stats();
	kbd_print_stats();
#ifdef USERPROG
	exception_print_stats();
#endif
}
