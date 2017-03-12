#include "dbg.h"
#include <inttypes.h>
#include <lip/memory.h>
#include <lip/io.h>
#include <lip/vm.h>
#include <lip/array.h>
#include <lip/print.h>
#include <lip/asm.h>
#include <cmp.h>

#if LIP_DBG_EMBED_RESOURCES == 1
#	include <index.html.h>
#	include <bundle.js.h>
#	include <bundle.js.map.h>
#endif

#define WBY_STATIC
#define WBY_IMPLEMENTATION
#include "vendor/wby.h"

#define LIP_HAL_REL_BASE "http://lip.bullno1.com/hal/relations"

#define LIP_DBG(F) \
	F(LIP_DBG_BREAK) \
	F(LIP_DBG_CONTINUE) \
	F(LIP_DBG_STEP) \
	F(LIP_DBG_STEP_INTO) \
	F(LIP_DBG_STEP_OUT) \
	F(LIP_DBG_STEP_OVER)

LIP_ENUM(lip_dbg_cmd_type_t, LIP_DBG)

struct lip_dbg_cmd_s
{
	lip_dbg_cmd_type_t type;
	union
	{
		lip_stack_frame_t* fp;
	} arg;
};

struct lip_dbg_s
{
	lip_vm_hook_t vtable;
	lip_dbg_config_t cfg;
	bool own_fs;
	struct wby_server server;
	struct lip_dbg_cmd_s cmd;
	lip_context_t* ctx;
	lip_vm_t* vm;
	lip_array(char) char_buf;
	lip_array(char) msg_buf;
	lip_array(uintptr_t) ws_ids;
	uintptr_t next_ws_id;
	void* server_mem;
};

static const struct wby_header lip_cors_headers[] = {
	{ .name = "Access-Control-Allow-Origin", .value = "*" },
	{ .name = "Access-Control-Allow-Methods", .value = "GET, POST, PUT, PATCH, DELETE, OPTIONS" },
	{ .name = "Access-Control-Allow-Headers", .value = "Connection, Content-Type" },
	{ .name = "Access-Control-Max-Age", .value = "600" },
};

static const struct wby_header lip_msgpack_headers[] = {
	{ .name = "Content-Type", .value = "application/hal+msgpack" },
	{ .name = "Cache-Control", .value = "no-cache" },
	{ .name = "Access-Control-Allow-Origin", .value = "*" },
	{ .name = "Access-Control-Allow-Methods", .value = "GET, POST, PUT, PATCH, DELETE, OPTIONS" },
	{ .name = "Access-Control-Allow-Headers", .value = "Connection, Content-Type" },
	{ .name = "Access-Control-Max-Age", .value = "600" },
};

struct lip_dbg_msgpack_s
{
	struct wby_con* conn;
	cmp_ctx_t cmp;
	wby_size bytes_read;
	lip_array(char)* msg_buf;
};

static lip_string_ref_t
lip_string_ref_from_string(lip_string_t* str)
{
	return (lip_string_ref_t){
		.length = str->length,
		.ptr = str->ptr
	};
}

LIP_PRINTF_LIKE(2, 3) static size_t
lip_sprintf(lip_array(char)* buf, const char* fmt, ...)
{
	va_list args;
	struct lip_osstream_s osstream;
	lip_out_t* out = lip_make_osstream(buf, &osstream);
	va_start(args, fmt);
	size_t result = lip_vprintf(out, fmt, args);
	va_end(args);
	return result;
}

static bool
cmp_write_str_ref(cmp_ctx_t* cmp, lip_string_ref_t str)
{
	return cmp_write_str(cmp, str.ptr, str.length);
}

static bool
cmp_write_simple_link(cmp_ctx_t* cmp, const char* rel, const char* href)
{
	return cmp_write_str_ref(cmp, lip_string_ref(rel))
		&& cmp_write_map(cmp, 1)
		&& cmp_write_str_ref(cmp, lip_string_ref("href"))
		&& cmp_write_str_ref(cmp, lip_string_ref(href));
}

static bool
cmp_write_loc(cmp_ctx_t* cmp, lip_loc_t loc)
{
	return cmp_write_map(cmp, 2)
		&& cmp_write_str_ref(cmp, lip_string_ref("line"))
		&& cmp_write_u32(cmp, loc.line)
		&& cmp_write_str_ref(cmp, lip_string_ref("column"))
		&& cmp_write_u32(cmp, loc.column);
}

