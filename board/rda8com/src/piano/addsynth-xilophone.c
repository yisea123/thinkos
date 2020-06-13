/* 
 * Copyright(C) 2013 Robinson Mittmann. All Rights Reserved.
 * 
 * This file is part of the usb-serial converter.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You can receive a copy of the GNU Lesser General Public License from 
 * http://www.gnu.org/
 */

/** 
 * @file addsynth_xilophone.c
 * @brief
 * @author Robinson Mittmann <bobmittmann@gmail.com>
 */ 

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "addsynth.h"

const struct addsynth_instrument_cfg addsynth_xilophone_cfg = {
	.tag = "Xilophone",
	.note_cnt = 16,
	.note = {
		[0] = {
			.tag = "C4",
			.midi = MIDI_C4,
			.osc = { 
				[0] = { .freq = MIDI_C4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_C4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_C4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_C4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_C4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_C4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.guard_itv_ms = 50,
				.delay_itv_ms = 0,
				.attack_itv_ms = 25,
				.hold_itv_ms = 0,
				.decay_itv_ms = 2000,
				.release_itv_ms = 0,
				.sustain_lvl = 0
			}
		},
		[1] = {
			.tag = "D4",
			.midi = MIDI_D4,
			.osc = { 
				[0] = { .freq = MIDI_D4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_D4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_D4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_D4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_D4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_D4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.guard_itv_ms = 50,
				.delay_itv_ms = 0,
				.attack_itv_ms = 25,
				.hold_itv_ms = 0,
				.decay_itv_ms = 2000,
				.release_itv_ms = 0,
				.sustain_lvl = 0
			}

		},
		[2] = {
			.tag = "E4",
			.midi = MIDI_E4,
			.osc = { 
				[0] = { .freq = MIDI_E4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_E4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_E4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_E4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_E4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_E4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 3,
			}

		},
		[3] = {
			.tag = "F4",
			.midi = MIDI_F4,
			.osc = { 
				[0] = { .freq = MIDI_F4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_F4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_F4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_F4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_F4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_F4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 4,
			}

		},
		[4] = {
			.tag = "G4",
			.midi = MIDI_G4,
			.osc = { 
				[0] = { .freq = MIDI_G4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_G4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_G4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_G4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_G4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_G4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 5,
			}

		},
		[5] = {
			.tag = "A4",
			.midi = MIDI_A4,
			.osc = { 
				[0] = { .freq = MIDI_A4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_A4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_A4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_A4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_A4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_A4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 6,
			}

		},
		[6] = {
			.tag = "B4",
			.midi = MIDI_B4,
			.osc = { 
				[0] = { .freq = MIDI_B4_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_B4_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_B4_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_B4_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_B4_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_B4_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 7,
			}

		},
		[7] = {
			.tag = "C5",
			.midi = MIDI_C5,
			.osc = { 
				[0] = { .freq = MIDI_C5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_C5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_C5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_C5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_C5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_C5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 8,
			}

		},
		[8] = {
			.tag = "D5",
			.midi = MIDI_D5,
			.osc = { 
				[0] = { .freq = MIDI_D5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_D5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_D5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_D5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_D5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_D5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 9,
			}

		},
		[9] = {
			.tag = "E5",
			.midi = MIDI_F5,
			.osc = { 
				[0] = { .freq = MIDI_E5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_E5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_E5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_E5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_E5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_E5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 10,
			}

		},
		[10] = {
			.tag = "F5",
			.midi = MIDI_F5,
			.osc = { 
				[0] = { .freq = MIDI_F5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_F5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_F5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_F5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_F5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_F5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 11,
			}

		},
		[11] = {
			.tag = "G5",
			.midi = MIDI_G5,
			.osc = { 
				[0] = { .freq = MIDI_G5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_G5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_G5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_G5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_G5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_G5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 12,
			}

		},
		[12] = {
			.tag = "A5",
			.midi = MIDI_A5,
			.osc = { 
				[0] = { .freq = MIDI_A5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_A5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_A5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_A5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_A5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_A5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 13,
			}

		},
		[13] = {
			.tag = "B5",
			.midi = MIDI_B5,
			.osc = { 
				[0] = { .freq = MIDI_B5_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_B5_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_B5_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_B5_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_B5_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_B5_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 14,
			}

		},
		[14] = {
			.tag = "C6",
			.midi = MIDI_C6,
			.osc = { 
				[0] = { .freq = MIDI_C6_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_C6_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_C6_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_C6_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_C6_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_C6_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.id = 15,
			}

		},
		[15] = {
			.tag = "D6",
			.midi = MIDI_D6,
			.osc = { 
				[0] = { .freq = MIDI_D6_FREQ, .ampl = 0.333 },
				[1] = { .freq = MIDI_D6_FREQ * 2, .ampl = 0 },
				[2] = { .freq = MIDI_D6_FREQ * 3, .ampl = 0 },
				[3] = { .freq = MIDI_D6_FREQ * 4, .ampl = 0 },
				[4] = { .freq = MIDI_D6_FREQ * 5, .ampl = 0 },
				[5] = { .freq = MIDI_D6_FREQ * 6, .ampl = 0 }
			},
			.env = {
				.guard_itv_ms = 50,
				.delay_itv_ms = 0,
				.attack_itv_ms = 25,
				.hold_itv_ms = 0,
				.decay_itv_ms = 2000,
				.release_itv_ms = 0,
				.sustain_lvl = 0
			}

		},
		/* end of record */
		[16] = {
			.midi = -1,
		}
	}
};

