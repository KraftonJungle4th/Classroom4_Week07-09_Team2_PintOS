#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* Doubly linked list.
 *
 * This implementation of a doubly linked list does not require
 * use of dynamically allocated memory.  Instead, each structure
 * that is a potential list element must embed a struct list_elem
 * member.  All of the list functions operate on these `struct
 * list_elem's.  The list_entry macro allows conversion from a
 * struct list_elem back to a structure object that contains it.

 * For example, suppose there is a needed for a list of `struct
 * foo'.  `struct foo' should contain a `struct list_elem'
 * member, like so:

 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...other members...
 * };

 * Then a list of `struct foo' can be be declared and initialized
 * like so:

 * struct list foo_list;

 * list_init (&foo_list);

 * Iteration is a typical situation where it is necessary to
 * convert from a struct list_elem back to its enclosing
 * structure.  Here's an example using foo_list:

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...do something with f...
 * }

 * You can find real examples of list usage throughout the
 * source; for example, malloc.c, palloc.c, and thread.c in the
 * threads directory all use lists.

 * The interface for this list is inspired by the list<> template
 * in the C++ STL.  If you're familiar with list<>, you should
 * find this easy to use.  However, it should be emphasized that
 * these lists do *no* type checking and can't do much other
 * correctness checking.  If you screw up, it will bite you.

 * Glossary of list terms:

 * - "front": The first element in a list.  Undefined in an
 * empty list.  Returned by list_front().

 * - "back": The last element in a list.  Undefined in an empty
 * list.  Returned by list_back().

 * - "tail": The element figuratively just after the last
 * element of a list.  Well defined even in an empty list.
 * Returned by list_end().  Used as the end sentinel for an
 * iteration from front to back.

 * - "beginning": In a non-empty list, the front.  In an empty
 * list, the tail.  Returned by list_begin().  Used as the
 * starting point for an iteration from front to back.

 * - "head": The element figuratively just before the first
 * element of a list.  Well defined even in an empty list.
 * Returned by list_rend().  Used as the end sentinel for an
 * iteration from back to front.

 * - "reverse beginning": In a non-empty list, the back.  In an
 * empty list, the head.  Returned by list_rbegin().  Used as
 * the starting point for an iteration from back to front.
 *
 * - "interior element": An element that is not the head or
 * tail, that is, a real list element.  An empty list does
 * not have any interior elements.*/

/* 이중 연결 리스트.

   이중 연결 리스트의 구현에는 동적으로 할당된 메모리를 사용할 필요가
   없습니다. 대신, 잠재적 리스트 요소인 각 구조체는 구조체 list_elem
   멤버를 포함해야 합니다. 모든 리스트 함수는 이러한 `struct list_elem'
   에서 작동합니다. list_entry 매크로를 사용하면 구조체 list_elem에서
   이를 포함하는 구조체 객체로 다시 변환할 수 있습니다.

   예를 들어 `struct foo' 리스트가 필요하다고 가정해 보겠습니다. `struct
   foo'는 다음과 같이 `struct list_elem' 멤버를 포함해야 합니다:

   struct foo {
      struct list_elem elem;
      int bar;
      // ...다른 멤버들...
   };

   그러면 다음과 같이 'struct foo'의 리스트를 선언하고 초기화할 수 있습니다:

   ```
   struct list foo_list;
   list_init (&foo_list);
   ```

   반복은 struct list_elem에서 다시 그 안에 있는 구조체로 변환해야 하는
   일반적인 상황입니다. 다음은 foo_list를 사용한 예시입니다:

   ```
   struct list_elem *e;

   for (e = list_begin (&foo_list); e != list_end (&foo_list); e = list_next (e)) {
      struct foo *f = list_entry (e, struct foo, elem);
      // ...f로 무언가를 수행...
   }
   ```

   소스 전체에서 리스트 사용의 실제 예를 찾을 수 있습니다. 예를 들어,
   threads 디렉터리의 malloc.c, palloc.c, thread.c는 모두 리스트를 사용합니다.

   이 리스트의 인터페이스는 C++ STL의 list<> 템플릿에서 영감을 얻었습니다. list<>에
   익숙하다면 사용하기 쉬울 것입니다. 하지만 이 리스트는 타입 검사를 *아니* 수행하지
   않으며 다른 많은 정확성 검사를 수행할 수 없다는 점을 강조해야 합니다.
   실수하면 물릴 수 있습니다.

   목록 용어집:

   - "front": 목록의 첫 번째 요소. 빈 리스트에서는 정의되지 않습니다. list_front()에 의해 반환됩니다.
   - "back": 목록의 마지막 요소. 빈 리스트에서 정의되지 않습니다. list_back()에 의해 반환됩니다.
   - "tail": 비유적으로 목록의 마지막 요소 바로 뒤에 있는 요소. 빈 목록에서도 잘 정의됩니다.
   list_end()에 의해 반환됩니다. 앞쪽에서 뒤쪽으로 반복할 때 마지막 센티널로 사용됩니다.

   - "beginning": 비어 있지 않은 목록에서는 앞쪽입니다. 비어 있는 목록에서는 꼬리입니다.
   list_begin()에 의해 반환됩니다. 앞쪽에서 뒤쪽으로의 반복을 위한 시작점으로 사용됩니다.

   - "head": 비유적으로 목록의 첫 번째 요소 바로 앞에 있는 요소. 빈 목록에서도 잘 정의됩니다.
   list_rend()에 의해 반환됩니다. 뒤에서 앞으로의 반복에 대한 끝 감시자로 사용됩니다.

   - "reverse beginning": 비어 있지 않은 목록에서는 뒤쪽. 비어 있는 목록에서는 앞쪽입니다.
   list_rbegin()에 의해 반환됩니다. 뒤에서 앞으로의 반복을 위한 시작점으로 사용됩니다.

   - "interior element": 머리나 꼬리가 아닌 요소, 즉 실제 목록 요소입니다.
   빈 목록에는 내부 요소가 없습니다. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* List element. */
