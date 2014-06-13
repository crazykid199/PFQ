/***************************************************************
 *
 * (C) 2011-14 Nicola Bonelli <nicola.bonelli@cnit.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 ****************************************************************/

#ifndef _PF_Q_MODULE_H_
#define _PF_Q_MODULE_H_

#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/pf_q.h>

#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/ipv6.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>

#include "pf_q-sparse.h"

struct pfq_function_descr;
struct pfq_exec;

extern int pfq_symtable_register_functions  (const char *module, struct list_head *category, struct pfq_function_descr *fun);
extern int pfq_symtable_unregister_functions(const char *module, struct list_head *category, struct pfq_function_descr *fun);

/**** macros ****/

#define JUST(x) 		((1ULL<<31) | x)
#define IS_JUST(x)		((1ULL<<31) & x)
#define FROM_JUST(x)		(~(1ULL<<31) & x)
#define NOTHING 		0

#define ARGS_TYPE(a)  		__builtin_choose_expr(__builtin_types_compatible_p(arguments_t, typeof(a)), a, (void)0)

#define EVAL_FUNCTION(f, skb) 	((function_ptr_t)f.fun->ptr)(f.fun, skb)
#define EVAL_PROPERTY(f, skb) 	((property_ptr_t)f.fun->ptr)(f.fun, skb)
#define EVAL_PREDICATE(f, skb) 	((predicate_ptr_t)f.fun->ptr)(f.fun, skb)

#define get_data(type,a) get_data0(type,a)
#define set_data(type,a) set_data0(type,a)

#define get_data0(type,a) 	__builtin_choose_expr(sizeof(type) <= sizeof(ptrdiff_t), *(type *)&ARGS_TYPE(a)->arg[0], (void *)ARGS_TYPE(a)->arg[0])
#define get_data1(type,a) 	__builtin_choose_expr(sizeof(type) <= sizeof(ptrdiff_t), *(type *)&ARGS_TYPE(a)->arg[1], (void *)ARGS_TYPE(a)->arg[1])
#define get_data2(type,a) 	__builtin_choose_expr(sizeof(type) <= sizeof(ptrdiff_t), *(type *)&ARGS_TYPE(a)->arg[2], (void *)ARGS_TYPE(a)->arg[2])
#define get_data3(type,a) 	__builtin_choose_expr(sizeof(type) <= sizeof(ptrdiff_t), *(type *)&ARGS_TYPE(a)->arg[2], (void *)ARGS_TYPE(a)->arg[2])

#define set_data0(a, v)		__builtin_choose_expr(sizeof(typeof(v)) <= sizeof(ptrdiff_t), *(typeof(v) *)(&ARGS_TYPE(a)->arg[0]) = v, (void)0)
#define set_data1(a, v)		__builtin_choose_expr(sizeof(typeof(v)) <= sizeof(ptrdiff_t), *(typeof(v) *)(&ARGS_TYPE(a)->arg[1]) = v, (void)0)
#define set_data2(a, v)		__builtin_choose_expr(sizeof(typeof(v)) <= sizeof(ptrdiff_t), *(typeof(v) *)(&ARGS_TYPE(a)->arg[2]) = v, (void)0)
#define set_data3(a, v)		__builtin_choose_expr(sizeof(typeof(v)) <= sizeof(ptrdiff_t), *(typeof(v) *)(&ARGS_TYPE(a)->arg[2]) = v, (void)0)

#define make_mask(prefix)       htonl(~((1ULL << (32-prefix)) - 1))


/**** generic functional type ****/

struct pfq_functional
{
	const void *  ptr; 		// pointer to function
        ptrdiff_t     arg[4];
};

typedef struct pfq_functional *  arguments_t;

/**** function prototypes ****/

typedef struct sk_buff *(*function_ptr_t)(arguments_t, struct sk_buff *);
typedef uint64_t (*property_ptr_t) (arguments_t, struct sk_buff const *);
typedef bool (*predicate_ptr_t)	(arguments_t, struct sk_buff const *);
typedef int (*init_ptr_t) (arguments_t);
typedef int (*fini_ptr_t) (arguments_t);

typedef struct
{
	struct pfq_functional * fun;

} function_t;


typedef struct
{
	struct pfq_functional * fun;

} predicate_t;


typedef struct
{
	struct pfq_functional * fun;

} property_t;


struct pfq_functional_node
{
 	struct pfq_functional fun;

 	init_ptr_t 	      init;
 	init_ptr_t 	      fini;

	struct pfq_functional_node *next;
};


struct pfq_computation_tree
{
        size_t size;
        struct pfq_functional_node *entry_point;
        struct pfq_functional_node node[];

};


/* function descriptors */

struct pfq_function_descr
{
        const char *    symbol;
        const char *	signature;
        void * 		ptr;
        init_ptr_t 	init;
        fini_ptr_t 	fini;
};

/* actions types */

enum action
{
        action_drop  = 0,
        action_copy  = 1,
        action_steer = 2
};

/* action attributes */

enum action_attr
{
        attr_stolen        = 0x1,
        attr_ret_to_kernel = 0x2
};


