#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

#define QUERY_FIELD_QID 0
#define QUERY_FIELD_A1 1
#define QUERY_FIELD_A2 2
#define QUERY_FIELD_A3 3
#define QUERY_FIELD_A4 4
#define QUERY_FIELD_BS 5
#define QUERY_FIELD_BE 6

Person *person_map;
short *scoredP;
unsigned int *knows_map;
unsigned short *interest_map;

unsigned long person_length, knows_length, interest_length;

FILE *outfile;

int result_comparator(const void *v1, const void *v2) {
    Result *r1 = (Result *) v1;
    Result *r2 = (Result *) v2;
    if (r1->score > r2->score)
        return -1;
    else if (r1->score < r2->score)
        return +1;
    else if (r1->person_id < r2->person_id)
        return -1;
    else if (r1->person_id > r2->person_id)
        return +1;
     else if (r1->knows_id < r2->knows_id)
        return -1;
    else if (r1->knows_id > r2->knows_id)
        return +1;
    else
        return 0;
}

unsigned int get_score(Person *person, unsigned short areltd[]) {
	long interest_offset;
	unsigned short interest;
	unsigned int score = 0;
	for (interest_offset = person->interests_first; 
		interest_offset < person->interests_first + person->interest_n; 
		interest_offset++) {

		interest = interest_map[interest_offset];
		if (areltd[0] == interest) score++;
		if (areltd[1] == interest) score++;
		if (areltd[2] == interest) score++;
		// early exit
		if (score > 2) {
			break;
		}
	}
	return score;
}

char likes_artist(Person *person, unsigned short artist) {
	long interest_offset;
	unsigned short interest;
	unsigned short likesartist = 0;

	for (interest_offset = person->interests_first; 
		interest_offset < person->interests_first + person->interest_n; 
		interest_offset++) {

		interest = interest_map[interest_offset];
		if (interest == artist) {
			likesartist = 1;
			break;
		}
	}
	return likesartist;
}

void query(unsigned short qid, unsigned short artist, unsigned short areltd[], unsigned short bdstart, unsigned short bdend) {
	unsigned long person_offset;
	unsigned long knows_offset, knows_offset2;

	Person *person, *knows;

	unsigned int result_length = 0, new_result_length = 0, result_idx, result_set_size = 1000, new_result_set_size = 1000;
	unsigned int scored_length = 0, scored_set_size = 2000;
	unsigned int scSize = 4000;
	scoredP = malloc(scSize*sizeof(short));

	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
		person = &person_map[person_offset];

		// if (person_offset > 0 && person_offset % REPORTING_N == 0) {
		// 	printf("%.2f%%\n", 100 * (person_offset * 1.0/(person_length/sizeof(Person))));
		// }
		 //25395
		 if (person_offset >= scSize) {
					scSize *= 2;
					scoredP = realloc(scoredP, scSize * sizeof (short));
				}
		scoredP[person_offset] = likes_artist(person, artist) ? -1 : get_score(person, areltd);		
		
	}
	//printf("1st case done!");
	//Iterating to get list of possible friends..

	Result* results = malloc(result_set_size * sizeof (Result));
	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
		short score = scoredP[person_offset];
		if (score < 1) continue;
		person = &person_map[person_offset];
		// filter by birthday
		if (person->birthday < bdstart || person->birthday > bdend) continue; 
		
		for (knows_offset = person->knows_first; knows_offset < person->knows_first + person->knows_n; knows_offset++) {
			unsigned int kp = knows_map[knows_offset];
			knows = &person_map[kp];
//			if (person->location != knows->location) continue;
			if (scoredP[kp] != -1) continue;
			if (result_length >= result_set_size) {
				result_set_size *= 2;
				results = realloc(results, result_set_size * sizeof (Result));
			}
			results[result_length].person_offset = person_offset;
			results[result_length].person_id = person->person_id;
			results[result_length].knows_offset = kp;
			results[result_length].knows_id = knows->person_id;
			results[result_length].score = score;
			result_length++;
		}
	}
	
	// // sort result
	qsort(results, result_length, sizeof(Result), &result_comparator);

	// //free(scoredP);
	//Step of mutual friendship..
	for (person_offset = 0; person_offset < result_length; ++person_offset) {
		knows = &person_map[results[person_offset].knows_offset];
		unsigned long poff = results[person_offset].person_offset;
		for (knows_offset2 = knows->knows_first; knows_offset2 < knows->knows_first + knows->knows_n; knows_offset2++) {
			if (knows_map[knows_offset2] == poff) {
				fprintf(outfile, "%d|%d|%lu|%lu\n", qid, results[person_offset].score, results[person_offset].person_id, results[person_offset].knows_id);
				// newResults[new_result_length].knows_id = knows->person_id;
				// newResults[new_result_length].person_id = person_map[poff].person_id;
				// newResults[new_result_length].score = results[person_offset].score;
				// new_result_length++;
			}
		}
	}

	// fprintf(outfile,"3rd case done!");

	// // output
	// for (result_idx = 0; result_idx < new_result_length; result_idx++) {
	// 	fprintf(outfile, "%d|%d|%lu|%lu\n", qid, newResults[result_idx].score, 
	// 		newResults[result_idx].person_id, newResults[result_idx].knows_id);
	// }

	// fprintf(outfile,"done");
	//free(results);
	//free(scoredP);
}

void query_line_handler(unsigned char nfields, char** tokens) {
	unsigned short q_id, q_artist, q_bdaystart, q_bdayend;
	unsigned short q_relartists[3];

	q_id            = atoi(tokens[QUERY_FIELD_QID]);
	q_artist        = atoi(tokens[QUERY_FIELD_A1]);
	q_relartists[0] = atoi(tokens[QUERY_FIELD_A2]);
	q_relartists[1] = atoi(tokens[QUERY_FIELD_A3]);
	q_relartists[2] = atoi(tokens[QUERY_FIELD_A4]);
	q_bdaystart     = birthday_to_short(tokens[QUERY_FIELD_BS]);
	q_bdayend       = birthday_to_short(tokens[QUERY_FIELD_BE]);
	
	query(q_id, q_artist, q_relartists, q_bdaystart, q_bdayend);
}

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr, "Usage: [datadir] [query file] [results file]\n");
		exit(1);
	}
	/* memory-map files created by loader */
	person_map   = (Person *)         mmapr(makepath(argv[1], "person_temp2",   "bin"), &person_length);
	interest_map = (unsigned short *) mmapr(makepath(argv[1], "interest", "bin"), &interest_length);
	knows_map    = (unsigned int *)   mmapr(makepath(argv[1], "knows_temp2",    "bin"), &knows_length);
	//scoredP = malloc(person_length/sizeof(Person));

  	outfile = fopen(argv[3], "w");  
  	if (outfile == NULL) {
  		fprintf(stderr, "Can't write to output file at %s\n", argv[3]);
		exit(-1);
  	}

  	/* run through queries */
	parse_csv(argv[2], &query_line_handler);
	return 0;
}
