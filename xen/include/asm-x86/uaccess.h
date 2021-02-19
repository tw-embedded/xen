
#ifndef __X86_UACCESS_H__
#define __X86_UACCESS_H__

#include <xen/compiler.h>
#include <xen/errno.h>
#include <xen/prefetch.h>
#include <asm/asm_defns.h>

#include <asm/x86_64/uaccess.h>

unsigned copy_to_user(void *to, const void *from, unsigned len);
unsigned clear_user(void *to, unsigned len);
unsigned copy_from_user(void *to, const void *from, unsigned len);
/* Handles exceptions in both to and from, but doesn't do access_ok */
unsigned __copy_to_user_ll(void __user*to, const void *from, unsigned n);
unsigned __copy_from_user_ll(void *to, const void __user *from, unsigned n);

extern long __get_user_bad(void);
extern void __put_user_bad(void);

/**
 * get_user: - Get a simple variable from user space.
 * @x:   Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define get_user(x,ptr)	\
  __get_user_check((x),(ptr),sizeof(*(ptr)))

/**
 * put_user: - Write a simple value into user space.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define put_user(x,ptr)							\
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

/**
 * __get_guest: - Get a simple variable from guest space, with less checking.
 * @x:   Variable to store result.
 * @ptr: Source address, in guest space.
 *
 * This macro copies a single simple variable from guest space to hypervisor
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define __get_guest(x, ptr) get_guest_nocheck(x, ptr, sizeof(*(ptr)))
#define get_unsafe __get_guest

/**
 * __put_guest: - Write a simple value into guest space, with less checking.
 * @x:   Value to store in guest space.
 * @ptr: Destination address, in guest space.
 *
 * This macro copies a single simple value from hypervisor space to guest
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define __put_guest(x, ptr) \
    put_guest_nocheck((__typeof__(*(ptr)))(x), ptr, sizeof(*(ptr)))
#define put_unsafe __put_guest

#define put_guest_nocheck(x, ptr, size)					\
({									\
	int err_; 							\
	put_guest_size(x, ptr, size, err_, -EFAULT);			\
	err_;								\
})

#define __put_user_check(x, ptr, size)					\
({									\
	__typeof__(*(ptr)) __user *ptr_ = (ptr);			\
	__typeof__(size) size_ = (size);				\
	access_ok(ptr_, size_) ? put_guest_nocheck(x, ptr_, size_)	\
			       : -EFAULT;				\
})

#define get_guest_nocheck(x, ptr, size)					\
({									\
	int err_; 							\
	get_guest_size(x, ptr, size, err_, -EFAULT);			\
	err_;								\
})

#define __get_user_check(x, ptr, size)					\
({									\
	__typeof__(*(ptr)) __user *ptr_ = (ptr);			\
	__typeof__(size) size_ = (size);				\
	access_ok(ptr_, size_) ? get_guest_nocheck(x, ptr_, size_)	\
			       : -EFAULT;				\
})

struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(const struct __large_struct *)(x))

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */
#define put_unsafe_asm(x, addr, err, itype, rtype, ltype, errret)	\
	stac();								\
	__asm__ __volatile__(						\
		"1:	mov"itype" %"rtype"1,%2\n"			\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	mov %3,%0\n"					\
		"	jmp 2b\n"					\
		".previous\n"						\
		_ASM_EXTABLE(1b, 3b)					\
		: "=r"(err)						\
		: ltype (x), "m"(__m(addr)), "i"(errret), "0"(err));	\
	clac()

#define get_unsafe_asm(x, addr, err, itype, rtype, ltype, errret)	\
	stac();								\
	__asm__ __volatile__(						\
		"1:	mov"itype" %2,%"rtype"1\n"			\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	mov %3,%0\n"					\
		"	xor"itype" %"rtype"1,%"rtype"1\n"		\
		"	jmp 2b\n"					\
		".previous\n"						\
		_ASM_EXTABLE(1b, 3b)					\
		: "=r"(err), ltype (x)					\
		: "m"(__m(addr)), "i"(errret), "0"(err));		\
	clac()

#define put_unsafe_size(x, ptr, size, retval, errret)                      \
do {                                                                       \
    retval = 0;                                                            \
    switch ( size )                                                        \
    {                                                                      \
    case 1: put_unsafe_asm(x, ptr, retval, "b", "b", "iq", errret); break; \
    case 2: put_unsafe_asm(x, ptr, retval, "w", "w", "ir", errret); break; \
    case 4: put_unsafe_asm(x, ptr, retval, "l", "k", "ir", errret); break; \
    case 8: put_unsafe_asm(x, ptr, retval, "q",  "", "ir", errret); break; \
    default: __put_user_bad();                                             \
    }                                                                      \
} while ( false )
#define put_guest_size put_unsafe_size

#define get_unsafe_size(x, ptr, size, retval, errret)                      \
do {                                                                       \
    retval = 0;                                                            \
    switch ( size )                                                        \
    {                                                                      \
    case 1: get_unsafe_asm(x, ptr, retval, "b", "b", "=q", errret); break; \
    case 2: get_unsafe_asm(x, ptr, retval, "w", "w", "=r", errret); break; \
    case 4: get_unsafe_asm(x, ptr, retval, "l", "k", "=r", errret); break; \
    case 8: get_unsafe_asm(x, ptr, retval, "q",  "", "=r", errret); break; \
    default: __get_user_bad();                                             \
    }                                                                      \
} while ( false )
#define get_guest_size get_unsafe_size

/**
 * __copy_to_guest_pv: - Copy a block of data into guest space, with less
 *                       checking
 * @to:   Destination address, in guest space.
 * @from: Source address, in hypervisor space.
 * @n:    Number of bytes to copy.
 *
 * Copy data from hypervisor space to guest space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
static always_inline unsigned long
__copy_to_guest_pv(void __user *to, const void *from, unsigned long n)
{
    if (__builtin_constant_p(n)) {
        unsigned long ret;

        switch (n) {
        case 1:
            put_guest_size(*(const uint8_t *)from, to, 1, ret, 1);
            return ret;
        case 2:
            put_guest_size(*(const uint16_t *)from, to, 2, ret, 2);
            return ret;
        case 4:
            put_guest_size(*(const uint32_t *)from, to, 4, ret, 4);
            return ret;
        case 8:
            put_guest_size(*(const uint64_t *)from, to, 8, ret, 8);
            return ret;
        }
    }
    return __copy_to_user_ll(to, from, n);
}
#define copy_to_unsafe __copy_to_guest_pv

/**
 * __copy_from_guest_pv: - Copy a block of data from guest space, with less
 *                         checking
 * @to:   Destination address, in hypervisor space.
 * @from: Source address, in guest space.
 * @n:    Number of bytes to copy.
 *
 * Copy data from guest space to hypervisor space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 *
 * If some data could not be copied, this function will pad the copied
 * data to the requested size using zero bytes.
 */
static always_inline unsigned long
__copy_from_guest_pv(void *to, const void __user *from, unsigned long n)
{
    if (__builtin_constant_p(n)) {
        unsigned long ret;

        switch (n) {
        case 1:
            get_guest_size(*(uint8_t *)to, from, 1, ret, 1);
            return ret;
        case 2:
            get_guest_size(*(uint16_t *)to, from, 2, ret, 2);
            return ret;
        case 4:
            get_guest_size(*(uint32_t *)to, from, 4, ret, 4);
            return ret;
        case 8:
            get_guest_size(*(uint64_t *)to, from, 8, ret, 8);
            return ret;
        }
    }
    return __copy_from_user_ll(to, from, n);
}
#define copy_from_unsafe __copy_from_guest_pv

/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	s32 addr, cont;
};
extern struct exception_table_entry __start___ex_table[];
extern struct exception_table_entry __stop___ex_table[];
extern struct exception_table_entry __start___pre_ex_table[];
extern struct exception_table_entry __stop___pre_ex_table[];

union stub_exception_token {
    struct {
        uint16_t ec;
        uint8_t trapnr;
    } fields;
    unsigned long raw;
};

extern unsigned long search_exception_table(const struct cpu_user_regs *regs);
extern void sort_exception_tables(void);
extern void sort_exception_table(struct exception_table_entry *start,
                                 const struct exception_table_entry *stop);

#endif /* __X86_UACCESS_H__ */
