// Copyright (c) 2017 Walter Kuppens
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdbool.h>
#include <stdio.h>

#include "program.h"
#include "utils.h"

#define INSTRUCTION_ALLOC_COUNT 1024
#define BF_MAX_PROGRAM_SIZE 65536

struct bf_program *bf_program_create()
{
    struct bf_program *program = calloc(1, sizeof(struct bf_program));
    if (!program) {
        goto error1;
    }

    struct bf_instruction *ir = calloc(1, sizeof(struct bf_instruction) * INSTRUCTION_ALLOC_COUNT);
    if (!ir) {
        goto error2;
    }

    program->size = 0;
    program->capacity = INSTRUCTION_ALLOC_COUNT;
    program->ir = ir;

    return program;

error2:
    free(program);
error1:
    return NULL;
}

void bf_program_destroy(struct bf_program *program)
{
    free(program->ir);
    free(program);
}

/**
 * Unconditionally increases the capacity of contiguous memory holding the
 * bytecode by INSTRUCTION_ALLOC_COUNT * sizeof(struct bf_instruction) bytes.
 */
bool bf_program_grow(struct bf_program *program)
{
    struct bf_instruction *resized_ir;
    size_t new_capacity;

    new_capacity = program->capacity + INSTRUCTION_ALLOC_COUNT;

    // Prevent the capacity from going over 65536 bytes in size. This limitation
    // is imposed so branch instructions can use smaller 16-bit addresses. Most
    // brainfuck programs, including stress tests such as mandlebrot.b and
    // hanoi.b, fit in this space within a great margin.
    if (new_capacity > BF_MAX_PROGRAM_SIZE) {
        if (program->capacity < BF_MAX_PROGRAM_SIZE) {
            new_capacity = BF_MAX_PROGRAM_SIZE;
        } else {
            goto error1;
        }
    }

    resized_ir = realloc(program->ir, sizeof(struct bf_instruction) * new_capacity);
    if (!resized_ir) {
        goto error1;
    }

    program->ir = resized_ir;
    program->capacity = new_capacity;

    return true;

error1:
    return false;
}

/**
 * Appends an instruction to the end of the program. This function will also
 * allocate more space by calling 'bf_program_grow' if there isn't enough room
 * for the new instruction.
 */
bool bf_program_append(struct bf_program *program, const struct bf_instruction instruction)
{
    if (program->size >= program->capacity) {
        if (!bf_program_grow(program)) {
            goto error1;
        }
    }

    program->ir[program->size] = instruction;
    program->size++;

    return true;

error1:
    return false;
}

/**
 * Substitutes existing IR code with new IR at a desired position. If the size
 * of the new IR would cause an overwrite, the operation is cancelled and
 * 'false' is returned to indicate failure.
 */
bool bf_program_substitute(struct bf_program *program, const struct bf_instruction *ir, int pos, size_t size)
{
    if (pos + size >= program->size) {
        return false;
    }

    for (int i = 0; i < size; i++) {
        program->ir[pos + i] = ir[i];
    }

    return true;
}

int bf_program_match_sequence(struct bf_program *program, const struct bf_pattern_rule *rules, int pos, size_t size)
{
    int real_iters = 0;
    int mut_size = size;

    if (pos + size >= program->size || size <= 0) {
        return 0;
    }

    for (int i = 0; i < mut_size && (pos + mut_size) < program->size; i++) {
        struct bf_instruction instr = program->ir[pos + i];
        struct bf_pattern_rule rule = rules[real_iters];

        if (instr.opcode == BF_INS_NOP) {
            mut_size++;
            continue;
        }

        // Opcodes should always match regardless of the flags.
        if (instr.opcode != rule.instruction.opcode) {
            return 0;
        }

        // Ensure arguments match when strict mode is enabled.
        bool is_strict = bf_utils_check_flag(rule.flags, BF_PATTERN_STRICT);
        if (is_strict && instr.argument != rule.instruction.argument) {
            return 0;
        }

        real_iters++;
    }

    // This prevents a sequence of NOPs at the end of a program from tricking
    // the match function into returning true.
    if (real_iters == size) {
        return mut_size;
    } else {
        return 0;
    }
}

void bf_program_dump(const struct bf_program *program)
{
    struct bf_instruction *instr;

    for (int i = 0; i < program->size; i++) {
        instr = &program->ir[i];
        printf("(0x%08x) %-9s -> 0x%08x (%d), Offset: %d\n", i, bf_program_map_ins_name(instr->opcode), instr->argument, instr->argument, instr->offset);
    }
}

/**
 * Uses a massive switch to map enum values to strings.
 */
const char *bf_program_map_ins_name(enum bf_opcode opcode)
{
    switch (opcode) {
    case BF_INS_NOP:
        return "NOP";
    case BF_INS_IN:
        return "IN";
    case BF_INS_OUT:
        return "OUT";
    case BF_INS_INC_V:
        return "INC_V";
    case BF_INS_DEC_V:
        return "DEC_V";
    case BF_INS_ADD_V:
        return "ADD_V";
    case BF_INS_SUB_V:
        return "SUB_V";
    case BF_INS_INC_P:
        return "INC_P";
    case BF_INS_DEC_P:
        return "DEC_P";
    case BF_INS_ADD_P:
        return "ADD_P";
    case BF_INS_SUB_P:
        return "SUB_P";
    case BF_INS_BRANCH_Z:
        return "BRANCH_Z";
    case BF_INS_BRANCH_NZ:
        return "BRANCH_NZ";
    case BF_INS_JMP:
        return "JMP";
    case BF_INS_HALT:
        return "HALT";
    case BF_INS_CLEAR:
        return "CLEAR";
    case BF_INS_COPY:
        return "COPY";
    case BF_INS_MUL:
        return "MUL";
    default:
        return "?";
    }
}
