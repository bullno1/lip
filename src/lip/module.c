#include "module.h"
#include <stdio.h>

void lip_module_print(lip_module_t* module)
{
	for(size_t i = 0; i < module->num_symbols; ++i)
	{
		printf("%.*s:\n", module->symbols[i]->length, module->symbols[i]->ptr);
		lip_value_print(&module->values[i], 1);
		printf("\n");
	}
}
