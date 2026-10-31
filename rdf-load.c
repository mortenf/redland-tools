/*
 * rdf-load.c - Load statements from URI and store in persistent
 * Redland storage.
 *
 * Copyright (C) 2003-2014 Morten Frederiksen - http://purl.org/net/morten/
 *
 * and
 *
 * Copyright (C) 2000-2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
 * University of Bristol - http://www.bristol.ac.uk/
 *
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 *
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <redland.h>
#include <unistd.h>
#include <time.h>

const char *VERSION="0.10";

struct options
{
	char *bulk;
	librdf_uri *base;
	long int cache;
	librdf_node *context;
	char *database;
	char *directory;
	char *host;
	char *input;
	char *model;
	int keep;
	char *is_new;
	int port;
	char *password;
	int quiet;
	char *user;
} opts;

int main(int argc,char *argv[]);
void stamp(char *argv0,char *t0);
int getoptions(int argc,char *argv[],librdf_world *world);
int usage(char *argv0,int version);

int main(int argc,char *argv[])
{
	/* Redland objects. */
	librdf_world *world;
	librdf_storage *storage;
	librdf_model *model;
	librdf_parser *parser;
	librdf_uri *uri;
	librdf_stream *stream;
	int argnum;
	char *storage_type;
	char *storage_options;
	char *t0;

	t0=malloc(strlen(argv[0])+31);
	stamp(argv[0],t0);

	/* Create rdflib world. */
	world=librdf_new_world();
	if(!world)
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create Redland world\n",t0);
		return(1);
	};
	librdf_world_open(world);

	/* Parse command line options (if possible). */
	argnum=getoptions(argc,argv,world);

	/* Check for URI argument. */
	if (argnum>=argc)
	{
		stamp(argv[0],t0);
		fprintf(stderr,"%s: Missing URI argument\n",t0);
		usage(t0,0);
	};

	/* Get URI. */
	uri=librdf_new_uri(world,(const unsigned char *)argv[argnum]);
	if(!uri)
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create input uri\n",t0);
		exit(1);
	};

	/* Set defaults. */
	if (!opts.base)
	{
		opts.base=librdf_new_uri_from_uri(uri);
		if(!opts.base)
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create base uri\n",t0);
			exit(1);
		};
	};
	if (!opts.context)
	{
		opts.context=librdf_new_node_from_uri(world,opts.base);
		if(!opts.context)
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create context node\n",t0);
			exit(1);
		};
	};

	/* Set storage options. */
	if (opts.database && !strcmp(opts.database,"memory"))
	{
		storage_type=strdup("memory");
		storage_options=0;
		if (!storage_type)
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create 'memory' storage options\n",t0);
			return(1);
		};
	}
	else
	if (opts.database)
	{
		storage_type=strdup("mysql");
		storage_options=(char*)malloc(strlen(opts.host)+strlen(opts.database)+strlen(opts.user)+strlen(opts.password)+120);
		if (!storage_type || !storage_options)
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create 'mysql' storage options\n",t0);
			return(1);
		};
		sprintf(storage_options,"merge='yes',bulk='%s',nodecache='%li',new='%s',host='%s',database='%s',port='%i',user='%s',password='%s',write='yes'",
			opts.bulk,opts.cache,opts.is_new,opts.host,opts.database,opts.port,opts.user,opts.password);
	}
	else
	{
		storage_type=strdup("hashes");
		storage_options=(char*)malloc(strlen(opts.directory)+120);
		if (!storage_type || !storage_options)
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create 'hashes' storage options\n",t0);
			return(1);
		};
		sprintf(storage_options,"new='%s',hash-type='bdb',dir='%s',contexts='yes',write='yes'",
			opts.is_new,opts.directory);
	};

	/* Create storage. */
	storage=librdf_new_storage(world,storage_type,opts.model,storage_options);
	if(!storage)
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create new storage (%s/%s/%s)\n",t0,storage_type,opts.model,storage_options);
		return(1);
	}
	free(storage_type);
	if(storage_options)
		free(storage_options);

	/* Create model. */
	model=librdf_new_model(world,storage,NULL);
	if(!model)
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create model\n",t0);
		return(1);
	}
	if (librdf_model_size(model)!=-1 && !opts.quiet) {
		stamp(argv[0],t0);
		fprintf(stdout, "%s: * Model '%s' initially contained %d statements\n",t0,opts.model,librdf_model_size(model));
		fflush(stdout);
	}

	/* Create parser. */
	parser=librdf_new_parser(world,opts.input,NULL,NULL);
	if(!parser)
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create parser\n",t0);
		return(1);
	}

	/* Parse... */
	if (!(stream=librdf_parser_parse_as_stream(parser,uri,opts.base)))
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to parse uri (%s)\n",t0,librdf_uri_as_string(uri));
		return(1);
	};

	/* Remove existing statements with context? */
	if (!opts.keep && librdf_model_size(model))
	{
		if (librdf_model_context_remove_statements(model,opts.context))
		{
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to remove existing statements from model\n",t0);
			return(1);
		};
		librdf_model_sync(model);
		if (librdf_model_size(model)!=-1 && !opts.quiet) {
			stamp(argv[0],t0);
			fprintf(stdout, "%s: * After context truncation: %d statements\n",t0,librdf_model_size(model));
			fflush(stdout);
		}
	};

	/* Add statements with context. */
	if (librdf_model_context_add_statements(model,opts.context,stream))
	{
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to add statements to model\n",t0);
		return(1);
	};
	if (librdf_model_size(model)!=-1 && !opts.quiet) {
		stamp(argv[0],t0);
		fprintf(stdout, "%s: * After insertion: %d statements\n",t0,librdf_model_size(model));
		fflush(stdout);
	}

	/* Sleep to make sure that generated bnode ID's don't clash... */
	/*sleep(1);*/

	/* Clean up. */
	librdf_free_stream(stream);
	librdf_storage_sync(storage);
	librdf_free_parser(parser);
	librdf_free_model(model);
	librdf_free_storage(storage);
	librdf_free_world(world);

	if (!opts.quiet) {
		stamp(argv[0],t0);
		fprintf(stdout, "%s: * Done.\n",t0);
		fflush(stdout);
	}
	free(t0);

	/* keep gcc -Wall happy */
	return(0);
}

