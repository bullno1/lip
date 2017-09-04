#ifndef LIP_CLI_H
#define LIP_CLI_H

static inline unsigned int
get_opt_width(const struct optparse_long opt, const char* param)
{
	return 0
		+ 2 // "-o"
		+ (opt.longname ? 3 + strlen(opt.longname) : 0) // ",--option",
		+ (param ? 3 + strlen(param) : 0); //" [param]"
}

static inline void
show_options(const struct optparse_long* opts, const char* help[])
{
	unsigned int max_width = 0;
	for(unsigned int i = 0;; ++i)
	{
		struct optparse_long opt = opts[i];
		if(!opt.shortname) { break; }

		unsigned int opt_width = get_opt_width(opt, help[i * 2]);
		max_width = LIP_MAX(opt_width, max_width);
	}

	for(unsigned int i = 0;; ++i)
	{
		struct optparse_long opt = opts[i];
		if(!opt.shortname) { break; }

		fprintf(stderr, "  -%c", opt.shortname);
		if(opt.longname)
		{
			fprintf(stderr, ",--%s", opt.longname);
		}

		const char* param = help[i * 2];
		const char* description = help[i * 2 + 1];

		switch(opt.argtype)
		{
			case OPTPARSE_NONE:
				break;
			case OPTPARSE_OPTIONAL:
				fprintf(stderr, "[=%s]", param);
				break;
			case OPTPARSE_REQUIRED:
				fprintf(stderr, " <%s>", param);
				break;
		}
		fprintf(stderr, "%*s", max_width + 4 - get_opt_width(opt, param), "");
		fprintf(stderr, "%s\n", description);
	}
}

#endif
