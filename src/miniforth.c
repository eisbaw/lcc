/* LCC backend for Miniforth VM - based on bytecode.c */
#include "c.h"
#include <ctype.h>
#define I(f) miniforth_##f

static char rcsid[] = "$Id: miniforth.c $";

/* Miniforth opcodes */
#define OP_EXIT  0x00
#define OP_LIT   0x01
#define OP_DUP   0x02
#define OP_DROP  0x03
#define OP_SWAP  0x04
#define OP_ADD   0x05
#define OP_SUB   0x06
#define OP_MUL   0x07
#define OP_DIV   0x08
#define OP_EQ    0x09
#define OP_LT    0x0A
#define OP_FETCH 0x0B
#define OP_STORE 0x0C
#define OP_JMP   0x0D
#define OP_JZ    0x0E
#define OP_CALL  0x0F

static int localoffset;

static void I(segment)(int n) {
    static int cseg;
    
    if (cseg != n)
        switch (cseg = n) {
        case CODE: print("; code section\n"); return;
        case DATA: print("; data section\n"); return; 
        case BSS:  print("; bss section\n");  return;
        case LIT:  print("; literal section\n");  return;
        default: assert(0);
        }
}

static void I(address)(Symbol q, Symbol p, long n) {
    q->x.name = stringf("%s%s%D", p->x.name, n > 0 ? "+" : "", n);
}

static void I(defaddress)(Symbol p) {
    print("; address %s\n", p->x.name);
}

static void I(defconst)(int suffix, int size, Value v) {
    switch (suffix) {
    case I:
        if (size > sizeof (int))
            print("%d %D ; const int %d bytes\n", OP_LIT, v.i, size);
        else  
            print("%d %d ; const int\n", OP_LIT, v.i);
        return;
    case U:
        if (size > sizeof (unsigned))
            print("%d %U ; const uint %d bytes\n", OP_LIT, v.u, size);
        else
            print("%d %u ; const uint\n", OP_LIT, v.u);
        return;
    case P: 
        print("%d %U ; const pointer\n", OP_LIT, (unsigned long)v.p); 
        return;
    case F:
        if (size == 4) {
            float f = v.d;
            print("%d %u ; const float\n", OP_LIT, *(unsigned *)&f);
        } else {
            /* For double, split into two literals */
            double d = v.d;
            unsigned *p = (unsigned *)&d;
            print("%d %u ; const double lo\n", OP_LIT, p[0]);
            print("%d %u ; const double hi\n", OP_LIT, p[1]);
        }
        return;
    }
    assert(0);
}

static void I(defstring)(int len, char *str) {
    /* Emit string as sequence of byte literals */
    int i;
    print("; string literal length %d\n", len);
    for (i = 0; i < len; i++) {
        print("%d %d ; char '%c'\n", OP_LIT, (unsigned char)str[i], 
              isprint(str[i]) ? str[i] : '?');
    }
}

static void I(defsymbol)(Symbol p) {
    if (p->scope >= LOCAL && p->sclass == STATIC)
        p->x.name = stringf("L%d", genlabel(1));
    else if (p->generated)
        p->x.name = stringf("L%d", p->x.offset);
    else if (p->scope == GLOBAL || p->sclass == EXTERN || p->sclass == STATIC)
        p->x.name = stringf("_%s", p->name);
    else
        p->x.name = p->name;
}

static void I(export)(Symbol p) {
    print("; export %s\n", p->name);
}

static void I(function)(Symbol f, Symbol caller[], Symbol callee[], int n) {
    int i;
    print("\n; function %s\n", f->name);
    print(":%s ; function entry\n", f->x.name);
    localoffset = 0;
    for (i = 0; callee[i]; i++) {
        localoffset = roundup(localoffset, callee[i]->type->align);
        callee[i]->x.offset = localoffset;
        callee[i]->x.name = stringf("%d", localoffset);
        localoffset += callee[i]->type->size;
    }
}

