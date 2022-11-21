#include <stdbool.h>

/** Define a proposition as likely true.
 * @param prop Proposition
 **/
#undef likely
#ifdef __GNUC__
#define likely(prop)                                                           \
  __builtin_expect((prop) ? true : false, true /* likely    \
                                                                   */)
#else
#define likely(prop) (prop)
#endif

/** Define a proposition as likely false.
 * @param prop Proposition
 **/
#undef unlikely
#ifdef __GNUC__
#define unlikely(prop)                                                         \
  __builtin_expect((prop) ? true : false, false /* unlikely */)
#else
#define unlikely(prop) (prop)
#endif

/** Define a variable as unused.
 **/
#undef unused
#ifdef __GNUC__
#define unused(variable) variable __attribute__((unused))
#else
#define unused(variable)
#warning This compiler has no support for GCC attributes
#endif

/** Get the index of segment
 * @param vaddr virtual address
 */
#define index_from_va(va) ((((unsigned long)va >> 48) & 0xFFFF) - 1)

/** Get virtual base address for segment
 * @param idx index in virtual memory
 * @param offset offset
 */
#define va_from_index(idx, offset)                                             \
  (void *)(((unsigned long)(idx+1) << 48) + (unsigned long)offset)

/** Get offset from virtual address
 * @param va virtual address
 */
#define offset_from_va(va) (size_t)((unsigned long)va & 0xFFFFFFFFFFFF)