static bool
cmp_write_loc_range(cmp_ctx_t* cmp, lip_loc_range_t loc)
{
	return cmp_write_map(cmp, 2)
		&& cmp_write_str_ref(cmp, lip_string_ref("start"))
		&& cmp_write_loc(cmp, loc.start)
		&& cmp_write_str_ref(cmp, lip_string_ref("end"))
		&& cmp_write_loc(cmp, loc.end);
}

bool
cmp_write_curies(cmp_ctx_t* cmp)
{
	return cmp_write_str_ref(cmp, lip_string_ref("curies"))
		&& cmp_write_array(cmp, 1)
		&& cmp_write_map(cmp, 3)
		&& cmp_write_str_ref(cmp, lip_string_ref("name"))
		&& cmp_write_str_ref(cmp, lip_string_ref("lip"))
		&& cmp_write_str_ref(cmp, lip_string_ref("href"))
		&& cmp_write_str_ref(cmp, lip_string_ref(LIP_HAL_REL_BASE "/{rel}"))
		&& cmp_write_str_ref(cmp, lip_string_ref("templated"))
		&& cmp_write_bool(cmp, true);
}

static size_t
lip_dbg_msgpack_write(cmp_ctx_t* ctx, const void* data, size_t count)
{
	struct lip_dbg_msgpack_s* msgpack = ctx->buf;
	struct lip_osstream_s osstream;
	return lip_write(data, count, lip_make_osstream(msgpack->msg_buf, &osstream));
}

static bool
lip_dbg_msgpack_read(cmp_ctx_t* ctx, void* data, size_t count)
{
	struct lip_dbg_msgpack_s* msgpack = ctx->buf;
	if(msgpack->bytes_read + count < (size_t)msgpack->conn->request.content_length)
	{
		bool succeeded = wby_read(msgpack->conn, data, count) == 0;
		msgpack->bytes_read += count;
		return succeeded;
	}
	else
	{
		return false;
	}
}

static cmp_ctx_t*
lip_dbg_begin_msgpack(
	struct lip_dbg_msgpack_s* msgpack,
	lip_array(char)* msg_buf,
	struct wby_con* conn
)
{
	*msgpack = (struct lip_dbg_msgpack_s){
		.conn = conn,
		.msg_buf = msg_buf,
	};
	cmp_init(&msgpack->cmp, msgpack, lip_dbg_msgpack_read, lip_dbg_msgpack_write);
	lip_array_clear(*msg_buf);

	return &msgpack->cmp;
}

static void
lip_dbg_end_msgpack(struct lip_dbg_msgpack_s* msgpack)
{
	wby_response_begin(
		msgpack->conn, 200, lip_array_len(*msgpack->msg_buf),
		lip_msgpack_headers, LIP_STATIC_ARRAY_LEN(lip_msgpack_headers)
	);
	wby_write(msgpack->conn, *msgpack->msg_buf, lip_array_len(*msgpack->msg_buf));
	wby_response_end(msgpack->conn);
}

static int
lip_dbg_simple_response(struct wby_con* conn, int status)
{
	wby_response_begin(
		conn, status, 0, lip_cors_headers, LIP_STATIC_ARRAY_LEN(lip_cors_headers)
	);
	wby_response_end(conn);
	return 0;
}

static bool
lip_str_startswith(const char* str, const char* prefix)
{
	return strncmp(str, prefix, strlen(prefix)) == 0;
}

