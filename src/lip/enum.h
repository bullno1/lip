#ifndef LIP_ENUM_H
#define LIP_ENUM_H

#define LIP_ENUM(NAME, FOREACH) \
	LIP_DEFINE_ENUM(NAME, FOREACH) \
	LIP_DEFINE_ENUM_TO_STR(NAME, FOREACH)

#define LIP_DEFINE_ENUM(NAME, FOREACH) \
	typedef enum NAME { FOREACH(LIP_GEN_ENUM) } NAME;
#define LIP_GEN_ENUM(ENUM) ENUM,

#define LIP_DEFINE_ENUM_TO_STR(NAME, FOREACH) \
	static inline const char* NAME##_to_str(NAME value) { \
		switch(value) { \
			FOREACH(LIP_DEFINE_ENUM_CASE) \
			default: return 0; \
		} \
	}
#define LIP_DEFINE_ENUM_CASE(ENUM) case ENUM : return #ENUM ;

#endif
