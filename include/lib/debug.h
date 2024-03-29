#ifndef __LIB_DEBUG_H
#define __LIB_DEBUG_H

/* GCC lets us add "attributes" to functions, function
 * parameters, etc. to indicate their properties.
 * See the GCC manual for details. */
/* GCC를 사용하면 함수, 함수 매개변수 등에 '속성'을
 * 추가하여 해당 속성을 나타낼 수 있습니다.
 * 자세한 내용은 GCC 매뉴얼을 참조하세요. */
#define UNUSED __attribute__((unused))
#define NO_RETURN __attribute__((noreturn))
#define NO_INLINE __attribute__((noinline))
#define PRINTF_FORMAT(FMT, FIRST) __attribute__((format(printf, FMT, FIRST)))

/* Halts the OS, printing the source file name, line number, and
 * function name, plus a user-specific message. */
/* OS를 중지하고 소스 파일 이름, 줄 번호, 함수 이름과
 * 사용자별 메시지를 출력합니다. */
#define PANIC(...) debug_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)

void debug_panic(const char *file, int line, const char *function,
				 const char *message, ...) PRINTF_FORMAT(4, 5) NO_RETURN;
void debug_backtrace(void);

#endif

/* This is outside the header guard so that debug.h may be
 * included multiple times with different settings of NDEBUG. */
/* 이것은 헤더 가드 외부에 있으므로 다른 NDEBUG 설정으로
 * debug.h가 여러 번 포함될 수 있습니다. */
#undef ASSERT
#undef NOT_REACHED

#ifndef NDEBUG
#define ASSERT(CONDITION)                            \
	if ((CONDITION))                                 \
	{                                                \
	}                                                \
	else                                             \
	{                                                \
		PANIC("assertion `%s' failed.", #CONDITION); \
	}
// 연결할 수 없는 명령문을 실행했습니다.
#define NOT_REACHED() PANIC("executed an unreachable statement");
#else
#define ASSERT(CONDITION) ((void)0)
#define NOT_REACHED() for (;;)
#endif /* lib/debug.h */
