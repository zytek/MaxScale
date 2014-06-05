/*
 * This file is distributed as part of MaxScale by SkySQL.  It is free
 * software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright SkySQL Ab 2014
 */
#include <stdio.h>
#include <filter.h>
#include <modinfo.h>
#include <modutil.h>
#include <string.h>
#include <regex.h>

/**
 * regexfilter.c - a very simple regular expression rewrite filter.
 *
 * A simple regular expression query rewrite filter.
 * Two parameters should be defined in the filter configuration
 *	match=<regular expression>
 *	replace=<replacement text>
 */

MODULE_INFO 	info = {
	MODULE_API_FILTER,
	MODULE_ALPHA_RELEASE,
	FILTER_VERSION,
	"A query rewrite filter that uses regular expressions to rewite queries"
};

static char *version_str = "V1.0.0";

static	FILTER	*createInstance(char **options, FILTER_PARAMETER **params);
static	void	*newSession(FILTER *instance, SESSION *session);
static	void 	closeSession(FILTER *instance, void *session);
static	void 	freeSession(FILTER *instance, void *session);
static	void	setDownstream(FILTER *instance, void *fsession, DOWNSTREAM *downstream);
static	int	routeQuery(FILTER *instance, void *fsession, GWBUF *queue);
static	void	diagnostic(FILTER *instance, void *fsession, DCB *dcb);

static char	*regex_replace(char *sql, int length, regex_t *re, char *replace);

static FILTER_OBJECT MyObject = {
    createInstance,
    newSession,
    closeSession,
    freeSession,
    setDownstream,
    routeQuery,
    diagnostic,
};

/**
 * Instance structure
 */
typedef struct {
	char	*match;		/* Regular expression to match */
	char	*replace;	/* Replacement text */
	regex_t	re;		/* Compiled regex text */
} REGEX_INSTANCE;

/**
 * The session structure for this regex filter
 */
typedef struct {
	DOWNSTREAM	down;
	int		no_change;
	int		replacements;
} REGEX_SESSION;

/**
 * Implementation of the mandatory version entry point
 *
 * @return version string of the module
 */
char *
version()
{
	return version_str;
}

/**
 * The module initialisation routine, called when the module
 * is first loaded.
 */
void
ModuleInit()
{
}

/**
 * The module entry point routine. It is this routine that
 * must populate the structure that is referred to as the
 * "module object", this is a structure with the set of
 * external entry points for this module.
 *
 * @return The module object
 */
FILTER_OBJECT *
GetModuleObject()
{
	return &MyObject;
}

/**
 * Create an instance of the filter for a particular service
 * within MaxScale.
 * 
 * @param options	The options for this filter
 *
 * @return The instance data for this new instance
 */
static	FILTER	*
createInstance(char **options, FILTER_PARAMETER **params)
{
REGEX_INSTANCE	*my_instance;
int		i;

	if ((my_instance = calloc(1, sizeof(REGEX_INSTANCE))) != NULL)
	{
		my_instance->match = NULL;
		my_instance->replace = NULL;

		for (i = 0; params[i]; i++)
		{
			if (!strcmp(params[i]->name, "match"))
				my_instance->match = strdup(params[i]->value);
			if (!strcmp(params[i]->name, "replace"))
				my_instance->replace = strdup(params[i]->value);
		}
		if (my_instance->match == NULL || my_instance->replace == NULL)
		{
			return NULL;
		}

		if (regcomp(&my_instance->re, my_instance->match, REG_ICASE))
		{
			free(my_instance->match);
			free(my_instance->replace);
			free(my_instance);
			return NULL;
		}
	}
	return (FILTER *)my_instance;
}

/**
 * Associate a new session with this instance of the filter.
 *
 * @param instance	The filter instance data
 * @param session	The session itself
 * @return Session specific data for this session
 */
static	void	*
newSession(FILTER *instance, SESSION *session)
{
REGEX_SESSION	*my_session;

	if ((my_session = calloc(1, sizeof(REGEX_SESSION))) != NULL)
	{
		my_session->no_change = 0;
		my_session->replacements = 0;
	}

	return my_session;
}

/**
 * Close a session with the filter, this is the mechanism
 * by which a filter may cleanup data structure etc.
 *
 * @param instance	The filter instance data
 * @param session	The session being closed
 */
static	void 	
closeSession(FILTER *instance, void *session)
{
}

/**
 * Free the memory associated with this filter session.
 *
 * @param instance	The filter instance data
 * @param session	The session being closed
 */
