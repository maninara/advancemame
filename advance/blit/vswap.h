/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 1999-2002 Andrea Mazzoleni
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#ifndef __VSWAP_H
#define __VSWAP_H

#include "blit.h"

#if defined(USE_ASM_i586)
static inline unsigned internal_swapeven8_step1_mmx(unsigned state, uint8* buffer, uint8* dst, const uint8* src, unsigned count)
{
	switch (state) {
	case 0 :
		internal_copy8_mmx(dst, src, count);
		internal_copy8_mmx(buffer, src, count);
		state = 1;
		break;
	case 1 :
		internal_copy8_mmx(dst, src, count);
		state = 2;
		break;
	case 2 :
		internal_copy8_mmx(dst, buffer, count);
		internal_copy8_mmx(buffer, src, count);
		state = 1;
		break;
	}
	return state;
}

static inline unsigned internal_swapodd8_step1_mmx(unsigned state, uint8* buffer, uint8* dst, const uint8* src, unsigned count)
{
	switch (state) {
	case 0 :
		internal_copy8_mmx(dst, src, count);
		internal_copy8_mmx(buffer, src, count);
		state = 1;
		break;
	case 1 :
		internal_copy8_mmx(dst, buffer, count);
		internal_copy8_mmx(buffer, src, count);
		state = 2;
		break;
	case 2 :
		internal_copy8_mmx(dst, src, count);
		state = 1;
		break;
	}
	return state;
}
#endif

static inline unsigned internal_swapeven8_step1_def(unsigned state, uint8* buffer, uint8* dst, const uint8* src, unsigned count)
{
	switch (state) {
	case 0 :
		internal_copy8_def(dst, src, count);
		internal_copy8_def(buffer, src, count);
		state = 1;
		break;
	case 1 :
		internal_copy8_def(dst, src, count);
		state = 2;
		break;
	case 2 :
		internal_copy8_def(dst, buffer, count);
		internal_copy8_def(buffer, src, count);
		state = 1;
		break;
	}
	return state;
}

static inline unsigned internal_swapodd8_step1_def(unsigned state, uint8* buffer, uint8* dst, const uint8* src, unsigned count)
{
	switch (state) {
	case 0 :
		internal_copy8_def(dst, src, count);
		internal_copy8_def(buffer, src, count);
		state = 1;
		break;
	case 1 :
		internal_copy8_def(dst, buffer, count);
		internal_copy8_def(buffer, src, count);
		state = 2;
		break;
	case 2 :
		internal_copy8_def(dst, src, count);
		state = 1;
		break;
	}
	return state;
}

/****************************************************************************/
/* swap8 */

#if defined(USE_ASM_i586)
static void video_line_swapeven8_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count);
}

static void video_line_swapodd8_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count);
}
#endif

static void video_line_swapeven8_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count);
}

static void video_line_swapodd8_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count);
}

static void video_stage_swapeven8_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_even, sdx, sdp, 1, sdx, 1);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapeven8_step1), 0);
}

static void video_stage_swapodd8_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_odd, sdx, sdp, 1, sdx, 1);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapodd8_step1), 0);
}

/****************************************************************************/
/* swap16 */

#if defined(USE_ASM_i586)
static void video_line_swapeven16_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 2);
}

static void video_line_swapodd16_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 2);
}
#endif

static void video_line_swapeven16_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 2);
}

static void video_line_swapodd16_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 2);
}

static void video_stage_swapeven16_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_even, sdx, sdp, 2, sdx, 2);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapeven16_step1), 0);
}

static void video_stage_swapodd16_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_odd, sdx, sdp, 2, sdx, 2);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapodd16_step1), 0);
}

/****************************************************************************/
/* swap32 */

#if defined(USE_ASM_i586)
static void video_line_swapeven32_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 4);
}

static void video_line_swapodd32_step1_mmx(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_mmx(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 4);
}
#endif

static void video_line_swapeven32_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapeven8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 4);
}

static void video_line_swapodd32_step1_def(const struct video_stage_horz_struct* stage, void* dst, const void* src)
{
	((struct video_stage_horz_struct*)stage)->state_mutable = internal_swapodd8_step1_def(stage->state_mutable, (uint8*)stage->buffer_extra, (uint8*)dst, (const uint8*)src, stage->slice.count * 4);
}

static void video_stage_swapeven32_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_even, sdx, sdp, 4, sdx, 4);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapeven32_step1), 0);
}

static void video_stage_swapodd32_set(struct video_stage_horz_struct* stage, unsigned sdx, int sdp)
{
	STAGE_SIZE(stage, pipe_swap_odd, sdx, sdp, 4, sdx, 4);
	STAGE_EXTRA(stage);
	STAGE_PUT(stage, BLITTER(video_line_swapodd32_step1), 0);
}

#endif