static Node I(gen)(Node p) {
    assert(p);
    switch (specific(p->op)) {
    case ADDRL+P: case ADDRF+P: 
        print("%d %s ; address of %s\n", OP_LIT, p->syms[0]->x.name, p->syms[0]->name);
        break;
    case CNST+I: case CNST+P: case CNST+U:
        miniforth_defconst(p->op, opsize(p->op), p->syms[0]->u.c.v);
        break;
    case ARG+B: case ARG+I: case ARG+F:
    case ARG+P: case ARG+U:
        /* Arguments are passed on stack - no action needed */
        break;
    case ASGN+B: case ASGN+F: case ASGN+I:
    case ASGN+P: case ASGN+U:
        print("%d ; store\n", OP_STORE);
        break;
    case INDIR+B: case INDIR+F: case INDIR+I:
    case INDIR+P: case INDIR+U:
        print("%d ; fetch\n", OP_FETCH);
        break;
    case CVF+I: case CVI+F: case CVP+U: case CVU+I: case CVU+P:
        /* Type conversions - no-op for our simple VM */
        break;
    case NEG+F: case NEG+I:
        print("%d 0 ; negate: push 0\n", OP_LIT);
        print("%d ; swap\n", OP_SWAP);
        print("%d ; subtract\n", OP_SUB);
        break;
    case ADD+F: case ADD+I: case ADD+U: case ADD+P:
        print("%d ; add\n", OP_ADD);
        break;
    case SUB+F: case SUB+I: case SUB+U: case SUB+P:
        print("%d ; subtract\n", OP_SUB);
        break;
    case MUL+F: case MUL+I: case MUL+U:
        print("%d ; multiply\n", OP_MUL);
        break;
    case DIV+F: case DIV+I: case DIV+U:
        print("%d ; divide\n", OP_DIV);
        break;
    case EQ+F: case EQ+I: case EQ+U:
        print("%d ; equal\n", OP_EQ);
        break;
    case LT+F: case LT+I: case LT+U:
        print("%d ; less than\n", OP_LT);
        break;
    case GT+F: case GT+I: case GT+U:
        print("%d ; greater than (swap+lt)\n", OP_SWAP);
        print("%d ; less than\n", OP_LT);
        break;
    case LE+F: case LE+I: case LE+U:
        print("%d ; less/equal (gt+not)\n", OP_SWAP);
        print("%d ; less than\n", OP_LT); 
        print("%d 0 ; push 0\n", OP_LIT);
        print("%d ; equal (logical not)\n", OP_EQ);
        break;
    case GE+F: case GE+I: case GE+U:
        print("%d ; greater/equal (lt+not)\n", OP_LT);
        print("%d 0 ; push 0\n", OP_LIT);  
        print("%d ; equal (logical not)\n", OP_EQ);
        break;
    case NE+F: case NE+I: case NE+U:
        print("%d ; not equal\n", OP_EQ);
        print("%d 0 ; push 0\n", OP_LIT);
        print("%d ; equal (logical not)\n", OP_EQ);
        break;
    case RET+V:
        print("%d ; return void\n", OP_EXIT);
        break;
    case RET+B: case RET+F: case RET+I:
    case RET+P: case RET+U:
        print("%d ; return value\n", OP_EXIT);
        break;
    default:
        print("; unhandled op %s\n", opname(p->op));
        break;
    }
    return p;
}

static void I(global)(Symbol p) {
    print("; global %s at address %s\n", p->name, p->x.name);
}

static void I(import)(Symbol p) {
    print("; import %s\n", p->name);
}

static void I(local)(Symbol p) {
    localoffset = roundup(localoffset, p->type->align);
    p->x.name = stringf("%d", localoffset);
    p->x.offset = localoffset;
    localoffset += p->type->size;
    print("; local %s at offset %d\n", p->name, p->x.offset);
}

static void I(progbeg)(int argc, char *argv[]) {
    print("; Miniforth bytecode generated by LCC\n");
    print("; Target: Miniforth Stack VM\n\n");
}

static void I(progend)(void) {
    print("\n; end of program\n");
    print("%d ; exit\n", OP_EXIT);
}

static void I(space)(int n) {
    print("; space %d bytes\n", n);
    /* For our VM, we might emit NOP instructions or reserve space */
    while (n-- > 0) {
        print("%d 0 ; space padding\n", OP_LIT);
        print("%d ; drop padding\n", OP_DROP);
    }
}

/* Stub implementations for interface completeness */
static void I(blockbeg)(Env *e) { }
static void I(blockend)(Env *e) { }
static void I(emit)(Node p) { gen(p); }

/* Interface structure for LCC */
Interface miniforthIR = {
    1, 1, 0,  /* char */
    2, 2, 0,  /* short */
    4, 4, 0,  /* int */
    4, 4, 0,  /* long */
    4, 4, 0,  /* long long */
    4, 4, 1,  /* float */
    8, 8, 1,  /* double */
    8, 8, 1,  /* long double */
    4, 4, 0,  /* T* */
    0, 4, 0,  /* struct */
    1,        /* little endian */
    0,   /* use calls for multiply ops */
    0,        /* wants callb */
    0,        /* wants argb */
    1,        /* left to right */
    0,        /* wants dag */
    0,        /* unsigned char */
    I(address),
    I(blockbeg),
    I(blockend), 
    I(defaddress),
    I(defconst),
    I(defstring),
    I(defsymbol),
    I(emit),
    I(export),
    I(function),
    I(gen),
    I(global),
    I(import),
    I(local),
    I(progbeg),
    I(progend),
    I(segment),
    I(space),
    0,  /* wants stabblock */
    0,  /* wants stabend */
    0,  /* wants stabfend */
    0,  /* wants stabinit */
    0,  /* wants stabline */
    0,  /* wants stabsym */
    0,  /* wants stabtype */
};