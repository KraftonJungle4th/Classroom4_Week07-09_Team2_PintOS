#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "intrinsic.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill(struct intr_frame *);
static void page_fault(struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void exception_init(void)
{
	/* These exceptions can be raised explicitly by a user program,
	   e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
	   we set DPL==3, meaning that user programs are allowed to
	   invoke them via these instructions. */
	intr_register_int(3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
	intr_register_int(4, 3, INTR_ON, kill, "#OF Overflow Exception");
	intr_register_int(5, 3, INTR_ON, kill,
					  "#BR BOUND Range Exceeded Exception");

	/* These exceptions have DPL==0, preventing user processes from
	   invoking them via the INT instruction.  They can still be
	   caused indirectly, e.g. #DE can be caused by dividing by
	   0.  */
	intr_register_int(0, 0, INTR_ON, kill, "#DE Divide Error");
	intr_register_int(1, 0, INTR_ON, kill, "#DB Debug Exception");
	intr_register_int(6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
	intr_register_int(7, 0, INTR_ON, kill,
					  "#NM Device Not Available Exception");
	intr_register_int(11, 0, INTR_ON, kill, "#NP Segment Not Present");
	intr_register_int(12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
	intr_register_int(13, 0, INTR_ON, kill, "#GP General Protection Exception");
	intr_register_int(16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
	intr_register_int(19, 0, INTR_ON, kill,
					  "#XF SIMD Floating-Point Exception");

	/* Most exceptions can be handled with interrupts turned on.
	   We need to disable interrupts for page faults because the
	   fault address is stored in CR2 and needs to be preserved. */
	intr_register_int(14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void exception_print_stats(void)
{
	printf("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill(struct intr_frame *f)
{
	/* This interrupt is one (probably) caused by a user process.
	   For example, the process might have tried to access unmapped
	   virtual memory (a page fault).  For now, we simply kill the
	   user process.  Later, we'll want to handle page faults in
	   the kernel.  Real Unix-like operating systems pass most
	   exceptions back to the process via signals, but we don't
	   implement them. */

	/* The interrupt frame's code segment value tells us where the
	   exception originated. */
	switch (f->cs)
	{
	case SEL_UCSEG:
		/* User's code segment, so it's a user exception, as we
		   expected.  Kill the user process.  */
		printf("%s: dying due to interrupt %#04llx (%s).\n",
			   thread_name(), f->vec_no, intr_name(f->vec_no));
		intr_dump_frame(f);
		thread_exit();

	case SEL_KCSEG:
		/* Kernel's code segment, which indicates a kernel bug.
		   Kernel code shouldn't throw exceptions.  (Page faults
		   may cause kernel exceptions--but they shouldn't arrive
		   here.)  Panic the kernel to make the point.  */
		intr_dump_frame(f);
		PANIC("Kernel bug - unexpected interrupt in kernel");

	default:
		/* Some other code segment?  Shouldn't happen.  Panic the
		   kernel. */
		printf("Interrupt %#04llx (%s) in unknown segment %04x\n",
			   f->vec_no, intr_name(f->vec_no), f->cs);
		thread_exit();
	}
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
/* 페이지 오류 처리기. 이것은 가상 메모리를 구현하기 위해 채워야
   하는 골격입니다. 프로젝트 2의 일부 솔루션은 이 코드를 수정해야
   할 수도 있습니다.

   입력 시 오류가 발생한 주소는 CR2(제어 레지스터 2)에 있으며,
   exception.h의 PF_* 매크로에 설명된 대로 형식이 지정된 오류에
   대한 정보는 F의 error_code 멤버에 있습니다. 여기 예제 코드는
   해당 정보를 구문 분석하는 방법을 보여줍니다. 이 두 가지에 대한
   자세한 내용은 [IA32-v3a] 섹션 5.15 "예외 및 인터럽트 참조"의
   "인터럽트 14--페이지 오류 예외(#PF)"에 대한 설명에서 확인할 수
   있습니다. */
static void page_fault(struct intr_frame *f)
{
	bool not_present; /* True: not-present page, false: writing r/o page. */
					  /* 참: 존재하지 않는 페이지, 거짓: R/O 페이지 쓰기. */
	bool write;		  /* True: access was write, false: access was read. */
					  /* 참: 액세스 권한이 쓰기, 거짓: 액세스 권한이 읽기입니다. */
	bool user;		  /* True: access by user, false: access by kernel. */
					  /* 참: 사용자에 의한 액세스, 거짓: 커널에 의한 액세스. */
	void *fault_addr; /* Fault address. */
					  /* 오류 주소. */

	/* Obtain faulting address, the virtual address that was
	   accessed to cause the fault.  It may point to code or to
	   data.  It is not necessarily the address of the instruction
	   that caused the fault (that's f->rip). */
	/* 결함 주소, 즉 결함을 유발하기 위해 액세스한 가상 주소를
	   가져옵니다. 코드나 데이터를 가리킬 수 있습니다. 반드시 오류를
	   일으킨 명령어의 주소일 필요는 없습니다. (즉, f->rip) */
	fault_addr = (void *)rcr2();

	/* Turn interrupts back on (they were only off so that we could
	   be assured of reading CR2 before it changed). */
	/* 인터럽트를 다시 켭니다. (변경 전에는 CR2를 읽을 수 있도록
	   하기 위해 꺼져 있었습니다) */
	intr_enable();

	/* Determine cause. */
	/* 원인을 파악합니다. */
	not_present = (f->error_code & PF_P) == 0;
	write = (f->error_code & PF_W) != 0;
	user = (f->error_code & PF_U) != 0;

#ifdef VM
	/* For project 3 and later. */
	if (vm_try_handle_fault(f, fault_addr, user, write, not_present))
		return;
#endif

	/* Count page faults. */
	/* 페이지 오류를 계산합니다. */
	page_fault_cnt++;

	/* If the fault is true fault, show info and exit. */
	/* 결함이 실제 결함인 경우, 정보를 표시하고 종료합니다. */
	printf("Page fault at %p: %s error %s page in %s context.\n",
		   fault_addr,
		   not_present ? "not present" : "rights violation",
		   write ? "writing" : "reading",
		   user ? "user" : "kernel");
	kill(f);
}