void stamp(char *argv0,char *t0)
{
	time_t now=time(NULL);
	strcpy(t0,argv0);
	strcat(t0," [");
	strftime(&t0[strlen(t0)],strlen(argv0)+31-strlen(t0),"%FT%TZ",gmtime(&now));
	strcat(t0,"]");
}

int getoptions(int argc,char *argv[],librdf_world *world)
{
	/* Define command line options. */
	struct option opts_long[]={
		{"help",no_argument,NULL,'?'},
		{"bulk",no_argument,NULL,'B'},
		{"base",required_argument,NULL,'b'},
		{"cache",required_argument,NULL,'C'},
		{"context",required_argument,NULL,'c'},
		{"database",required_argument,NULL,'D'},
		{"directory",required_argument,NULL,'d'},
		{"host",required_argument,NULL,'h'},
		{"input",required_argument,NULL,'i'},
		{"keep",no_argument,NULL,'k'},
		{"model",required_argument,NULL,'m'},
		{"new",no_argument,NULL,'n'},
		{"port",required_argument,NULL,'P'},
		{"password",required_argument,NULL,'p'},
		{"quiet",no_argument,NULL,'q'},
		{"user",required_argument,NULL,'u'},
		{"version",no_argument,NULL,'v'},
		{0,0,0,0}};
	const char *opts_short="?Bb:C:c:D:d:h:i:m:knP:p:qu:v";
	int i=1;
	char c;
	char *buffer;
	int ttypasswd=1;

	/* Set defaults. */
	opts.base=0;
	opts.cache=0;
	opts.context=0;
	opts.password=0;
	opts.port=3306;
	opts.quiet=0;
	opts.user=0;
	opts.keep=0;
	if (!(opts.directory=strdup("./"))
			|| !(opts.bulk=strdup("no"))
			|| !(opts.host=strdup("mysql"))
			|| !(opts.model=strdup("redland"))
			|| !(opts.is_new=strdup("no"))
			|| !(opts.input=strdup("raptor"))
			|| !(opts.database=strdup("redland")))
	{
		fprintf(stderr,"%s: Failed to allocate default options\n",argv[0]);
		exit(1);
	};

	while ((c=getopt_long(argc,argv,opts_short,opts_long,&i))!=-1)
	{
		if (optarg)
		{
			buffer=(char*)malloc(strlen(optarg)+1);
			if (!buffer)
			{
				fprintf(stderr,"%s: Failed to allocate buffer for command line argument (%s)\n",argv[0],optarg);
				exit(1);
			};
			strncpy(buffer,optarg,strlen(optarg)+1);
		};
		switch (c)
		{
			case '?':
				usage(argv[0],0);
			case 'B':
				free(opts.bulk);
				opts.bulk=(char*)malloc(4);
				if (!opts.bulk)
				{
					fprintf(stderr,"%s: Failed to allocate buffer for bulk parameter\n",argv[0]);
					exit(1);
				};
				strncpy(opts.bulk,"yes",4);
				break;
			case 'b':
				free(buffer);
				opts.base=librdf_new_uri(world,(const unsigned char *)optarg);
				if (!opts.base)
				{
					fprintf(stderr,"%s: Failed to create base URI (%s)\n",argv[0],optarg);
					exit(1);
				};
				break;
			case 'C':
				free(buffer);
				opts.cache=atol(optarg);
				break;
			case 'c':
				free(buffer);
				opts.context=librdf_new_node_from_uri_string(world,(const unsigned char *)optarg);
				if (!opts.context)
				{
					fprintf(stderr,"%s: Failed to create context node (%s)\n",argv[0],optarg);
					exit(1);
				};
				break;
			case 'D':
				free(opts.database);
				free(opts.directory);
				opts.directory=0;
				opts.database=buffer;
				break;
			case 'd':
				free(opts.database);
				free(opts.directory);
				opts.database=0;
				opts.directory=buffer;
				break;
			case 'h':
				free(opts.host);
				opts.host=buffer;
				break;
			case 'i':
				free(opts.input);
				opts.input=buffer;
				break;
			case 'k':
				opts.keep=1;
				break;
			case 'm':
				free(opts.model);
				opts.model=buffer;
				break;
			case 'n':
				free(opts.is_new);
				opts.is_new=(char*)malloc(4);
				if (!opts.is_new)
				{
					fprintf(stderr,"%s: Failed to allocate buffer for new parameter\n",argv[0]);
					exit(1);
				};
				strncpy(opts.is_new,"yes",4);
				break;
			case 'P':
				free(buffer);
				opts.port=atoi(optarg);
				break;
			case 'p':
				opts.password=buffer;
				ttypasswd=0;
				break;
			case 'q':
				opts.quiet=1;
				break;
			case 'u':
				opts.user=buffer;
				break;
			case 'v':
				usage(argv[0],1);
			default:
				fprintf(stderr,"%s: Invalid option (%c)\n",argv[0],c);
				usage(argv[0],0);
		}
	}

	/* Flag missing user name. */
	if (opts.database && !opts.user)
	{
		fprintf(stderr,"%s: Missing user name for mysql storage\n",argv[0]);
		usage(argv[0],0);
		exit(1);
	};

	/* Read password from tty if not specified. */
	if (opts.database && ttypasswd)
	{
		char c2;
		int i2=0;
		opts.password=malloc(128);
		if (!opts.password)
		{
			fprintf(stderr,"%s: Failed to allocate buffer for password\n",argv[0]);
			exit(1);
		};
		fprintf(stderr,"%s: Enter password for %s@%s/%s: ",argv[0],opts.user,opts.host,opts.database);
		while ((c2=getchar())!='\n')
		{
			opts.password[i2++]=c2;
			if (i2==127)
				break;
		};
		opts.password[i2++]=0;
	};

	return optind;
}

