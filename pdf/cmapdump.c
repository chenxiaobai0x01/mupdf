/* cmapdump.c -- parse a CMap file and dump it as a c-struct */

#include <stdio.h>
#include <string.h>

#include "fitz.h"
#include "mupdf.h"

#include "../fitz/base_error.c"
#include "../fitz/base_memory.c"
#include "../fitz/base_string.c"
#include "../fitz/stm_buffer.c"
#include "../fitz/stm_open.c"
#include "../fitz/stm_read.c"

#include "../pdf/pdf_lex.c"
#include "../pdf/pdf_cmap.c"
#include "../pdf/pdf_cmap_parse.c"

static void
clean(char *p)
{
	while (*p)
	{
		if ((*p == '/') || (*p == '.') || (*p == '\\') || (*p == '-'))
			*p = '_';
		p ++;
	}
}

int
main(int argc, char **argv)
{
	pdf_cmap *cmap;
	fz_error error;
	fz_stream *fi;
	FILE *fo;
	char name[256];
	char *realname;
	int i, k;

	if (argc < 3)
	{
		fprintf(stderr, "usage: cmapdump output.c lots of cmap files\n");
		return 1;
	}

	fo = fopen(argv[1], "wb");
	if (!fo)
	{
		fprintf(stderr, "cmapdump: could not open output file '%s'\n", argv[1]);
		return 1;
	}

	fprintf(fo, "#include \"fitz.h\"\n");
	fprintf(fo, "#include \"mupdf.h\"\n");
	fprintf(fo, "\n");

	for (i = 2; i < argc; i++)
	{
		realname = strrchr(argv[i], '/');
		if (!realname)
			realname = strrchr(argv[i], '\\');
		if (realname)
			realname ++;
		else
			realname = argv[i];

		if (strlen(realname) > (sizeof name - 1))
		{
			fprintf(stderr, "cmapdump: file name too long\n");
			return 1;
		}

		strcpy(name, realname);
		clean(name);

		fi = fz_openfile(argv[i]);
		if (!fi)
			fz_throw("cmapdump: could not open input file '%s'\n", argv[i]);

		error = pdf_parsecmap(&cmap, fi);
		if (error)
		{
			fz_catch(error, "cmapdump: could not parse input cmap '%s'\n", argv[i]);
			return 1;
		}

		fprintf(fo, "/* %s */\n\n", cmap->cmapname);

		fprintf(fo, "static const pdf_range pdf_cmap_%s_ranges[] =\n{\n", name);
		if (cmap->rlen == 0)
		{
			fprintf(fo, "\t/* dummy entry for non-c99 compilers */\n");
			fprintf(fo, "\t{ 0x0, %d, 0 }\n", PDF_CMAP_RANGE);
		}
		for (k = 0; k < cmap->rlen; k++)
		{
			fprintf(fo, "\t{ 0x%04x, 0x%04x, %d },\n",
				cmap->ranges[k].low, cmap->ranges[k].extentflags, cmap->ranges[k].offset);
		}
		fprintf(fo, "};\n\n");

		if (cmap->tlen == 0)
		{
			fprintf(fo, "static const unsigned short pdf_cmap_%s_table[1] = { 0 };\n\n", name);
		}
		else
		{
			fprintf(fo, "static const unsigned short pdf_cmap_%s_table[%d] =\n{",
				name, cmap->tlen);
			for (k = 0; k < cmap->tlen; k++)
			{
				if (k % 8 == 0)
					fprintf(fo, "\n\t");
				fprintf(fo, "%d,", cmap->table[k]);
			}
			fprintf(fo, "\n};\n\n");
		}

		fprintf(fo, "pdf_cmap pdf_cmap_%s =\n", name);
		fprintf(fo, "{\n");
		fprintf(fo, "\t-1, ");
		fprintf(fo, "\"%s\", ", cmap->cmapname);
		fprintf(fo, "\"%s\", nil, ", cmap->usecmapname);
		fprintf(fo, "%d,\n", cmap->wmode);

		fprintf(fo, "\t%d, /* codespace table */\n", cmap->ncspace);
		fprintf(fo, "\t{\n");

		if (cmap->ncspace == 0)
		{
			fprintf(fo, "\t/* dummy entry for non-c99 compilers */\n");
			fprintf(fo, "\t{ 0, 0x0, 0x0 },\n");
		}
		for (k = 0; k < cmap->ncspace; k++)
		{
			fprintf(fo, "\t\t{ %d, 0x%04x, 0x%04x },\n",
				cmap->cspace[k].n, cmap->cspace[k].low, cmap->cspace[k].high);
		}
		fprintf(fo, "\t},\n");

		fprintf(fo, "\t%d, %d, (pdf_range*) pdf_cmap_%s_ranges,\n",
			cmap->rlen, cmap->rlen, name);

		fprintf(fo, "\t%d, %d, (unsigned short*) pdf_cmap_%s_table,\n",
			cmap->tlen, cmap->tlen, name);

		fprintf(fo, "};\n\n");

		fz_close(fi);
	}

	if (fclose(fo))
	{
		fprintf(stderr, "cmapdump: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}