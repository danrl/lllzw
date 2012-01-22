/*
 * lllzw - Implementation of the Lempel-Ziv-Welch algorithm.
 * 
 * Copyright (C) 2012  Dan Luedtke
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define LL_BUFFER_SIZE		1000

/* -------------------------------------------------------------------------- */

/*
 * info
 */

const char *progname = "lllzw";
const char *progversion = "0.1 alpha (Jan 2012)";

static void show_version (void)
{
	fprintf(stderr, "%s %s\n", progname, progversion);
}

/* -------------------------------------------------------------------------- */

/*
 * dictionary
 */

#define LL_BIT_WIDTH		9
#define LL_DICT_SIZE		(1<<LL_BIT_WIDTH)
#define LL_DICT_OFFSET		256

struct ll_dict {
	int		idx;
	char		*db[LL_DICT_SIZE];
};

/**
 * dict_init() - initializes the dictionary
 */
static struct ll_dict *dict_init (void)
{
	struct ll_dict *dict;
	int i;
	char tmp[2];

	dict = calloc(1, sizeof(struct ll_dict));
	for (i=0; i < LL_DICT_OFFSET; i++) {
		tmp[0] = (char) i;
		tmp[1] = '\0';
		dict->db[i] = malloc(2);
		strncpy(dict->db[i], &tmp[0], 2);
	}
	dict->idx = LL_DICT_OFFSET; /* this creates a gap. we like! */
	fprintf(stderr, "DICT: init\n");
	return dict;
}

/**
 * dict_destroy() - destroys a given dictionary
 * @dict:	dictionary to destroy
 */
static struct ll_dict *dict_destroy (struct ll_dict *dict)
{
	int i;

	if (dict == NULL)
		goto exit;
	for (i = 0; i < LL_DICT_SIZE; i++) {
		if (dict->db[i] != NULL) {
			free(dict->db[i]);
		}
	}
	free(dict);
	fprintf(stderr, "DICT: destroyed\n");
exit:
	return NULL;
}

/**
 * dict_add() - adds a value into dictionary
 * @dict:	dictionary
 * @value:	value to add
 *
 * Returns key of new value on success, -1 on error.
 */
static int dict_add (struct ll_dict *dict, char *value)
{
	int nidx;

	nidx = dict->idx +1;
	if (nidx >= LL_DICT_SIZE)
		return -1;
	dict->db[nidx] = malloc(strlen(value) +1);
	strncpy(dict->db[nidx], value, sizeof(dict->db[nidx]));
	dict->idx = nidx;
	fprintf(stderr, "DICT: added idx=%d value={%s}\n", nidx, value);

	return nidx;
}

/**
 * dict_find() - finds the key for a given value in dictionary
 * @dict:	dictionary
 * @value:	value to search for
 *
 * Returns key of given value on success, -1 on error.
 */
static int dict_find (struct ll_dict *dict, char *value)
{
	int i;

	fprintf(stderr, "DICT: find value={%s}... ", value);
	for (i = 0; i < dict->idx; i++) {
		if (dict->db[i] == NULL)
			continue;
		if (strncmp(dict->db[i], value, strlen(value)) == 0) {
			fprintf(stderr, "found at idx=%d\n", i);
			return i;
		}
	}
	fprintf(stderr, "not found\n");
	return -1;
}

/**
 * dict_dump() - prints all values of dictionary to stderr
 * @dict:	dictionary
 */
static void dict_dump (struct ll_dict *dict)
{
	int i;

	for (i = LL_DICT_OFFSET; i < dict->idx; i++) {
		if (dict->db[i] == NULL)
			continue;
		fprintf(stderr, "DUMP: idx=%d value={%s}\n", i, dict->db[i]);
	}
}

/* -------------------------------------------------------------------------- */

static uint32_t queue = 0;
static int queue_nbit = 8;
static int queue_pos = 0;

/**
 * queue_put() - enqueues a 9-bit x for 8-bit output
 * @x:		item to enque
 *
 * Caution: Signdness will be ignored!
 */
static void queue_put (int x)
{
	uint32_t tmp;
	/* verbose */
	if (x < LL_DICT_OFFSET) {
		fprintf(stderr,
			"CHAR: - - - - - - - - - - - - - - - - - >{%c}\n",
			(char) x);
	} else {
		fprintf(stderr,
			"CODE: - - - - - - - - - - - - - - - - - >[%d]\n",
			x);
	}
	/* bit-width detect */
	/*
	int i;
	int tbit;
	for (i = queue_nbit; i < LL_BIT_WIDTH; i++) {
		tbit = 1 << (i-1);
		if (x > tbit && i > queue_nbit) {
			queue_nbit = i;
			fprintf(stderr,
				"BITD: bit detection nbit=%d\n",
				queue_nbit);
		}
	}
	*/
	/* enqueue */
	queue = queue << queue_nbit;
	queue = queue | (x & 0x1FF);
	queue_pos += queue_nbit;
	fprintf(stderr, "QUEUE=%d\n", queue);
	/* flush */
	while (queue_pos >= 8) {
		tmp = queue;
		tmp = tmp << (32 - queue_pos);
		tmp = tmp >> 24;
		fprintf(stdout, "%c", (char) (tmp & 0xFF));
		queue_pos -= 8;
	}
	queue_nbit=9;
}

/**
 * queue_put_s() - enqueues a string
 * @s:		string to enque
 */
static void queue_put_s (char *s)
{
	int i;
	for (i = 0; i < strlen(s); i++) {
		queue_put((int) s[i]);
	}
}

/**
 * queue_flush() - empty queue 8-bit aligned
 */
static void queue_flush (void)
{
	while (queue_pos%queue_nbit) {
		queue_put(256);
	}
}

/* -------------------------------------------------------------------------- */

int main(int argc, char *argv[]) {
	struct ll_dict *dict;
	char c;
	char str[LL_BUFFER_SIZE+1];
	char curstr[LL_BUFFER_SIZE+1];
	int code;
	int curcode;

	show_version();
	dict = dict_init();

	/* Z-Magic */
	fprintf(stdout, "%c%c", 0x1f, 0x9d);
	/* Header */
	fprintf(stdout, "%c", (char) (0x80 | 9));

	str[0] = '\0';
	curstr[0] = '\0';
	while ((c = fgetc(stdin)) != EOF) {
		strncat(curstr, &c, 1);
		fprintf(stderr, "ALGO: curstr={%s}\n", curstr);
		curcode = dict_find(dict, curstr);
		if (curcode < 0) {
			dict_add(dict, curstr);
			if (code < 0)
				queue_put_s(str);
			else
				queue_put(code);
			curstr[0] = c;
			curstr[1] = '\0';
		}
		code = curcode;
		strncpy(str, curstr, LL_BUFFER_SIZE);
	}
	if (code < 0)
		queue_put_s(str);
	else
		queue_put(code);
	dict_dump(dict);
	dict = dict_destroy(dict);
	queue_flush();
	exit(EXIT_SUCCESS);
}