int usage(char *argv0,int version)
{
	printf("\n\
%s  Version %s\n\
Load statements from URI and store in persistent Redland storage.\n\
* Copyright (C) 2003-2004 Morten Frederiksen - http://purl.org/net/morten/\n\
* Copyright (C) 2000-2003 David Beckett - http://purl.org/net/dajobe/\n\
",argv0,VERSION);
	if (version)
		exit(0);
	printf("\n\
usage: %s [options] <URI>\n\
\n\
  -?, --help         Display this help message and exit.\n\
  -b<uri>, --base=<uri>\n\
                     Base URI for parsing input statements.\n\
  -B, --bulk         Optimize for bulk loads by locking tables and disabling\n\
                     indices while loading. Note that this excludes all other\n\
                     access to the storage.\n\
  -C<list length>, --cache=<list length>\n\
                     Length of node cache list, to reduce extranous inserts.\n\
                     Default node cache list length is 64.\n\
  -c<uri>, --context=<uri>\n\
                     URI to store with triples as context, default is base URI\n\
                     if present, otherwise source URI.\n\
  -D<database>, --database=<database>\n\
                     Name of MySQL database to use, default is 'redland'.\n\
  -d<directory>, --directory=<directory>\n\
                     Directory to use for BDB files. When provided implies use\n\
                     of 'hashes' storage type instead of 'mysql'.\n\
  -h<host name>, --host=<host name>\n\
                     Host to contact for MySQL connections, default is 'mysql'.\n\
  -i<parser name>, --input=<parser name>\n\
                     Name of parser to use ('raptor' for RDF/XML, or 'ntriples'\n\
                     for NTriples), default is 'raptor'.\n\
  -k, --keep         By default, existing statements in the given context will\n\
                     be removed prior to insertion of new statements, this\n\
                     option will skip the removal.\n\
  -m<model id>, --model=<model id>\n\
                     Identifier for (name of) storage (model name for storage\n\
                     type 'mysql', base file name for storage type 'hashes'),\n\
                     default is 'redland'.\n\
  -n, --new          Truncate model before adding new statements.\n\
  -p<password>, --password=<password>\n\
                     Password to use when connecting to MySQL server.\n\
                     If password is not given it's asked from the tty.\n\
  -P<port number>, --port=<port number>\n\
                     The port number to use when connecting to MySQL server.\n\
                     Default port number is 3306.\n\
  -u<user name>, --user=<user name>\n\
                     User name for MySQL server.\n\
  -v, --version      Output version information and exit.\n\
\n\
",argv0);
	exit(1);
}
