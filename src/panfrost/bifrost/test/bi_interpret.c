/*
 * Copyright (C) 2020 Collabora Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors (Collabora):
 *      Alyssa Rosenzweig <alyssa.rosenzweig@collabora.com>
 */

#include "bit.h"

typedef union {
        uint64_t u64;
        uint32_t u32;
        uint16_t u16[2];
        uint8_t u8[4];
        double f64;
        float f32;
        uint16_t f16;
} bit_t;

/* Interprets a subset of Bifrost IR required for automated testing */

static uint64_t
bit_read(struct bit_state *s, bi_instruction *ins, unsigned index, nir_alu_type T, bool FMA)
{
        if (index & BIR_INDEX_REGISTER) {
                uint32_t reg = index & ~BIR_INDEX_REGISTER;
                assert(reg < 64);
                return s->r[reg];
        } else if (index & BIR_INDEX_UNIFORM) {
                unreachable("Uniform registers to be implemented");
        } else if (index & BIR_INDEX_CONSTANT) {
                return ins->constant.u64 >> (index & ~BIR_INDEX_CONSTANT);
        } else if (index & BIR_INDEX_ZERO) {
                return 0;
        } else if (index & (BIR_INDEX_PASS | BIFROST_SRC_STAGE)) {
                return FMA ? 0 : s->T;
        } else if (index & (BIR_INDEX_PASS | BIFROST_SRC_PASS_FMA)) {
                return s->T0;
        } else if (index & (BIR_INDEX_PASS | BIFROST_SRC_PASS_ADD)) {
                return s->T1;
        } else if (!index) {
                /* Placeholder */
                return 0;
        } else {
                unreachable("Invalid source");
        }
}

static void
bit_write(struct bit_state *s, unsigned index, nir_alu_type T, bit_t value, bool FMA)
{
        /* Always write stage passthrough */
        if (FMA)
                s->T = value.u32;

        if (index & BIR_INDEX_REGISTER) {
                uint32_t reg = index & ~BIR_INDEX_REGISTER;
                assert(reg < 64);
                s->r[reg] = value.u32;
        } else if (!index) {
                /* Nothing to do */
        } else {
                unreachable("Invalid destination");
        }
}

void
bit_step(struct bit_state *s, bi_instruction *ins, bool FMA)
{
        /* First, load sources */
        bit_t srcs[BIR_SRC_COUNT] = { 0 };

        bi_foreach_src(ins, src)
                srcs[src].u64 = bit_read(s, ins, ins->src[src], ins->src_types[src], FMA);

        /* Next, do the action of the instruction */
        bit_t dest = { 0 };

        switch (ins->type) {
        case BI_ADD:
        case BI_ATEST:
        case BI_BRANCH:
        case BI_CMP:
        case BI_BLEND:
        case BI_BITWISE:
        case BI_COMBINE:
        case BI_CONVERT:
        case BI_CSEL:
        case BI_DISCARD:
        case BI_FMA:
        case BI_FREXP:
        case BI_ISUB:
        case BI_LOAD:
        case BI_LOAD_UNIFORM:
        case BI_LOAD_ATTR:
        case BI_LOAD_VAR:
        case BI_LOAD_VAR_ADDRESS:
        case BI_MINMAX:
        case BI_MOV:
        case BI_SHIFT:
        case BI_STORE:
        case BI_STORE_VAR:
        case BI_SPECIAL: /* _FAST, _TABLE on supported GPUs */
        case BI_SWIZZLE:
        case BI_TEX:
        case BI_ROUND:
        default:
                unreachable("Unsupported op");
        }

        /* Finally, store the result */
        bit_write(s, ins->dest, ins->dest_type, dest, FMA);

        /* For ADD - change out the passthrough */
        if (!FMA) {
                s->T0 = s->T;
                s->T1 = dest.u32;
        }
}