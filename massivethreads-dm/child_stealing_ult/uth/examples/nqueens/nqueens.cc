/**********************************************************************************************/
/*  This program is part of the Barcelona OpenMP Tasks Suite                                  */
/*  Copyright (C) 2009 Barcelona Supercomputing Center - Centro Nacional de Supercomputacion  */
/*  Copyright (C) 2009 Universitat Politecnica de Catalunya                                   */
/*                                                                                            */
/*  This program is free software; you can redistribute it and/or modify                      */
/*  it under the terms of the GNU General Public License as published by                      */
/*  the Free Software Foundation; either version 2 of the License, or                         */
/*  (at your option) any later version.                                                       */
/*                                                                                            */
/*  This program is distributed in the hope that it will be useful,                           */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                            */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                             */
/*  GNU General Public License for more details.                                              */
/*                                                                                            */
/*  You should have received a copy of the GNU General Public License                         */
/*  along with this program; if not, write to the Free Software                               */
/*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA            */
/**********************************************************************************************/

/*
 * Original code from the Cilk project (by Keith Randall)
 * 
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 */

#include <uth.h>
#include <uth-dtbb.h>
#include <madm_comm.h>
#include <memory>
#include <functional>
#include <cstdlib>
#include <cstdio>
#include <cassert>

namespace uth = madm::uth;

#ifndef BINARY_TASKS
#define BINARY_TASKS  1
#endif

/* Checking information */

static long solutions[] = {
        1,
        0,
        0,
        2,
        10, /* 5 */
        4,
        40,
        92,
        352,
        724, /* 10 */
        2680,
        14200,
        73712,
        365596,
        2279184, /* 15 */
        14772512,
        95815104,
        666090624,
        4968057848,
        39029188884, /* 20 */
};
#define MAX_SOLUTIONS ((int)(sizeof(solutions)/sizeof(int)))

static int g_n_cutoff = 0;
static long g_nodes = 0;

/*
 * <a> contains array of <n> queen positions.  Returns 1
 * if none of the queens conflict, and returns 0 otherwise.
 */
static int ok(int n, char *a)
{
     int i, j;
     char p, q;

     for (i = 0; i < n; i++) {
         p = a[i];

         for (j = i + 1; j < n; j++) {
             q = a[j];
             if (q == p || q == p - (j - i) || q == p + (j - i))
                 return 0;
         }
     }
     return 1;
}

#if 0
static long nqueens_ser(int n, int j, char *a)
{
    int i;

    if (n == j) {
        /* good solution, count it */
        return 1;
    }

    long solutions = 0;

    /* try each possible position for queen <j> */
    for (i = 0; i < n; i++) {
        /* allocate a temporary array and copy <a> into it */
        a[j] = (char) i;
        printf("a[%d] = %d\n", j, i);
        if (ok(j + 1, a)) {
            printf("ok: (i, j) = (%d, %d)\n", i, j + 1);
            solutions += nqueens_ser(n, j + 1, a);
        } else {
            printf("ng: (i, j) = (%d, %d)\n", i, j + 1);
        }
    }

    return solutions;
}
#endif

enum constants {
    MAX_N = 20
};

struct board {
    char array[MAX_N];
};

typedef struct {
    int begin;
    int end;
    int n;
    int j;
    char *a;
    int depth;
} nqueens_t;

//-- sequential ---------------------------------------------------------------

static long nqueens_seq(int n, int j, board b, int depth);

static long
call_tasks_seq(int begin, int end, int n, int j, board &b, int depth)
{
    int i;
    long solutions = 0;

    for (i = begin; i < end; i++) {
        b.array[j] = (char) i;
        if (ok(j + 1, b.array)) {
            solutions += nqueens_seq(n, j + 1, b, depth + 1);
        }
    }

    return solutions;
}

static long
spawn_rec_tasks_seq(int begin, int end, int n, int j, board b, int depth)
{
    if (end - begin == 1) {
        int i = begin;

        b.array[j] = (char)i;
        if (ok(j + 1, b.array)) {
            return nqueens_seq(n, j + 1, b, depth + 1);
        } else {
            return 0;
        }
    } else {
        int center = (begin + end) / 2;

        long solutions0 = spawn_rec_tasks_seq(begin, center, n, j, b, depth);
        long solutions1 = spawn_rec_tasks_seq(center, end, n, j, b, depth);

        return solutions0 + solutions1;
    }
}

