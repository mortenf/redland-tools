/*
 * rdf-smush.c - Smush a persistent Redland storage by owl:IFP's and owl:FP's.
 *
 * Based in part on the work by mattb and libby.
 *
 * See: http://www.wasab.dk/morten/blog/archives/2005/03/20/redland-smushing
 *
 * Copyright (C) 2003-2026 Morten Frederiksen - http://purl.org/net/morten/
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
#include <time.h>

const char *VERSION="0.24";

struct options
{
	char *bulk;
	librdf_node *context;
	char *database;
	char *directory;
	librdf_node *fp;
	char *host;
	librdf_node *ifp;
	int list;
	char *model;
	int port;
	char *password;
	int quiet;
	int test;
	int safe;
	char *user;
} opts;

int main(int argc,char *argv[]);
void stamp(char *argv0,char *t0);
int smush(char *argv0,char *t0,librdf_world *world,librdf_model *model,librdf_node *ifp,librdf_node *fp);
int rewrite(librdf_world *world, librdf_model *model, librdf_node *canonical, librdf_node *node, librdf_statement *search);

int getoptions(int argc,char *argv[],librdf_world *world);
int usage(char *argv0,int version);

int main(int argc,char *argv[])
{
	/* Redland objects. */
	librdf_world *world;
	librdf_storage *storage;
	librdf_model *model;
	librdf_storage *tempstorage;
	librdf_model *tempmodel;
	char *storage_type;
	char *storage_options;
	char *t0;

	t0=malloc(strlen(argv[0])+31);
	stamp(argv[0],t0);

	/* Create rdflib world. */
	if (!(world=librdf_new_world())) {
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create Redland world\n",t0);
		return(1);
	}
	librdf_world_open(world);

	/* Parse command line options (if possible) and check for context. */
	getoptions(argc,argv,world);

	/* Set storage options. */
	if (opts.database) {
		storage_type=strdup("mysql");
		storage_options=malloc(strlen(opts.host)+strlen(opts.database)+strlen(opts.user)+strlen(opts.password)+120);
		if (!storage_type || !storage_options) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create 'mysql' storage options\n",t0);
			return(1);
		}
		sprintf(storage_options,"merge='yes',bulk='%s',host='%s',database='%s',port='%i',user='%s',password='%s',contexts='yes',write='yes'",
				opts.bulk,opts.host,opts.database,opts.port,opts.user,opts.password);
	} else {
		storage_type=strdup("hashes");
		storage_options=malloc(strlen(opts.directory)+120);
		if (!storage_type || !storage_options) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create 'hashes' storage options\n",t0);
			return(1);
		}
		sprintf(storage_options,"hash-type='bdb',dir='%s',contexts='yes',write='yes'",
				opts.directory);
	}

	/* Create storage. */
	if (!(storage=librdf_new_storage(world,storage_type,opts.model,storage_options))) {
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create storage (%s/%s/%s)\n",t0,storage_type,opts.model,storage_options);
		return(1);
	}

	/* Create model. */
	if (!(model=librdf_new_model(world,storage,NULL))) {
		stamp(argv[0],t0);
		fprintf(stderr, "%s: Failed to create model\n",t0);
		return(1);
	}

	if (librdf_model_size(model)!=-1 && !opts.quiet) {
		stamp(argv[0],t0);
		fprintf(stdout, "%s: * Model '%s' contains %d statements.\n",t0,opts.model,librdf_model_size(model));
		fflush(stdout);
	}

	/* Smush - a single context or all, internal IFP's and FP's or given? */
	if (opts.ifp) {
		if (smush(argv[0],t0,world,model,opts.ifp,NULL)) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to smush.\n",t0);
			return(1);
		}
	} else if (opts.fp) {
		if (smush(argv[0],t0,world,model,NULL,opts.fp)) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to smush.\n",t0);
			return(1);
		}
	} else {
		librdf_node *rdftype=librdf_new_node_from_uri_string(world,(const unsigned char*)"http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
		librdf_node *owlifp=librdf_new_node_from_uri_string(world,(const unsigned char*)"http://www.w3.org/2002/07/owl#InverseFunctionalProperty");
		librdf_stream *ifpstream;
		librdf_statement *ifpstatement;
		librdf_node *owlfp=librdf_new_node_from_uri_string(world,(const unsigned char*)"http://www.w3.org/2002/07/owl#FunctionalProperty");
		librdf_stream *fpstream;
		librdf_statement *fpstatement;
		librdf_stream *pstream;
		/* Create in-memory storage/model to hold properties. */
		if (!(tempstorage=librdf_new_storage(world,"memory","props",""))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create temporary storage\n",t0);
			return(1);
		}
		if (!(tempmodel=librdf_new_model(world,tempstorage,NULL))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to create temporary model\n",t0);
			return(1);
		}

		/* Find all owl:IFP's. */
		if (!(ifpstatement=librdf_new_statement_from_nodes(world,NULL,
				librdf_new_node_from_node(rdftype),
				librdf_new_node_from_node(owlifp)))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Unable to query for IFP's in model '%s'\n",t0,opts.model);
			return(1);
		}
		if (!(ifpstream=librdf_model_find_statements(model,ifpstatement))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to find any IFP's in model '%s'\n",t0,opts.model);
			return(1);
		}
		while (!librdf_stream_end(ifpstream)) {
			librdf_node *ifp=librdf_statement_get_subject(librdf_stream_get_object(ifpstream));
			librdf_node *context=librdf_stream_get_context(ifpstream);
			librdf_statement *st;
			if (!opts.safe || 
					(librdf_node_is_resource(ifp)
					&& librdf_node_is_resource(context)
					&& (char*)librdf_uri_as_string(librdf_node_get_uri(ifp))==
						strstr((char*)librdf_uri_as_string(librdf_node_get_uri(ifp)),
							(char*)librdf_uri_as_string(librdf_node_get_uri(context))))) {
				/* Add relation to temporary model. */
				if (!(st=librdf_new_statement_from_nodes(world,
							librdf_new_node_from_node(ifp),
							librdf_new_node_from_node(rdftype),
							librdf_new_node_from_node(owlifp)))
						|| librdf_model_add_statement(tempmodel,st)) {
					stamp(argv[0],t0);
					fprintf(stderr, "%s: Unable to add IFP to temporary model.\n",t0);
					return(1);
				}
			} else if (opts.safe && opts.list) {
				stamp(argv[0],t0);
				fprintf(stderr, "%s: * Skipping [%s] from [%s]\n",t0,librdf_uri_as_string(librdf_node_get_uri(ifp)),librdf_uri_as_string(librdf_node_get_uri(context)));
			}
			librdf_stream_next(ifpstream);
		}
		librdf_free_stream(ifpstream);
		librdf_free_statement(ifpstatement);

		/* Find all owl:FP's. */
		if (!(fpstatement=librdf_new_statement_from_nodes(world,NULL,
				librdf_new_node_from_node(rdftype),
				librdf_new_node_from_node(owlfp)))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Unable to query for FP's in model '%s'\n",t0,opts.model);
			return(1);
		}
		if (!(fpstream=librdf_model_find_statements(model,fpstatement))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: Failed to find any FP's in model '%s'\n",t0,opts.model);
			return(1);
		}
		while (!librdf_stream_end(fpstream)) {
			librdf_node *fp=librdf_statement_get_subject(librdf_stream_get_object(fpstream));
			librdf_node *context=librdf_stream_get_context(fpstream);
			librdf_statement *st;
			if (!opts.safe || 
					(librdf_node_is_resource(fp)
					&& librdf_node_is_resource(context)
					&& (char*)librdf_uri_as_string(librdf_node_get_uri(fp))==
						strstr((char*)librdf_uri_as_string(librdf_node_get_uri(fp)),
							(char*)librdf_uri_as_string(librdf_node_get_uri(context))))) {
				/* Add relation to temporary model. */
				if (!(st=librdf_new_statement_from_nodes(world,
							librdf_new_node_from_node(fp),
							librdf_new_node_from_node(rdftype),
							librdf_new_node_from_node(owlfp)))
						|| librdf_model_add_statement(tempmodel,st)) {
					stamp(argv[0],t0);
					fprintf(stderr, "%s: Unable to add FP to temporary model.\n",t0);
					return(1);
				}
			} else if (opts.safe && opts.list) {
				stamp(argv[0],t0);
				fprintf(stderr, "%s: * Skipping [%s] from [%s]\n",t0,librdf_uri_as_string(librdf_node_get_uri(fp)),librdf_uri_as_string(librdf_node_get_uri(context)));
			}
			librdf_stream_next(fpstream);
		}
		librdf_free_stream(fpstream);
		librdf_free_statement(fpstatement);

		/* Smush on found owl:IFP's and owl:FP's. */
		if (!(pstream=librdf_model_as_stream(tempmodel))) {
			stamp(argv[0],t0);
			fprintf(stderr, "%s: No properties found in temporary model.\n",t0);
			return(1);
		}
		while (!librdf_stream_end(pstream)) {
			librdf_node *p=librdf_statement_get_subject(librdf_stream_get_object(pstream));
			librdf_node *t=librdf_statement_get_object(librdf_stream_get_object(pstream));
			if (librdf_node_equals(t,owlifp)) {
				if (smush(argv[0],t0,world,model,p,NULL)) {
					stamp(argv[0],t0);
					fprintf(stderr, "%s: Failed to smush.\n",t0);
				}
			} else if (librdf_node_equals(t,owlfp)) {
				if (smush(argv[0],t0,world,model,NULL,p)) {
					stamp(argv[0],t0);
					fprintf(stderr, "%s: Failed to smush.\n",t0);
				}
			} else {
				stamp(argv[0],t0);
				fprintf(stderr, "%s: Unknown type for %s\n",t0,librdf_uri_as_string(librdf_node_get_uri(p)));
			}
			librdf_stream_next(pstream);
		}
		librdf_free_model(tempmodel);
		librdf_free_storage(tempstorage);
	}

	librdf_model_sync(model);
	if (!opts.quiet) {
		stamp(argv[0],t0);
		fprintf(stdout, "%s: * Done.\n",t0);
		fflush(stdout);
	}

	/* Clean up. */
	if (opts.context)
		librdf_free_node(opts.context);
	if (opts.ifp)
		librdf_free_node(opts.ifp);
	if (opts.fp)
		librdf_free_node(opts.fp);
	librdf_free_model(model);
	librdf_free_storage(storage);
	librdf_free_world(world);
	free(storage_options);
	free(storage_type);
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

librdf_statement *prop_filter(librdf_stream *stream, void *map_context, librdf_statement *statement)
{
	librdf_node *prop=(librdf_node*)map_context;
	if (librdf_node_equals(librdf_statement_get_predicate(statement),prop))
		return statement;
	return NULL;
}

int smush(char *argv0,char *t0,librdf_world *world,librdf_model *model,librdf_node *ifp,librdf_node *fp)
{
	librdf_storage *tempstorage;
	librdf_model *tempmodel;
	librdf_stream *ostream, *sstream, *rstream;
	librdf_statement *osearch, *ssearch, *rewrites;
	librdf_node *object, *prevobject, *canonical, *subject, *prevsubject, *predicate;
	int rc;

	if (!ifp && !fp) {
		stamp(argv0,t0);
		fprintf(stderr, "%s: No property to smush on?\n",t0);
		return 1;
	}
	
	if (!opts.quiet || opts.list) {
		unsigned char *p;
		stamp(argv0,t0);
		if (ifp) {
			p=librdf_node_to_string(ifp);
			fprintf(stdout, "%s: * Smushing on IFP %s...\n",t0,p);
			fflush(stdout);
			free(p);
		}
		if (fp) {
			p=librdf_node_to_string(fp);
			fprintf(stdout, "%s: * Smushing on FP %s...\n",t0,p);
			fflush(stdout);
			free(p);
		}
		if(opts.list)
			return 0;
	}

	/* Create in-memory storage/model to hold rewrites, to avoid deadlocks. */
	if (!(tempstorage=librdf_new_storage(world,"memory","dummy","")))
		return 1;
	if (!(tempmodel=librdf_new_model(world,tempstorage,NULL)))
		return 2;

	/* Find objects of statements involving IFP. */
	osearch=NULL;
	ostream=NULL;
	prevobject=NULL;
	if (ifp && opts.context) {
		ostream=librdf_model_context_as_stream(model,opts.context);
		if (ostream)
			librdf_stream_add_map(ostream,prop_filter,NULL,ifp);
	} else if (ifp) {
		if (!(osearch=librdf_new_statement_from_nodes(world,NULL,
				librdf_new_node_from_node(ifp),NULL)))
			return 3;
		ostream=librdf_model_find_statements(model,osearch);
	}
	/* Loop through all objects of IFP (possibly only in given context) */
	while (ifp && ostream && !librdf_stream_end(ostream)) {
		if (!(object=librdf_new_node_from_node(librdf_statement_get_object(librdf_stream_get_object(ostream)))))
			return 4;
		/* Skip empty literal values (a source of error) and duplicates... */
		if ((!prevobject || !librdf_node_equals(object,prevobject)) &&
				(librdf_node_get_type(object)!=LIBRDF_NODE_TYPE_LITERAL ||
				strlen((char*)librdf_node_get_literal_value(object)))) {
			if (prevobject)
				librdf_free_node(prevobject);
			if (!(prevobject=librdf_new_node_from_node(object)))
				return 5;
			/* Find all subjects with given value for IFP. */
			if (!(ssearch=librdf_new_statement_from_nodes(world,NULL,
					librdf_new_node_from_node(ifp),
					librdf_new_node_from_node(object))))
				return 6;
			sstream=librdf_model_find_statements(model,ssearch);
			/* Check not really necessary, but still... */
			if (!sstream || librdf_stream_end(sstream)) {
				librdf_free_statement(ssearch);
				if (sstream)
					librdf_free_stream(sstream);
				librdf_stream_next(ostream);
				continue;
			}
			/* Save first rewritable candidate subject as canonical. */
			canonical=librdf_new_node_from_node(librdf_statement_get_subject(librdf_stream_get_object(sstream)));
			if (!opts.quiet) {
				stamp(argv0,t0);
				unsigned char *o=librdf_node_to_string(object);
				unsigned char *c=librdf_node_to_string(canonical);
				fprintf(stdout, "%s:   %s %s\n",t0,o,c);
				fflush(stdout);
				free(o);
				free(c);
			}
			/* Rewrite other nodes to canonical. */
			prevsubject=NULL;
			while (!librdf_stream_end(sstream)) {
				subject=librdf_new_node_from_node(librdf_statement_get_subject(librdf_stream_get_object(sstream)));
				/* No need to rewrite canonical and duplicates... */
				if (librdf_node_equals(canonical,subject) ||
						(prevsubject && librdf_node_equals(subject,prevsubject))) {
					librdf_free_node(subject);
					librdf_stream_next(sstream);
					continue;
				}
				if (prevsubject)
					librdf_free_node(prevsubject);
				if (!(prevsubject=librdf_new_node_from_node(subject)))
					return 7;
				if (!opts.quiet) {
					stamp(argv0,t0);
					unsigned char *s=librdf_node_to_string(subject);
					fprintf(stdout, "%s:    %s\n",t0,s);
					fflush(stdout);
					free(s);
				}
				/* Add relation to temporary model. */
				if (!(rewrites=librdf_new_statement_from_nodes(world,
							librdf_new_node_from_node(subject),
							librdf_new_node_from_node(object),
							librdf_new_node_from_node(canonical))))
					return 8;
				if (librdf_model_add_statement(tempmodel,rewrites))
					return 9;
				librdf_free_statement(rewrites);
				librdf_free_node(subject);
				librdf_stream_next(sstream);
			}
			librdf_free_node(canonical);
			librdf_free_stream(sstream);
			librdf_free_statement(ssearch);
			if (prevsubject)
				librdf_free_node(prevsubject);
		}
		librdf_free_node(object);
		librdf_stream_next(ostream);
	}
	if (prevobject)
		librdf_free_node(prevobject);
	if (ostream)
		librdf_free_stream(ostream);
	if (osearch)
		librdf_free_statement(osearch);

	/* Find subjects of statements involving FP. */
	ssearch=NULL;
	sstream=NULL;
	prevsubject=NULL;
	if (fp && opts.context) {
		sstream=librdf_model_context_as_stream(model,opts.context);
		if (sstream)
			librdf_stream_add_map(sstream,prop_filter,NULL,fp);
	} else if (fp) {
		if (!(ssearch=librdf_new_statement_from_nodes(world,NULL,
				librdf_new_node_from_node(fp),NULL)))
			return 3;
		sstream=librdf_model_find_statements(model,ssearch);
	}
	/* Loop through all subjects of FP (possibly only in given context) */
	while (fp && sstream && !librdf_stream_end(sstream)) {
		if (!(subject=librdf_new_node_from_node(librdf_statement_get_subject(librdf_stream_get_object(sstream)))))
			return 4;
		/* Skip duplicates... */
		if ((!prevsubject || !librdf_node_equals(subject,prevsubject))) {
			if (prevsubject)
				librdf_free_node(prevsubject);
			if (!(prevsubject=librdf_new_node_from_node(subject)))
				return 5;
			/* Find all objects with given subject for FP. */
			if (!(osearch=librdf_new_statement_from_nodes(world,
					librdf_new_node_from_node(subject),
					librdf_new_node_from_node(fp),
					NULL)))
				return 6;
			ostream=librdf_model_find_statements(model,osearch);
			/* Check not really necessary, but still... */
			if (!ostream || librdf_stream_end(ostream)) {
				librdf_free_statement(osearch);
				if (ostream)
					librdf_free_stream(ostream);
				librdf_stream_next(sstream);
				continue;
			}
			/* Save first rewritable candidate object as canonical. */
			canonical=librdf_new_node_from_node(librdf_statement_get_object(librdf_stream_get_object(sstream)));
			if (!opts.quiet) {
				stamp(argv0,t0);
				unsigned char *o=librdf_node_to_string(subject);
				unsigned char *c=librdf_node_to_string(canonical);
				fprintf(stdout, "%s:   %s %s\n",t0,o,c);
				fflush(stdout);
				free(o);
				free(c);
			}
			/* Rewrite other nodes to canonical. */
			prevobject=NULL;
			while (!librdf_stream_end(ostream)) {
				object=librdf_new_node_from_node(librdf_statement_get_object(librdf_stream_get_object(ostream)));
				/* No need to rewrite canonical and duplicates... */
				if (librdf_node_equals(canonical,object) ||
						(prevobject && librdf_node_equals(object,prevobject))) {
					librdf_free_node(object);
					librdf_stream_next(ostream);
					continue;
				}
				if (prevobject)
					librdf_free_node(prevobject);
				if (!(prevobject=librdf_new_node_from_node(object)))
					return 7;
				if (!opts.quiet) {
					stamp(argv0,t0);
					unsigned char *s=librdf_node_to_string(object);
					fprintf(stdout, "%s:    %s\n",t0,s);
					fflush(stdout);
					free(s);
				}
				/* Add relation to temporary model. */
				if (!(rewrites=librdf_new_statement_from_nodes(world,
							librdf_new_node_from_node(object),
							librdf_new_node_from_node(subject),
							librdf_new_node_from_node(canonical))))
					return 8;
				if (librdf_model_add_statement(tempmodel,rewrites))
					return 9;
				librdf_free_statement(rewrites);
				librdf_free_node(object);
				librdf_stream_next(ostream);
			}
			librdf_free_node(canonical);
			librdf_free_stream(ostream);
			librdf_free_statement(osearch);
			if (prevobject)
				librdf_free_node(prevobject);
		}
		librdf_free_node(subject);
		librdf_stream_next(sstream);
	}
	if (prevsubject)
		librdf_free_node(prevsubject);
	if (sstream)
		librdf_free_stream(sstream);
	if (ssearch)
		librdf_free_statement(ssearch);

	/* Rewrite... */
	if(librdf_model_size(tempmodel)) {
		if (!opts.quiet) {
			stamp(argv0,t0);
			unsigned char *p;
			if (ifp) {
				p=librdf_node_to_string(ifp);
				fprintf(stdout, "%s: * Performing %d rewrites for IFP %s...\n",t0,librdf_model_size(tempmodel),p);
				fflush(stdout);
				free(p);
			}
			if (fp) {
				p=librdf_node_to_string(fp);
				fprintf(stdout, "%s: * Performing %d rewrites for FP %s...\n",t0,librdf_model_size(tempmodel),p);
				fflush(stdout);
				free(p);
			}
		}

		/* Find and perform queued rewrites. */
		rstream=librdf_model_as_stream(tempmodel);
		while (rstream && !librdf_stream_end(rstream)) {
			subject=librdf_statement_get_subject(librdf_stream_get_object(rstream));
			predicate=librdf_statement_get_predicate(librdf_stream_get_object(rstream));
			canonical=librdf_statement_get_object(librdf_stream_get_object(rstream));
			if (!opts.quiet) {
				unsigned char *p=librdf_node_to_string(predicate);
				unsigned char *s=librdf_node_to_string(subject);
				unsigned char *c=librdf_node_to_string(canonical);
				fprintf(stdout, "%s: %s -> %s\n",p,s,c);
				fflush(stdout);
				free(p);
				free(s);
				free(c);
			}
			/* Rewrite subjects... */
			if (!(rewrites=librdf_new_statement_from_nodes(world,
					librdf_new_node_from_node(subject),NULL,NULL)))
				return 16;
			rc=rewrite(world,model,canonical,subject,rewrites);
			if (rc)
				return rc+16;
			librdf_free_statement(rewrites);
			/* Rewrite objects... */
			if (!(rewrites=librdf_new_statement_from_nodes(world,NULL,NULL,
					librdf_new_node_from_node(subject))))
				return 32;
			rc=rewrite(world,model,canonical,subject,rewrites);
			if (rc)
				return rc+32;
			librdf_free_statement(rewrites);
			/* Rewrite predicates? */
			if (librdf_node_get_type(subject)==LIBRDF_NODE_TYPE_RESOURCE) {
				if (!(rewrites=librdf_new_statement_from_nodes(world,NULL,
						librdf_new_node_from_node(subject),NULL)))
					return 64;
				rc=rewrite(world,model,canonical,subject,rewrites);
				if (rc)
					return rc+64;
				librdf_free_statement(rewrites);
			}
			librdf_stream_next(rstream);
		}
		if (rstream)
			librdf_free_stream(rstream);
	}

	librdf_free_model(tempmodel);
	librdf_free_storage(tempstorage);

	return 0;
};

int rewrite(librdf_world *world, librdf_model *model, librdf_node *canonical, librdf_node *node, librdf_statement *search)
{
	librdf_stream *stream;
	librdf_statement *oldstatement, *newstatement;
	librdf_node *context, *object, *predicate, *subject;

	if (opts.test)
		return 0;

	/* Rewrite node to canonical in statements matching search. */
	while (1) {
		stream=librdf_model_find_statements(model,search);
		if (!stream)
			return 0;
		if (librdf_stream_end(stream)) {
			librdf_free_stream(stream);
			return 0;
		}
		/* Extract nodes from "old" statement and free stream. */
		oldstatement=librdf_new_statement_from_statement(librdf_stream_get_object(stream));
		object=librdf_new_node_from_node(librdf_statement_get_object(oldstatement));
		predicate=librdf_new_node_from_node(librdf_statement_get_predicate(oldstatement));
		subject=librdf_new_node_from_node(librdf_statement_get_subject(oldstatement));
		context=librdf_stream_get_context(stream);
		if (context)
			context=librdf_new_node_from_node(context);
		librdf_free_stream(stream);
		/* Rewrite object? */
		if (librdf_node_equals(object,node)) {
			librdf_free_node(object);
			object=librdf_new_node_from_node(canonical);
		}
		/* Rewrite predicate? */
		if (librdf_node_equals(predicate,node)) {
			librdf_free_node(predicate);
			predicate=librdf_new_node_from_node(canonical);
		}
		/* Rewrite subject? */
		if (librdf_node_equals(subject,node)) {
			librdf_free_node(subject);
			subject=librdf_new_node_from_node(canonical);
		}
		/* Create new statement. */
		if (!(newstatement=librdf_new_statement_from_nodes(world,
				subject,predicate,object)))
			return 1;
		/* Remove old statement. */
		if (context) {
			if (librdf_model_context_remove_statement(model,context,oldstatement))
				return 2;
		} else {
			if (librdf_model_remove_statement(model,oldstatement))
				return 2;
		}
		/* Insert new statement. */
		if (context) {
			if (librdf_model_context_add_statement(model,context,newstatement))
				return 3;
		} else {
			if (librdf_model_add_statement(model,newstatement))
				return 3;
		}
		librdf_free_statement(oldstatement);
		librdf_free_statement(newstatement);
		if (context)
			librdf_free_node(context);
	}
	return 0;
}

int getoptions(int argc,char *argv[],librdf_world *world)
{
	/* Define command line options. */
	struct option opts_long[]={
		{"help",no_argument,NULL,'?'},
		{"bulk",no_argument,NULL,'B'},
		{"context",required_argument,NULL,'c'},
		{"database",required_argument,NULL,'D'},
		{"directory",required_argument,NULL,'d'},
		{"fp",required_argument,NULL,'f'},
		{"host",required_argument,NULL,'h'},
		{"ifp",required_argument,NULL,'i'},
		{"list",no_argument,NULL,'l'},
		{"model",required_argument,NULL,'m'},
		{"port",required_argument,NULL,'P'},
		{"password",required_argument,NULL,'p'},
		{"quiet",no_argument,NULL,'q'},
		{"safe",no_argument,NULL,'s'},
		{"test",no_argument,NULL,'t'},
		{"user",required_argument,NULL,'u'},
		{"version",no_argument,NULL,'v'},
		{0,0,0,0}};
	const char *opts_short="?Bc:D:d:f:h:i:lm:P:p:qstu:v";
	int i=1;
	int c;
	char *buffer;
	int ttypasswd=1;

	/* Set defaults. */
	opts.context=NULL;
	opts.fp=NULL;
	opts.ifp=NULL;
	opts.list=0;
	opts.password=0;
	opts.port=3306;
	opts.quiet=0;
	opts.safe=0;
	opts.test=0;
	opts.user=0;
	if (!(opts.directory=strdup("./"))
			|| !(opts.bulk=strdup("no"))
			|| !(opts.host=strdup("mysql"))
			|| !(opts.model=strdup("redland"))
			|| !(opts.database=strdup("redland")))
	{
		fprintf(stderr,"%s: Failed to allocate default options\n",argv[0]);
		exit(1);
	};

	while ((c=getopt_long(argc,argv,opts_short,opts_long,&i))!=-1)
	{
		buffer=NULL;
		if (optarg)
		{
			buffer=malloc(strlen(optarg)+1);
			if (!buffer)
			{
				fprintf(stderr,"%s: Failed to allocate buffer for command line argument (%s)\n",argv[0],optarg);
				exit(1);
			};
			strncpy((char*)buffer,optarg,strlen(optarg)+1);
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
			case 'c':
				if (!(opts.context=librdf_new_node_from_uri_string(world,(unsigned char*)buffer))) {
					fprintf(stderr, "%s: Failed to create context node\n",argv[0]);
					usage(argv[0],0);
				}
				free(buffer);
				break;
			case 'D':
				free(opts.directory);
				opts.directory=0;
				free(opts.database);
				opts.database=buffer;
				break;
			case 'd':
				free(opts.database);
				opts.database=0;
				free(opts.directory);
				opts.directory=buffer;
				break;
			case 'f':
				if (!(opts.fp=librdf_new_node_from_uri_string(world,(unsigned char*)buffer))) {
					fprintf(stderr, "%s: Failed to create FP node\n",argv[0]);
					usage(argv[0],0);
				}
				free(buffer);
				break;
			case 'h':
				free(opts.host);
				opts.host=buffer;
				break;
			case 'i':
				if (!(opts.ifp=librdf_new_node_from_uri_string(world,(unsigned char*)buffer))) {
					fprintf(stderr, "%s: Failed to create IFP node\n",argv[0]);
					usage(argv[0],0);
				}
				free(buffer);
				break;
			case 'l':
				opts.list=1;
				break;
			case 'm':
				free(opts.model);
				opts.model=buffer;
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
			case 's':
				opts.safe=1;
				break;
			case 't':
				opts.test=1;
				break;
			case 'u':
				free(opts.user);
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
Smush a persistent Redland storage by owl:IFP's and owl:FP's.\n\
* Copyright (C) 2003-2005 Morten Frederiksen - http://purl.org/net/morten/\n\
* Copyright (C) 2000-2003 David Beckett - http://purl.org/net/dajobe/\n\
",argv0,VERSION);
	if (version)
		exit(0);
	printf("\n\
usage: %s [options]\n\
\n\
  -?, --help         Display this help message and exit.\n\
  -B, --bulk         Optimize for bulk loads by locking tables and disabling\n\
                     indices while loading. Note that this excludes all other\n\
                     access to the storage.\n\
  -c<uri>, --context=<uri>\n\
                     URI of context to smush.\n\
  -D<database>, --database=<database>\n\
                     Name of MySQL database to use, default is 'redland'.\n\
  -d<directory>, --directory=<directory>\n\
                     Directory to use for BDB files. When provided implies use\n\
                     of 'hashes' storage type instead of 'mysql'.\n\
  -f<uri>, --fp=<uri>\n\
                     URI of owl:FunctionalProperty to smush on. If not\n\
                     specified, all IFP's and FP's in model will be used.\n\
  -h<host name>, --host=<host name>\n\
                     Host to contact for MySQL connections, default is 'mysql'.\n\
  -i<uri>, --ifp=<uri>\n\
                     URI of owl:InverseFunctionalProperty to smush on. If not\n\
                     specified, all IFP's and FP's in model will be used.\n\
  -l, --list\n\
                     Only list IFP's and FP's that would be used for smushing.\n\
  -m<model id>, --id=<model id>\n\
                     Identifier for (name of) storage (model name for storage\n\
                     type 'mysql', base file name for storage type 'hashes'),\n\
                     default is 'redland'.\n\
  -p<password>, --password=<password>\n\
                     Password to use when connecting to MySQL server.\n\
                     If password is not given it's asked from the tty.\n\
  -P<port number>, --port=<port number>\n\
                     The port number to use when connecting to MySQL server.\n\
                     Default port number is 3306.\n\
  -q, --quiet\n\
                     No informational messages, only errors.\n\
  -s, --safe\n\
                     Only use IFP's and FP's that are stated by (determined by\n\
                     its \"context\") a prefix of the URI.\n\
  -t, --test\n\
                     Test only, don't perform rewrites, just list them as\n\
                     doubles (original and canonical).\n\
  -u<user name>, --user=<user name>\n\
                     User name for MySQL server.\n\
  -v, --version      Output version information and exit.\n\
\n\
",argv0);
	exit(1);
}