static void
freeSession(FILTER *instance, void *session)
{
	free(session);
        return;
}

/**
 * Set the downstream component for this filter.
 *
 * @param instance	The filter instance data
 * @param session	The session being closed
 * @param downstream	The downstream filter or router
 */
static void
setDownstream(FILTER *instance, void *session, DOWNSTREAM *downstream)
{
REGEX_SESSION	*my_session = (REGEX_SESSION *)session;

	my_session->down = *downstream;
}

/**
 * The routeQuery entry point. This is passed the query buffer
 * to which the filter should be applied. Once applied the
 * query shoudl normally be passed to the downstream component
 * (filter or router) in the filter chain.
 *
 * @param instance	The filter instance data
 * @param session	The filter session
 * @param queue		The query data
 */
static	int	
routeQuery(FILTER *instance, void *session, GWBUF *queue)
{
REGEX_INSTANCE	*my_instance = (REGEX_INSTANCE *)instance;
REGEX_SESSION	*my_session = (REGEX_SESSION *)session;
char		*sql, *newsql;
int		length;

	if (modutil_is_SQL(queue))
	{
		modutil_extract_SQL(queue, &sql, &length);
		newsql = regex_replace(sql, length, &my_instance->re,
					my_instance->replace);
		if (newsql)
		{
			queue = modutil_replace_SQL(queue, newsql);
			free(newsql);
			my_session->replacements++;
		}
		else
			my_session->no_change++;
		
	}
	return my_session->down.routeQuery(my_session->down.instance,
			my_session->down.session, queue);
}

/**
 * Diagnostics routine
 *
 * If fsession is NULL then print diagnostics on the filter
 * instance as a whole, otherwise print diagnostics for the
 * particular session.
 *
 * @param	instance	The filter instance
 * @param	fsession	Filter session, may be NULL
 * @param	dcb		The DCB for diagnostic output
 */
static	void
diagnostic(FILTER *instance, void *fsession, DCB *dcb)
{
REGEX_INSTANCE	*my_instance = (REGEX_INSTANCE *)instance;
REGEX_SESSION	*my_session = (REGEX_SESSION *)fsession;

	dcb_printf(dcb, "\t\tSearch and replace: 			s/%s/%s/\n",
			my_instance->match, my_instance->replace);
	if (my_session)
	{
		dcb_printf(dcb, "\t\tNo. of queries unaltered by filter:	%d\n",
			my_session->no_change);
		dcb_printf(dcb, "\t\tNo. of queries altered by filter:		%d\n",
			my_session->replacements);
	}
}

/**
 * Perform a regular expression match and subsititution on the SQL
 *
 * @param	sql	The original SQL text
 * @param	length	The length of the SQL text
 * @param	re	The compiled regular expression
 * @param	replace	The replacement text
 * @return	The replaced text or NULL if no replacement was done.
 */
static char *
regex_replace(char *sql, int length, regex_t *re, char *replace)
{
char		*orig, *result, *ptr;
int		i, res_size, res_length, rep_length;
int		last_match;
regmatch_t	match[10];

	orig = strndup(sql, length);
	if (regexec(re, orig, 10, match, 0))
	{
		free(orig);
		return NULL;
	}
	res_size = 2 * length;
	result = (char *)malloc(res_size);
	res_length = 0;
	rep_length = strlen(replace);
	last_match = 0;
	
	for (i = 0; i < 10; i++)
	{
		if (match[i].rm_so != -1)
		{
			if (res_length + match[i].rm_so > res_size)
			{
				result = (char *)realloc(result, res_size + length);
				res_size += length;
			}
			ptr = &result[res_length];
			if (last_match < match[i].rm_so)
			{
				int to_copy = match[i].rm_so - last_match;
				memcpy(ptr, &sql[last_match], to_copy);
				res_length += to_copy;
			}
			last_match = match[i].rm_eo;
			if (res_length + match[i].rm_so > res_size)
			{
				result = (char *)realloc(result, res_size + rep_length);
				res_size += length;
			}
			ptr = &result[res_length];
			memcpy(ptr, replace, rep_length);
			res_length += rep_length;
		}
	}
	if (res_length + length - last_match + 1 > res_size)
	{
		result = (char *)realloc(result, res_size + length);
		res_size += length;
	}
	if (last_match < length)
	{
		ptr = &result[res_length];
		memcpy(ptr, &sql[last_match], length - last_match);
		res_length += length - last_match;
	}
	result[res_length] = 0;

	return result;
}
