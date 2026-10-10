#pragma once
struct hm_fixup { unsigned int data_addr; unsigned int orig_target; void *target; };
struct hm_funcmap { unsigned int orig_addr; void *fn; };
extern const struct hm_fixup hm_fixups[];
extern const struct hm_funcmap hm_funcmap[];
