#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include "utils.h"

Person* person_map;
unsigned int person_length;
unsigned int* knows_map;
unsigned int knows_length;


FILE* person_file;
FILE* knows_file;

int index_comparator( const void* a, const void* b) {
    Index* ra = (Index*)a;
    Index* rb = (Index*)b;
    return ra->new_index - rb->new_index;
}

void diffLocationChecker(const char* person_output_file, const char* knows_output_file);

void biDirectionalityChecker(const char* person_output_file, const char* knows_output_file);

int main(int argc, char *argv[]) {

    if(argc != 2) {
        printf("Invalid usage\n");
        exit(1);
    }

//	char* person_output_file   = makepath(argv[1], "person",   "bin");
//	char* interest_output_file = makepath(argv[1], "interest", "bin");
//	char* knows_output_file    = makepath(argv[1], "knows",    "bin");
	
	// this does not do anything yet. But it could...

    char *person_input_file = makepath(argv[1], "person", "bin");
    char *knows_input_file = makepath(argv[1], "knows", "bin");

    size_t size;

    FILE *person_file;
    person_map = (Person*) mmap_file(person_input_file, &size, &person_file );
    person_length = size / sizeof(Person);

    FILE *knows_file;
    knows_map = (unsigned int*) mmap_file (knows_input_file, &size, &knows_file );
    knows_length = size / sizeof(unsigned int);

    char* person_output_file   = makepath(argv[1], "person_temp",   "bin");
    char* knows_output_file    = makepath(argv[1], "knows_temp",    "bin");

    diffLocationChecker(person_output_file, knows_output_file);

    munmap(person_map, person_length* sizeof(Person));
    munmap(knows_map, knows_length* sizeof(unsigned int));
    fclose(person_file);
    fclose(knows_file);

    person_map = (Person*) mmap_file(person_output_file, &size, &person_file);
    person_length = size / sizeof(Person);

    knows_map = (unsigned int*) mmap_file(knows_output_file, &size, &knows_file);
    knows_length = size / sizeof(unsigned int);

    person_output_file   = makepath(argv[1], "person_temp2",   "bin");
    knows_output_file    = makepath(argv[1], "knows_temp2",    "bin");

    biDirectionalityChecker(person_output_file, knows_output_file);

    munmap(person_map, person_length* sizeof(Person));
    munmap(knows_map, knows_length* sizeof(unsigned int));
    fclose(person_file);
    fclose(knows_file);

//    person_map = (Person*) mmap_file("person_temp2.bin", &size, &person_file);
//    person_length = size / sizeof(Person);
//
//    knows_map = (unsigned int*) mmap_file ("knows_temp2.bin", &size, &knows_file);
//    knows_length = size / sizeof(unsigned int);

	return 0;
}

void diffLocationChecker(const char* person_output_file, const char* knows_output_file) {

    Index *pnew_id = (Index*) malloc(sizeof(Index)*person_length);
    memset(pnew_id, 0xff, sizeof(Index)*person_length);

    Knows *knows = (Knows*) malloc(sizeof(Knows) * person_length);

    FILE *out_knows_file = fopen(knows_output_file, "wb");

    unsigned int knows_num = 0, nperson_num = 0, i, j;
    //Updating knows map..
    for(i = 0; i < person_length; ++i ){
//        if( i % 10000 == 0 )
//            printf("Processed %d Persons\n", i);

        Person *person = &person_map[i];
        unsigned int *knows_person_pointer = &knows_map[person->knows_first];
        if( pnew_id[i].new_index == 0xffffffff ) {
            pnew_id[i].old_index = i;
            pnew_id[i].new_index = nperson_num++;
        }
        knows[i].first = knows_num;

        unsigned int nknows_num = 0;

        for( j = 0; j < person->knows_n; ++j ) {
            unsigned int other_index = knows_person_pointer[j];
            Person *knowP = &person_map[other_index];
            if(knowP->location == person->location) {
                if( pnew_id[other_index].new_index == 0xffffffff ) {
                    pnew_id[other_index].old_index = other_index;
                    pnew_id[other_index].new_index = nperson_num++;
                }
                fwrite(&pnew_id[other_index].new_index, sizeof(unsigned int), 1, out_knows_file);
                nknows_num++;
                knows_num++;
            }
        }
        knows[i].n = nknows_num;
    }
    fclose(out_knows_file);

    qsort(pnew_id, person_length, sizeof(Index), index_comparator);

    FILE* out_persons_file = fopen(person_output_file, "wb");
    Person person;
    for(i = 0; i < person_length; ++i) {
        if( pnew_id[i].new_index != 0xffffffff ) {
            unsigned int index = pnew_id[i].old_index;
            person = person_map[index];
            person.knows_first = knows[index].first;
            person.knows_n = knows[index].n;
            fwrite(&person, sizeof(Person), 1, out_persons_file);
        }
    }
    fclose(out_persons_file);
    free(knows);
    free(pnew_id);
}

void biDirectionalityChecker(const char* out_persons_file_name, const char* out_knows_file_name) {

    //printf("In Bi Directionality checker..\n");

    Index *pnew_id = (Index*) malloc(sizeof(Index) * person_length);
    memset(pnew_id, 0xff, sizeof(Index) * person_length);

    Knows *knows = (Knows*) malloc(sizeof(Knows) *person_length);

    FILE* out_knows_file = fopen(out_knows_file_name, "wb");

    unsigned int knows_num = 0, nperson_num = 0, i, j, k;

    // Iterating over person P..
    for( i = 0; i < person_length; ++i ){
//        if( i % 10000 == 0 )
//            printf("Processed %d Persons\n", i);

        Person *person = &person_map[i];
        unsigned int *knows_person = &knows_map[person->knows_first];
        knows[i].first = knows_num;
        unsigned int nknows_num = 0;
        // Iterating over friends F..
        for( j = 0; j < person->knows_n; ++j ) {
            unsigned int friend_index = knows_person[j];
            Person *friend = &person_map[friend_index];
            unsigned int *knows_pointer = &knows_map[friend->knows_first];
            for(k = 0; k < friend->knows_n; ++k) {
                if(knows_pointer[k] == i){
                    break;
                }
            }
            // F is mutual to P
            if(k < friend->knows_n) {
                if( pnew_id[friend_index].new_index == 0xffffffff ) { // if id not already set
                    pnew_id[friend_index].old_index = friend_index;
                    pnew_id[friend_index].new_index = nperson_num++;
                }
                fwrite(&pnew_id[friend_index].new_index, sizeof(unsigned int), 1, out_knows_file);
                nknows_num++;
                knows_num++;
            }
        }
        knows[i].n = nknows_num;

        if( pnew_id[i].new_index == 0xffffffff && nknows_num > 0) {              // if id not already set
            pnew_id[i].old_index = i;
            pnew_id[i].new_index = nperson_num++;
        }
    }
    fclose(out_knows_file);

    qsort(pnew_id, person_length, sizeof(Index), index_comparator);

    FILE* out_persons_file = fopen(out_persons_file_name, "wb");
    Person person;
    for(i = 0; i < person_length; ++i) {
        if( pnew_id[i].new_index != 0xffffffff ) {
            unsigned int index = pnew_id[i].old_index;
            person = person_map[index];
            person.knows_first = knows[index].first;
            person.knows_n = knows[index].n;
            fwrite(&person, sizeof(Person), 1, out_persons_file);
        }
    }
    fclose(out_persons_file);
    free(knows);
    free(pnew_id);

}


