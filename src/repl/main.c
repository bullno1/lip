#include <stdlib.h>
#include <lip/lip.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/print.h>
#include <linenoise.h>

struct repl_context_s
{
	lip_repl_handler_t vtable;
	lip_context_t* ctx;
	char* line_buff;
	size_t read_pos;
	size_t buff_len;
	bool running;
};

static size_t
repl_read(lip_repl_handler_t* vtable, void* buff, size_t size)
{
	struct repl_context_s* repl = LIP_CONTAINER_OF(vtable, struct repl_context_s, vtable);

	if(!repl->running) { return 0; }

	if(repl->read_pos >= repl->buff_len)
	{
		char* line;
		for(;;)
		{
			line = linenoise("lip> ");

			if(line == 0)
			{
				return 0;
			}
			else if(line[0] == '\0')
			{
				free(line);
			}
			else
			{
				break;
			}
		}

		linenoiseHistoryAdd(line);

		size_t line_length = strlen(line);
		if(line_length + 1 > repl->buff_len)
		{
			repl->line_buff =
				lip_realloc(lip_default_allocator, repl->line_buff, line_length + 1);
		}
		memcpy(repl->line_buff, line, line_length);
		repl->line_buff[line_length] = '\n';
		repl->read_pos = 0;
		repl->buff_len = line_length + 1;
		free(line);
	}

	size_t num_bytes_to_read = LIP_MIN(repl->buff_len - repl->read_pos, size);
	memcpy(buff, &repl->line_buff[repl->read_pos], num_bytes_to_read);
	repl->read_pos += num_bytes_to_read;
	return num_bytes_to_read;
}

static void
repl_print(lip_repl_handler_t* vtable, lip_exec_status_t status, lip_value_t result)
{
	struct repl_context_s* repl = LIP_CONTAINER_OF(vtable, struct repl_context_s, vtable);
	if(status == LIP_EXEC_OK)
	{
		lip_print_value(5, 0, lip_stdout(), result);
	}
	else
	{
		const lip_context_error_t* err = lip_get_error(repl->ctx);
		lip_printf(
			lip_stdout(),
			"Error: %.*s.\n",
			(int)err->message.length, err->message.ptr
		);
		for(unsigned int i = 0; i < err->num_records; ++i)
		{
			lip_error_record_t* record = &err->records[i];
			lip_printf(
				lip_stdout(),
				"  %.*s:%u:%u - %u:%u: %.*s.\n",
				(int)record->filename.length, record->filename.ptr,
				record->location.start.line,
				record->location.start.column,
				record->location.end.line,
				record->location.end.column,
				(int)record->message.length, record->message.ptr
			);
		}

		repl->running = false;
	}
}

int main()
{
	lip_runtime_t* runtime = lip_create_runtime(lip_default_allocator);
	lip_context_t* ctx = lip_create_context(runtime, lip_default_allocator);
	lip_load_builtins(ctx);

	lip_vm_config_t vm_config = {
		.os_len = 256,
		.cs_len = 256,
		.env_len = 256
	};
	lip_vm_t* vm = lip_create_vm(ctx, &vm_config);

	struct repl_context_s repl_context = {
		.read_pos = 1,
		.running = true,
		.ctx = ctx,
		.vtable = { .read = repl_read, .print = repl_print }
	};
	linenoiseInstallWindowChangeHandler();
	lip_repl(vm, &repl_context.vtable);

	lip_destroy_runtime(runtime);
	lip_free(lip_default_allocator, repl_context.line_buff);

	return repl_context.running ? EXIT_SUCCESS : EXIT_FAILURE;
}