/* action */

#define Q_PERSISTENT_MEM 	64

struct pergroup_context
{
        sparse_counter_t counter[Q_MAX_COUNTERS];

	struct _persistent {

		spinlock_t 	lock;
		char 		memory[Q_PERSISTENT_MEM];

	} persistent [Q_MAX_PERSISTENT];

};

typedef struct
{
        unsigned long class_mask;
        uint32_t hash;
        uint8_t  type;
        uint8_t  attr;

} action_t;


struct pfq_cb
{
        action_t action;

        uint8_t  direct_skb;

        unsigned long group_mask;
        unsigned long state;

        struct pergroup_context *ctx;

} __attribute__((packed));


#define PFQ_CB(skb) ((struct pfq_cb *)(skb)->cb)

/* class predicates */

static inline bool
is_drop(action_t a)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
        BUILD_BUG_ON_MSG(sizeof(struct pfq_cb) > sizeof(((struct sk_buff *)0)->cb), "pfq control buffer overflow");
#endif
        return a.type == action_drop;
}

static inline bool
is_copy(action_t a)
{
        return a.type == action_copy;
}

static inline bool
is_steering(action_t a)
{
        return a.type == action_steer;
}

/* action: copy */

static inline
struct sk_buff *
copy(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->type = action_copy;
        return skb;
}

/* drop: ignore this packet for the current group */

static inline
struct sk_buff *
drop(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->type = action_drop;
        return skb;
}

/* class skb: specifies only the class for the packet */

static inline
struct sk_buff *
class(struct sk_buff *skb, uint64_t class_mask)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->class_mask = class_mask;
        return skb;
}

/* broadcast: broadcast the skb all the classes */

static inline
struct sk_buff *
broadcast(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->type  = action_copy;
        a->class_mask = Q_CLASS_ANY;
        return skb;
}

/* steering skb: for this group, steer the skb across sockets (by means of hash) */

static inline
struct sk_buff *
steering(struct sk_buff *skb, uint32_t hash)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->type  = action_steer;
        a->hash  = hash;
        return skb;
}

/* deliver: for this group, deliver the skb to the sockets of the given classes */

static inline
struct sk_buff *
deliver(struct sk_buff *skb, unsigned long class_mask)
{
        action_t * a  = & PFQ_CB(skb)->action;
        a->type       = action_copy;
        a->class_mask = class_mask;
        return skb;
}

/* class + steering: for this group, steer the skb across sockets of the given classes (by means of hash) */

static inline
struct sk_buff *
dispatch(struct sk_buff *skb, unsigned long class_mask, uint32_t hash)
{
        action_t * a = & PFQ_CB(skb)->action;
        a->type  = action_steer;
        a->class_mask = class_mask;
        a->hash  = hash;
        return skb;
}


/* steal packet: skb is stolen by the function. (i.e. forwarded) */

static inline
struct sk_buff *
steal(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;
        if (unlikely(a->attr & attr_ret_to_kernel))
        {
                if (printk_ratelimit())
                        pr_devel("[PFQ] steal modifier applied to a packet returning to kernel!\n");
                return skb;
        }
        a->attr |= attr_stolen;
        return skb;
}

/* to_kernel: set the skb to be passed to kernel */

static inline
struct sk_buff *
to_kernel(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;
        if (unlikely(a->attr & attr_stolen))
        {
                if (printk_ratelimit())
                        pr_devel("[PFQ] to_kernel modifier applied to a stolen packet!\n");
                return skb;
        }
        a->attr |= attr_ret_to_kernel;
        return skb;
}


static inline
bool is_for_kernel(struct sk_buff *skb)
{
        action_t * a = & PFQ_CB(skb)->action;

        return (a->attr & attr_ret_to_kernel) ||
               (a->class_mask == 0 && a->type == action_copy);
}


/* utility function: counter */

static inline
sparse_counter_t * get_counter(struct sk_buff *skb, int n)
{
        struct pfq_cb *cb = PFQ_CB(skb);
        if (n < 0 || n >= Q_MAX_COUNTERS)
                return NULL;

        return & cb->ctx->counter[n];
}


/* utility function: volatile state, persistent state */


static inline
unsigned long get_state(struct sk_buff const *skb)
{
        return PFQ_CB(skb)->state;
}

static inline
void set_state(struct sk_buff *skb, unsigned long state)
{
        PFQ_CB(skb)->state = state;
}

static inline void *
__get_persistent(struct sk_buff const *skb, int n)
{
	struct pergroup_context *ctx = PFQ_CB(skb)->ctx;
	spin_lock(&ctx->persistent[n].lock);
	return ctx->persistent[n].memory;
}

#define get_persistent(type, skb, n) __builtin_choose_expr(sizeof(type) <= 64, __get_persistent(skb, n) , (void)0)

static inline
void put_persistent(struct sk_buff const *skb, int n)
{
	struct pergroup_context *ctx = PFQ_CB(skb)->ctx;
	spin_unlock(&ctx->persistent[n].lock);
}


#endif /* _PF_Q_MODULE_H_ */