static long nqueens_seq(int n, int j, board b, int depth)
{
    g_nodes += 1;

    if (n == j) {
        /* good solution, count it */
        return 1;
    } else {
        long solutions;
        /* try each possible position for queen <j> */
        if (g_n_cutoff != 0 && depth >= g_n_cutoff) {
            solutions = call_tasks_seq(0, n, n, j, b, depth);
        } else {
            #if BINARY_TASKS
                solutions = spawn_rec_tasks_seq(0, n, n, j, b, depth);
            #else
#error "not implemented"
//                solutions = spawn_loop_tasks(0, n, n, j, b, depth);
            #endif
        }

        return solutions;
    }
}

static long nqueens_root_seq(int n)
{
    board b;
    return nqueens_seq(n, 0, b, 0);
}



//-- parallel -----------------------------------------------------------------

static long nqueens(int n, int j, board b, int depth);

static long
call_tasks(int begin, int end, int n, int j, board &b, int depth)
{
    int i;
    long solutions = 0;

    for (i = begin; i < end; i++) {
        b.array[j] = (char) i;
        if (ok(j + 1, b.array)) {
            solutions += nqueens(n, j + 1, b, depth + 1);
        }
    }

    return solutions;
}

static long nqueens(int n, int j, board b, int depth)
{
    int i;

    g_nodes += 1;

    if (n == j) {
        /* good solution, count it */
        return 1;
    } else {
        long solutions;
        /* try each possible position for queen <j> */
        if (g_n_cutoff != 0 && depth >= g_n_cutoff) {
            solutions = call_tasks(0, n, n, j, b, depth);
        } else {
            #if BINARY_TASKS
                solutions = uth::dtbb::parallel_reduce(0, n, 0,
                    [=] (int i) {
                        board brd = b; // copy for rewrite

                        brd.array[j] = (char)i;
                        if (ok(j + 1, brd.array)) {
                            return nqueens(n, j + 1, brd, depth + 1);
                        } else {
                            return 0L;
                        }
                    },
                    std::plus<long>());
            #else
                solutions = spawn_loop_tasks(0, n, n, j, b, depth);
            #endif
        }

        return solutions;
    }
}

static long nqueens_root(int n)
{
    board b;
    return nqueens(n, 0, b, 0);
}

//-- main ---------------------------------------------------------------------

long run(long n)
{
    if (getenv("SEQ") != NULL) {
        return nqueens_root_seq(n);
    } else {
        uth::thread<long> f(nqueens_root, n);
        return f.join();
    }
}

static bool verify_nqueens(int size, long total_count)
{
    if (size > MAX_SOLUTIONS) return false;
    if (total_count == solutions[size - 1]) return true;
    return false;
}

void real_main(int argc, char **argv)
{
    const int n_args = 1;
    if (argc != n_args + 1) {
        fprintf(stderr, "usage: %s N\n", argv[0]);
        exit(1);
    }
    int n = atoi(argv[1]);

    uth::pid_t me = uth::get_pid();
    size_t n_procs = uth::get_n_procs();

    if (me == 0) {
        size_t server_mod = madi::comm::get_server_mod();
        printf("program = nqueens, N = %d, np = %zu, server_mod = %zu\n",
               n, n_procs, server_mod);
        uth::print_options(stdout);
    }
    uth::barrier();

    double t0 = uth::time();

    g_nodes = 0;
    long total_count = 0;
    if (me == 0) {
        total_count = run(n);
    }

    double t1 = uth::time();

    uth::barrier();

    long nodes;
    madi::comm::reduce(&nodes, &g_nodes, 1, 0, madi::comm::reduce_op_sum);

    if (me == 0) {
        if (!verify_nqueens(n, total_count)) {
            printf("result = error,\n");
            exit(1);
        }
        
        double time = t1 - t0;
        double throughput = 1e-6 * (double)nodes / time;
        

        printf("result = %ld,\n", total_count);
        printf("time = %.6lf, nodes = %9zu,\n"
               "throughput = %.3lf, throughput/np = %.3lf,\n",
               time, nodes, throughput, throughput / (double)n_procs);

    }
}

int main(int argc, char **argv)
{ 
    uth::start(real_main, argc, argv); 
    return 0;
}
