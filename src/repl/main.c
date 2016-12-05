#include <stdio.h>
#include <stdlib.h>

int main()
{
	lip_runtime_t* runtime = lip_create_runtime(lip_default_allocator, NULL);

	lip_zone_t zone = lip_create_zone(runtime);
	lip_destroy_zone(zone);

	lip_vm_t* vm = lip_create_vm(runtime, zone);
	lip_destroy_vm(vm);

	lip_script_t* script = lip_load_script(runtime, input);
	lip_exec(vm, script, &result);
	lip_release_script(script);

	lip_value_t* value = lip_lookup(runtime, symbol);

	lip_push_value(vm, script);
	lip_call(vm, argc, &result);
	lip_resume(vm);

	lip_runtime_destroy(runtime);

	return EXIT_SUCCESS;
}