/* 리스트 요소. */
struct list_elem
{
   struct list_elem *prev; /* Previous list element. */
                           /* 이전 리스트 요소. */
   struct list_elem *next; /* Next list element. */
                           /* 다음 리스트 요소. */
};

/* List. */
/* 리스트. */
struct list
{
   struct list_elem head; /* List head. */
                          /* 리스트 머리. */
   struct list_elem tail; /* List tail. */
                          /* 리스트 꼬리. */
};

/* Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element.  See the big comment at the top of the
   file for an example. */
/* 리스트 요소 LIST_ELEM의 포인터를 LIST_ELEM이 포함된 구조체의
   포인터로 변환합니다. 외부 구조체 STRUCT의 이름과 리스트
   요소의 멤버 이름 MEMBER를 입력합니다. 예제는 파일 상단의 큰
   주석을 참조하세요. */
#define list_entry(LIST_ELEM, STRUCT, MEMBER) \
   ((STRUCT *)((uint8_t *)&(LIST_ELEM)->next - offsetof(STRUCT, MEMBER.next)))

void list_init(struct list *);

/* List traversal. */
/* 리스트 횡단. */
struct list_elem *list_begin(struct list *);
struct list_elem *list_next(struct list_elem *);
struct list_elem *list_end(struct list *);

struct list_elem *list_rbegin(struct list *);
struct list_elem *list_prev(struct list_elem *);
struct list_elem *list_rend(struct list *);

struct list_elem *list_head(struct list *);
struct list_elem *list_tail(struct list *);

/* List insertion. */
/* 리스트 삽입. */
void list_insert(struct list_elem *, struct list_elem *);
void list_splice(struct list_elem *before,
                 struct list_elem *first, struct list_elem *last);
void list_push_front(struct list *, struct list_elem *);
void list_push_back(struct list *, struct list_elem *);

/* List removal. */
/* 리스트 제거. */
struct list_elem *list_remove(struct list_elem *);
struct list_elem *list_pop_front(struct list *);
struct list_elem *list_pop_back(struct list *);

/* List elements. */
/* 리스트 요소. */
struct list_elem *list_front(struct list *);
struct list_elem *list_back(struct list *);

/* List properties. */
/* 리스트 속성. */
size_t list_size(struct list *);
bool list_empty(struct list *);

/* Miscellaneous. */
/* 기타. */
void list_reverse(struct list *);

/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
/* 보조 데이터 AUX가 주어졌을 때 두 목록 요소 A와 B의 값을
   비교합니다. A가 B보다 작으면 참을 반환하고, A가 B보다
   크거나 같으면 거짓을 반환합니다. */
typedef bool list_less_func(const struct list_elem *a,
                            const struct list_elem *b,
                            void *aux);

/* Operations on lists with ordered elements. */
/* 정렬된 요소가 있는 리스트에 대한 연산. */
void list_sort(struct list *,
               list_less_func *, void *aux);
void list_insert_ordered(struct list *, struct list_elem *,
                         list_less_func *, void *aux);
void list_unique(struct list *, struct list *duplicates,
                 list_less_func *, void *aux);

/* Max and min. */
/* 최대 및 최소. */
struct list_elem *list_max(struct list *, list_less_func *, void *aux);
struct list_elem *list_min(struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
