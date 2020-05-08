/* 
 * File:	 dac.h
 * Author:   Robinson Mittmann (bobmittmann@gmail.com)
 * Target:
 * Comment:
 * Copyright(C) 2020 Bob Mittmann. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __TONEGEN_H__
#define __TONEGEN_H__

#include <stdint.h>

struct tonegen {
	float a;
	float t;
	struct {
		volatile int32_t p;
		int32_t dp;
	} osc;
	struct {
		float e0;
		volatile float e1;
		volatile float e2;
		volatile float e3;
		volatile float c1;
		volatile float c2;
		volatile float c3;
	} env;
};

#ifdef __cplusplus
extern "C" {
#endif


int tonegen_init(struct tonegen *tone, float samplerate, float ampl);

int tonegen_reset(struct tonegen *tone);

int tonegen_release(struct tonegen *tone);

int tonegen_set(struct tonegen *tone, float freq, 
				float ampl, uint32_t k1, uint32_t k2);

int tonegen_env_set(struct tonegen *tone, uint32_t k1, uint32_t k2);

int tonegen_pcm_encode(struct tonegen *tone, 
					   float pcm[], unsigned int len);

#ifdef __cplusplus
}
#endif
#endif /* __TONEGEN_H__ */