static int
lip_dbg_handle_src(lip_dbg_t* dbg, struct wby_con* conn)
{
	if(strcmp(conn->request.method, "GET") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	lip_fs_t* fs = dbg->cfg.fs;
	lip_in_t* file =
		fs->begin_read(fs, lip_string_ref(conn->request.uri + sizeof("/src/") - 1));
	if(!file) { return lip_dbg_simple_response(conn, 404); }

	char buf[2048];

	const struct wby_header headers[] = {
		{ .name = "Content-Type", .value = "text/plain" },
		{ .name = "Access-Control-Allow-Origin", .value = "*" }
	};
	wby_response_begin(conn, 200, -1, headers, LIP_STATIC_ARRAY_LEN(headers));
	while(true)
	{
		size_t bytes_read = lip_read(buf, sizeof(buf), file);
		if(bytes_read == 0) { break; }

		wby_write(conn, buf, bytes_read);
	}
	wby_response_end(conn);

	fs->end_read(fs, file);
	return 0;
}

static void
lip_write_value_array(cmp_ctx_t* cmp, uint16_t count, const lip_value_t* array)
{
	cmp_write_array(cmp, count);
	for(uint16_t i = 0; i < count; ++i)
	{
		const lip_value_t* value = &array[i];
		switch(value->type)
		{
			case LIP_VAL_NIL:
				cmp_write_nil(cmp);
				break;
			case LIP_VAL_NUMBER:
				cmp_write_double(cmp, value->data.number);
				break;
			case LIP_VAL_BOOLEAN:
				cmp_write_bool(cmp, value->data.boolean);
				break;
			case LIP_VAL_STRING:
				cmp_write_str_ref(cmp, lip_string_ref_from_string(value->data.reference));
				break;
			case LIP_VAL_LIST:
				{
					const lip_list_t* list = value->data.reference;
					lip_write_value_array(cmp, list->length, list->elements);
				}
				break;
			case LIP_VAL_SYMBOL:
				{
					cmp_write_map(cmp, 1);
					cmp_write_str_ref(cmp, lip_string_ref("symbol"));
					cmp_write_str_ref(cmp, lip_string_ref_from_string(value->data.reference));
				}
				break;
			case LIP_VAL_NATIVE:
				{
					cmp_write_map(cmp, 1);
					cmp_write_str_ref(cmp, lip_string_ref("native"));
					cmp_write_uinteger(cmp, (uintptr_t)value->data.reference);
				}
				break;
			case LIP_VAL_FUNCTION:
				{
					cmp_write_map(cmp, 1);
					cmp_write_str_ref(cmp, lip_string_ref("function"));
					cmp_write_uinteger(cmp, (uintptr_t)value->data.reference);
				}
				break;
			case LIP_VAL_PLACEHOLDER:
				{
					cmp_write_map(cmp, 1);
					cmp_write_str_ref(cmp, lip_string_ref("placeholder"));
					cmp_write_uinteger(cmp, value->data.index);
				}
				break;
			default:
				cmp_write_map(cmp, 1);
				cmp_write_str_ref(cmp, lip_string_ref("<corrupted>"));
				cmp_write_nil(cmp);
				break;
		}
	}
}

static void
lip_dbg_write_stack_frame(
	lip_dbg_t* dbg,
	cmp_ctx_t* cmp,
	const lip_stack_frame_t* fp,
	uint16_t index,
	bool summary
)
{
	lip_string_ref_t filename;
	lip_loc_range_t location;

	bool is_native = lip_stack_frame_is_native(fp);

	if(is_native)
	{
		filename = lip_string_ref(
			fp->native_filename ? fp->native_filename : "<native>"
		);
		if(fp->native_line)
		{
			location = (lip_loc_range_t){
				.start = { .line = fp->native_line },
				.end = { .line = fp->native_line }
			};
		}
		else
		{
			location = LIP_LOC_NOWHERE;
		}
	}
	else
	{
		lip_function_layout_t function_layout;
		lip_function_layout(fp->closure->function.lip, &function_layout);
		filename = lip_string_ref_from_string(function_layout.source_name);
		uint16_t location_index =
			LIP_MAX(0, fp->pc - function_layout.instructions) + (index == 0 ? 1 : 0);
		location = function_layout.locations[location_index];
	}

	lip_string_ref_t function_name;
	if(fp->closure == NULL)
	{
		function_name = fp->native_function
			? lip_string_ref(fp->native_function)
			: lip_string_ref("?");
	}
	else if(fp->closure->debug_name)
	{
		function_name = lip_string_ref_from_string(fp->closure->debug_name);
	}
	else if(fp->native_function)
	{
		function_name = lip_string_ref(fp->native_function);
	}
	else
	{
		function_name =  lip_string_ref("?");
	}

	cmp_write_map(cmp, summary ? 5 : 7);
	{
		cmp_write_str_ref(cmp, lip_string_ref("_links"));
		cmp_write_map(cmp, 2);
		{
			cmp_write_str_ref(cmp, lip_string_ref("self"));
			cmp_write_map(cmp, 1);
			{
				cmp_write_str_ref(cmp, lip_string_ref("href"));
				lip_array_clear(dbg->char_buf);
				lip_sprintf(&dbg->char_buf, "/vm/call_stack/%" PRIu16, index);
				cmp_write_str(cmp, dbg->char_buf, lip_array_len(dbg->char_buf));
			}

			cmp_write_str_ref(
				cmp,
				summary
					? lip_string_ref("lip:src")
					: lip_string_ref(LIP_HAL_REL_BASE "/src")
			);
			cmp_write_map(cmp, 1);
			{
				cmp_write_str_ref(cmp, lip_string_ref("href"));
				lip_array_clear(dbg->char_buf);
				lip_sprintf(&dbg->char_buf, "/src/%.*s", (int)filename.length, filename.ptr);
				cmp_write_str(cmp, dbg->char_buf, lip_array_len(dbg->char_buf));
			}
		}

		cmp_write_str_ref(cmp, lip_string_ref("filename"));
		cmp_write_str_ref(cmp, filename);

		cmp_write_str_ref(cmp, lip_string_ref("location"));
		cmp_write_loc_range(cmp, location);

		cmp_write_str_ref(cmp, lip_string_ref("function_name"));
		cmp_write_str_ref(cmp, function_name);

		cmp_write_str_ref(cmp, lip_string_ref("is_native"));
		cmp_write_bool(cmp, is_native);

		if(summary) { return; }

		if(fp->closure == NULL)
		{
			cmp_write_str_ref(cmp, lip_string_ref("function"));
			cmp_write_nil(cmp);

			cmp_write_str_ref(cmp, lip_string_ref("pc"));
			cmp_write_nil(cmp);
		}
		else if(fp->closure->is_native)
		{
			cmp_write_str_ref(cmp, lip_string_ref("function"));
			cmp_write_map(cmp, 1);
			{
				cmp_write_str_ref(cmp, lip_string_ref("env"));
				lip_write_value_array(cmp, fp->closure->env_len, fp->closure->environment);
			}

			cmp_write_str_ref(cmp, lip_string_ref("pc"));
			cmp_write_nil(cmp);
		}
		else
		{
			lip_function_t* fn = fp->closure->function.lip;
			lip_function_layout_t layout;
			lip_function_layout(fn, &layout);

			cmp_write_str_ref(cmp, lip_string_ref("function"));
			cmp_write_map(cmp, 8);
			{
				cmp_write_str_ref(cmp, lip_string_ref("env"));
				lip_write_value_array(cmp, fp->closure->env_len, fp->closure->environment);

				cmp_write_str_ref(cmp, lip_string_ref("constants"));
				lip_write_value_array(cmp, fn->num_constants, layout.constants);

				cmp_write_str_ref(cmp, lip_string_ref("arity"));
				cmp_write_uinteger(cmp, fn->num_args);

				cmp_write_str_ref(cmp, lip_string_ref("vararg"));
				cmp_write_bool(cmp, fn->is_vararg);

				cmp_write_str_ref(cmp, lip_string_ref("stack"));
				lip_write_value_array(cmp, fp->bp - dbg->vm->sp, dbg->vm->sp);

				cmp_write_str_ref(cmp, lip_string_ref("locals"));
				lip_write_value_array(cmp, fn->num_locals, fp->ep);

				cmp_write_str_ref(cmp, lip_string_ref("bytecode"));
				cmp_write_array(cmp, fn->num_instructions);
				for(uint16_t i = 0; i < fn->num_instructions; ++i)
				{
					lip_opcode_t opcode;
					lip_operand_t operand;

					lip_disasm(layout.instructions[i], &opcode, &operand);
					const char* opcode_str = lip_opcode_t_to_str(opcode);

					switch((uint8_t)opcode)
					{
						case LIP_OP_NOP:
						case LIP_OP_NIL:
						case LIP_OP_RET:
							cmp_write_array(cmp, 1);
							cmp_write_str_ref(
								cmp,
								lip_string_ref(opcode_str + sizeof("LIP_OP_") - 1)
							);
							break;
						case LIP_OP_CLS:
							{
								int function_index = operand & 0xFFF;
								int num_captures = (operand >> 12) & 0xFFF;
								cmp_write_array(cmp, 3);
								cmp_write_str_ref(
									cmp,
									lip_string_ref(opcode_str + sizeof("LIP_OP_") - 1)
								);
								cmp_write_integer(cmp, function_index);
								cmp_write_integer(cmp, num_captures);
							}
							break;
						case LIP_OP_LABEL:
							cmp_write_array(cmp, 2);
							cmp_write_str_ref(cmp, lip_string_ref("LBL"));
							cmp_write_integer(cmp, operand);
							break;
						default:
							{
								cmp_write_array(cmp, 2);
								cmp_write_str_ref(
									cmp,
									lip_string_ref(
										opcode_str ? opcode_str + sizeof("LIP_OP_") - 1 : "ILL"
									)
								);
								cmp_write_integer(cmp, operand);
							}
							break;
					}
				}

				cmp_write_str_ref(cmp, lip_string_ref("locations"));
				cmp_write_array(cmp, fn->num_instructions);
				for(uint16_t i = 0; i < fn->num_instructions; ++i)
				{
					cmp_write_loc_range(cmp, layout.locations[i + 1]);
				}
			}

			cmp_write_str_ref(cmp, lip_string_ref("pc"));
			cmp_write_uint(cmp, fp->pc - layout.instructions);
		}
	}
}

static int
lip_dbg_handle_stack_frame(lip_dbg_t* dbg, struct wby_con* conn)
{
	if(strcmp(conn->request.method, "GET") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	const char* uri = conn->request.uri;
	const char* path = uri + sizeof("/vm/call_stack/") - 1;
	char* last_char;
	long frame_index = strtol(path, &last_char, 10);
	if(last_char != uri + strlen(uri))
	{
		return lip_dbg_simple_response(conn, 400);
	}

	lip_vm_t* vm = dbg->vm;
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(&vm->config, &os_block, &env_block, &cs_block);
	lip_stack_frame_t* fp_min = lip_locate_memblock(vm->mem, &cs_block);
	size_t num_frames = vm->fp - fp_min + 1;

	if(frame_index < 0 || frame_index >= (long)num_frames)
	{
		return lip_dbg_simple_response(conn, 404);
	}

	struct lip_dbg_msgpack_s msgpack;
	cmp_ctx_t* cmp = lip_dbg_begin_msgpack(&msgpack, &dbg->msg_buf, conn);

	const lip_stack_frame_t* stack_frame = &fp_min[num_frames - frame_index - 1];
	lip_dbg_write_stack_frame(dbg, cmp, stack_frame, frame_index, false);

	lip_dbg_end_msgpack(&msgpack);

	return 0;
}

static void
lip_dbg_write_call_stack(lip_dbg_t* dbg, lip_vm_t* vm, cmp_ctx_t* cmp)
{
	cmp_write_map(cmp, 2);
	{
		cmp_write_str_ref(cmp, lip_string_ref("_links"));
		cmp_write_map(cmp, 2);
		{
			cmp_write_simple_link(cmp, "self", "/vm/call_stack");
			cmp_write_curies(cmp);
		}

		cmp_write_str_ref(cmp, lip_string_ref("_embedded"));
		cmp_write_map(cmp, 1);
		{
			cmp_write_str_ref(cmp, lip_string_ref("item"));

			lip_memblock_info_t os_block, env_block, cs_block;
			lip_vm_memory_layout(&vm->config, &os_block, &env_block, &cs_block);
			lip_stack_frame_t* fp_min = lip_locate_memblock(vm->mem, &cs_block);
			size_t num_frames = vm->fp - fp_min + 1;

			cmp_write_array(cmp, num_frames);
			unsigned int i = 0;
			for(lip_stack_frame_t* fp = vm->fp; fp >= fp_min; --fp, ++i)
			{
				lip_dbg_write_stack_frame(dbg, cmp, fp, i, true);
			}
		}
	}
}

static int
lip_dbg_handle_call_stack(lip_dbg_t* dbg, struct wby_con* conn)
{
	if(strcmp(conn->request.method, "GET") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	struct lip_dbg_msgpack_s msgpack;
	cmp_ctx_t* cmp = lip_dbg_begin_msgpack(&msgpack, &dbg->msg_buf, conn);

	lip_dbg_write_call_stack(dbg, dbg->vm, cmp);

	lip_dbg_end_msgpack(&msgpack);

	return 0;
}

static void
lip_dbg_write_vm(lip_dbg_t* dbg, lip_vm_t* vm, cmp_ctx_t* cmp)
{
	lip_memblock_info_t os_block, env_block, cs_block;
	lip_vm_memory_layout(&vm->config, &os_block, &env_block, &cs_block);

	cmp_write_map(cmp, 4);
	{
		cmp_write_str_ref(cmp, lip_string_ref("_links"));
		cmp_write_map(cmp, 1);
		{
			cmp_write_simple_link(cmp, "self", "/vm");
		}

		cmp_write_str_ref(cmp, lip_string_ref("status"));
		cmp_write_str_ref(cmp, lip_string_ref(lip_exec_status_t_to_str(vm->status)));

		cmp_write_str_ref(cmp, lip_string_ref("cfg"));
		cmp_write_map(cmp, 3);
		{
			cmp_write_str_ref(cmp, lip_string_ref("os_len"));
			cmp_write_integer(cmp, vm->config.os_len);

			cmp_write_str_ref(cmp, lip_string_ref("cs_len"));
			cmp_write_integer(cmp, vm->config.cs_len);

			cmp_write_str_ref(cmp, lip_string_ref("env_len"));
			cmp_write_integer(cmp, vm->config.env_len);
		}

		cmp_write_str_ref(cmp, lip_string_ref("_embedded"));
		cmp_write_map(cmp, 1);
		{
			cmp_write_str_ref(cmp, lip_string_ref(LIP_HAL_REL_BASE  "/call_stack"));
			lip_dbg_write_call_stack(dbg, vm, cmp);
		}
	}
}

static int
lip_dbg_handle_vm(lip_dbg_t* dbg, struct wby_con* conn)
{
	if(strcmp(conn->request.method, "GET") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	struct lip_dbg_msgpack_s msgpack;
	cmp_ctx_t* cmp = lip_dbg_begin_msgpack(&msgpack, &dbg->msg_buf, conn);

	lip_dbg_write_vm(dbg, dbg->vm, cmp);

	lip_dbg_end_msgpack(&msgpack);

	return 0;
}

static void
lip_dbg_write_dbg(cmp_ctx_t* cmp, lip_dbg_t* dbg, struct wby_con* conn)
{
	cmp_write_map(cmp, 3);
	{
		cmp_write_str_ref(cmp, lip_string_ref("command"));
		cmp_write_str_ref(cmp, lip_string_ref(lip_dbg_cmd_type_t_to_str(dbg->cmd.type)));

		cmp_write_str_ref(cmp, lip_string_ref("_links"));
		cmp_write_map(cmp, 3);
		{
			cmp_write_simple_link(cmp, "self", "/dbg");
			cmp_write_simple_link(cmp, LIP_HAL_REL_BASE "/command", "/command");

			const char* host = wby_find_header(conn, "Host");
			if(host == NULL) { host = "localhost"; }
			lip_array_clear(dbg->char_buf);
			lip_sprintf(
				&dbg->char_buf, "ws://%s:%" PRIu16 "/status", host, dbg->cfg.port
			);
			lip_array_push(dbg->char_buf, '\0');
			cmp_write_simple_link(cmp, LIP_HAL_REL_BASE "/status", dbg->char_buf);
		}

		cmp_write_str_ref(cmp, lip_string_ref("_embedded"));
		cmp_write_map(cmp, 1);
		{
			cmp_write_str_ref(cmp, lip_string_ref(LIP_HAL_REL_BASE "/vm"));
			lip_dbg_write_vm(dbg, dbg->vm, cmp);
		}
	}
}

static int
lip_dbg_handle_dbg(lip_dbg_t* dbg, struct wby_con* conn)
{
	if(strcmp(conn->request.method, "GET") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	struct lip_dbg_msgpack_s msgpack;
	cmp_ctx_t* cmp = lip_dbg_begin_msgpack(&msgpack, &dbg->msg_buf, conn);

	lip_dbg_write_dbg(cmp, dbg, conn);

	lip_dbg_end_msgpack(&msgpack);
	return 0;
}

static int
lip_dbg_handle_command(lip_dbg_t* dbg, struct wby_con* conn)
{
#define LIP_MSGPACK_READ(op, ctx, ...) \
	LIP_ENSURE(cmp_read_##op (ctx, __VA_ARGS__))
#define LIP_ENSURE(cond) \
	do { \
		if(!(cond)) { \
			return lip_dbg_simple_response(conn, 400); \
		} \
	} while(0)

	if(strcmp(conn->request.method, "POST") != 0)
	{
		return lip_dbg_simple_response(conn, 405);
	}

	struct lip_dbg_msgpack_s msgpack;
	cmp_ctx_t* cmp = lip_dbg_begin_msgpack(&msgpack, &dbg->msg_buf, conn);

	cmp_object_t obj;
	LIP_MSGPACK_READ(object, cmp, &obj);
	if(false
		|| obj.type == CMP_TYPE_STR8
		|| obj.type == CMP_TYPE_STR16
		|| obj.type == CMP_TYPE_STR32
		|| obj.type == CMP_TYPE_FIXSTR
	)
	{
		char cmd_buf[32];
		LIP_ENSURE(obj.as.str_size < sizeof(cmd_buf));
		LIP_ENSURE(wby_read(conn, cmd_buf, obj.as.str_size) == 0);
		cmd_buf[obj.as.str_size] = '\0';

		if(strcmp(cmd_buf, "step") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_STEP
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else if(strcmp(cmd_buf, "step-into") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_STEP_INTO
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else if(strcmp(cmd_buf, "step-out") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_STEP_OUT,
				.arg = { .fp = dbg->vm->fp }
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else if(strcmp(cmd_buf, "step-over") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_STEP_OVER,
				.arg = { .fp = dbg->vm->fp }
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else if(strcmp(cmd_buf, "continue") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_CONTINUE
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else if(strcmp(cmd_buf, "break") == 0)
		{
			dbg->cmd = (struct lip_dbg_cmd_s) {
				.type = LIP_DBG_BREAK
			};
			return lip_dbg_simple_response(conn, 202);
		}
		else
		{
			return lip_dbg_simple_response(conn, 400);
		}
	}
	else
	{
		return lip_dbg_simple_response(conn, 400);
	}

	lip_dbg_end_msgpack(&msgpack);
	return 0;
}

#if LIP_DBG_EMBED_RESOURCES == 1
static int
lip_dbg_send_static(
	struct wby_con* conn, size_t size, const void* data, const char* contentType
)
{
	const struct wby_header headers[] = {
		{ .name = "Content-Type", .value = contentType },
		{ .name = "Cache-Control", .value = "no-cache" },
	};
	wby_response_begin(conn, 200, (int)size, headers, LIP_STATIC_ARRAY_LEN(headers));
	wby_write(conn, data, size);
	wby_response_end(conn);
	return 0;
}
#endif

static int
lip_dbg_dispatch(struct wby_con* conn, void* userdata)
{
	if(strcmp(conn->request.method, "OPTIONS") == 0)
	{
		return lip_dbg_simple_response(conn, 200);
	}

	lip_dbg_t* dbg = userdata;
	const char* uri = conn->request.uri;

	if(strcmp(uri, "/dbg") == 0)
	{
		return lip_dbg_handle_dbg(dbg, conn);
	}
	else if(strcmp(uri, "/vm") == 0)
	{
		return lip_dbg_handle_vm(dbg, conn);
	}
	else if(strcmp(uri, "/vm/call_stack") == 0)
	{
		return lip_dbg_handle_call_stack(dbg, conn);
	}
	else if(lip_str_startswith(uri, "/vm/call_stack/"))
	{
		return lip_dbg_handle_stack_frame(dbg, conn);
	}
	else if(lip_str_startswith(uri, "/src/"))
	{
		return lip_dbg_handle_src(dbg, conn);
	}
	else if(strcmp(uri, "/command") == 0)
	{
		return lip_dbg_handle_command(dbg, conn);
	}
#if LIP_DBG_EMBED_RESOURCES == 1
	else if(strcmp(uri, "/") == 0)
	{
		return lip_dbg_send_static(
			conn, index_html_size, index_html_data, "text/html; charset=UTF-8"
		);
	}
	else if(strcmp(uri, "/bundle.js") == 0)
	{
		return lip_dbg_send_static(
			conn, bundle_js_size, bundle_js_data, "application/javascript; charset=UTF-8"
		);
	}
	else if(strcmp(uri, "/bundle.js.map") == 0)
	{
		return lip_dbg_send_static(
			conn, bundle_js_map_size, bundle_js_map_data, "application/json"
		);
	}
#endif
	else
	{
		return 1;
	}
}

static int
lip_dbg_ws_connect(struct wby_con* conn, void* userdata)
{
	(void)userdata;
	return strcmp(conn->request.uri, "/status");
}

static void
lip_dbg_ws_connected(struct wby_con* conn, void* userdata)
{
	lip_dbg_t* dbg = userdata;
	uintptr_t id = dbg->next_ws_id++;
	conn->user_data = (void*)id;
	lip_array_push(dbg->ws_ids, id);
}

static void
lip_dbg_ws_closed(struct wby_con* conn, void* userdata)
{
	lip_dbg_t* dbg = userdata;
	lip_array_foreach(uintptr_t, itr, dbg->ws_ids)
	{
		if(*itr == (uintptr_t)conn->user_data)
		{
			lip_array_quick_remove(dbg->ws_ids, itr - dbg->ws_ids);
			break;
		}
	}
	conn->user_data = NULL;
}

static int
lip_dbg_ws_frame(
	struct wby_con* connection, const struct wby_frame* frame, void* userdata
)
{
	(void)connection;
	(void)frame;
	(void)userdata;
	return 0;
}

static void
lip_dbg_step(
	lip_vm_hook_t* vtable, const lip_vm_t* vm
)
{
	(void)vm;
	lip_dbg_t* dbg = LIP_CONTAINER_OF(vtable, lip_dbg_t, vtable);

	bool is_native = lip_stack_frame_is_native(vm->fp);
	bool has_loc;
	if(is_native)
	{
		has_loc = false;
	}
	else
	{
		lip_function_t* fn = vm->fp->closure->function.lip;
		lip_function_layout_t layout;
		lip_function_layout(fn, &layout);

		lip_loc_range_t location = layout.locations[vm->fp->pc - layout.instructions + 1];
		has_loc = memcmp(&location, &LIP_LOC_NOWHERE, sizeof(location)) != 0;
	}

	bool can_break;
	switch(dbg->cmd.type)
	{
		case LIP_DBG_BREAK:
			can_break = true;
			break;
		case LIP_DBG_CONTINUE:
			can_break = false;
			break;
		case LIP_DBG_STEP:
			can_break = true;
			break;
		case LIP_DBG_STEP_INTO:
			can_break = has_loc;
			break;
		case LIP_DBG_STEP_OUT:
			can_break = has_loc && vm->fp < dbg->cmd.arg.fp;
			break;
		case LIP_DBG_STEP_OVER:
			can_break = has_loc && vm->fp <= dbg->cmd.arg.fp;
			break;
	}

	if(can_break) { dbg->cmd.type = LIP_DBG_BREAK; }

	wby_update(&dbg->server, 0);

	// Broadcast break status over WebSocket
	if(dbg->cmd.type == LIP_DBG_BREAK)
	{
		lip_array_foreach(uintptr_t, id, dbg->ws_ids)
		{
			struct wby_con* conn = wby_find_conn(&dbg->server, (void*)*id);
			if(conn == NULL) { continue; }

			wby_frame_begin(conn, WBY_WSOP_BINARY_FRAME);

			lip_array_clear(dbg->msg_buf);
			struct lip_dbg_msgpack_s msgpack ={
				.conn = conn,
				.msg_buf = &dbg->msg_buf,
			};
			cmp_init(&msgpack.cmp, &msgpack, lip_dbg_msgpack_read, lip_dbg_msgpack_write);
			lip_dbg_write_dbg(&msgpack.cmp, dbg, conn);
			wby_write(conn, dbg->msg_buf, lip_array_len(dbg->msg_buf));

			wby_frame_end(conn);
		}
	}

	while(dbg->cmd.type == LIP_DBG_BREAK)
	{
		wby_update(&dbg->server, 1);
	}
}

void
lip_reset_dbg_config(lip_dbg_config_t* cfg)
{
	*cfg = (lip_dbg_config_t){
		.allocator = lip_default_allocator,
		.port = 8081
	};
}

lip_dbg_t*
lip_create_debugger(lip_dbg_config_t* cfg)
{
	lip_dbg_t* dbg = lip_new(cfg->allocator, lip_dbg_t);
	*dbg = (lip_dbg_t){
		.vtable = { .step = lip_dbg_step },
		.cfg = *cfg,
		.own_fs = cfg->fs == NULL,
		.cmd = { .type = LIP_DBG_BREAK },
		.char_buf = lip_array_create(cfg->allocator, char, 64),
		.msg_buf = lip_array_create(cfg->allocator, char, 1024),
		.ws_ids = lip_array_create(cfg->allocator, uintptr_t, 1),
		.next_ws_id = 1
	};

	if(dbg->own_fs)
	{
		dbg->cfg.fs = lip_create_native_fs(cfg->allocator);
	}

	struct wby_config wby_config = {
		.address = "127.0.0.1",
		.port = cfg->port,
		.connection_max = 4,
		.request_buffer_size = 4096,
		.io_buffer_size = 4096,
		.dispatch = lip_dbg_dispatch,
		.ws_connect = lip_dbg_ws_connect,
		.ws_connected = lip_dbg_ws_connected,
		.ws_closed = lip_dbg_ws_closed,
		.ws_frame = lip_dbg_ws_frame,
		.userdata = dbg
	};
	size_t needed_memory;
	wby_init(&dbg->server, &wby_config, &needed_memory);
	dbg->server_mem = lip_malloc(cfg->allocator, needed_memory);
	wby_start(&dbg->server, dbg->server_mem);

	return dbg;
}

void
lip_destroy_debugger(lip_dbg_t* dbg)
{
	wby_stop(&dbg->server);
	if(dbg->own_fs)
	{
		lip_destroy_native_fs(dbg->cfg.fs);
	}

	lip_array_destroy(dbg->ws_ids);
	lip_array_destroy(dbg->msg_buf);
	lip_array_destroy(dbg->char_buf);
	lip_free(dbg->cfg.allocator, dbg->server_mem);
	lip_free(dbg->cfg.allocator, dbg);
}

void
lip_attach_debugger(lip_dbg_t* dbg, lip_vm_t* vm)
{
	lip_set_vm_hook(vm, &dbg->vtable);
	dbg->vm = vm;
}
